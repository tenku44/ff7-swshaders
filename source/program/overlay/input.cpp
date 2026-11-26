#include "overlay/input.hpp"

#include <cfloat>

#include "imgui.h"

#if defined(__aarch64__)
#include "lib.hpp"
#include <lib/reloc/reloc.hpp>
#include <lib/util/sys/mem_layout.hpp>
#endif

namespace rh::input {
namespace {

using NpadId = uint32_t;
constexpr NpadId kNpadNo1 = 0x0;
constexpr NpadId kNpadHandheld = 0x20;
constexpr float kStickDeadzone = 0.25f;

struct AnalogStick {
    int32_t x, y;
};

struct NpadState {
    int64_t samplingNumber;
    uint64_t buttons;
    AnalogStick analogStickL;
    AnalogStick analogStickR;
    uint32_t attributes;
    uint32_t reserved;
    uint64_t spare[4];
};
static_assert(sizeof(NpadState) == 0x48, "unexpected NpadState size");

using ReadFn = void (*)(NpadState* out, const NpadId& id);

struct TouchPoint {
    uint64_t deltaTime;
    uint32_t attributes;
    uint32_t fingerId;
    uint32_t x, y;
    uint32_t diameterX, diameterY;
    uint32_t rotationAngle;
    uint32_t reserved;
};
struct TouchState {
    int64_t samplingNumber;
    int32_t count;
    uint32_t reserved;
    TouchPoint touches[16];
};
using TouchInitFn = void (*)();
using TouchReadFn = void (*)(TouchState* out);
constexpr float kTouchScreenWidth = 1280.0f;
constexpr float kTouchScreenHeight = 720.0f;

struct Functions {
    ReadFn handheld = nullptr;
    ReadFn fullKey = nullptr;
    ReadFn joyDual = nullptr;
    TouchInitFn touchInit = nullptr;
    TouchReadFn touchRead = nullptr;
    bool resolved = false;
};
Functions g_fn;

void ResolveFunctions() {
    struct Wanted {
        const char* symbol;
        void** target;
    };
    const Wanted wanted[] = {
        {"_ZN2nn3hid12GetNpadStateEPNS0_17NpadHandheldStateERKj", reinterpret_cast<void**>(&g_fn.handheld)},
        {"_ZN2nn3hid12GetNpadStateEPNS0_16NpadFullKeyStateERKj", reinterpret_cast<void**>(&g_fn.fullKey)},
        {"_ZN2nn3hid12GetNpadStateEPNS0_16NpadJoyDualStateERKj", reinterpret_cast<void**>(&g_fn.joyDual)},
        {"_ZN2nn3hid21InitializeTouchScreenEv", reinterpret_cast<void**>(&g_fn.touchInit)},
        {"_ZN2nn3hid19GetTouchScreenStateILm4EEEvPNS0_16TouchScreenStateIXT_EEE", reinterpret_cast<void**>(&g_fn.touchRead)},
    };
    for (int i = static_cast<int>(exl::util::ModuleIndex::Start); i < static_cast<int>(exl::util::ModuleIndex::End); i++) {
        const auto index = static_cast<exl::util::ModuleIndex>(i);
        if (!exl::util::HasModule(index)) continue;
        const uintptr_t base = exl::util::GetModuleInfo(index).m_Total.m_Start;
        for (const Wanted& w : wanted) {
            if (*w.target) continue;
            if (auto* sym = exl::reloc::GetSymbol(index, w.symbol)) *w.target = reinterpret_cast<void*>(base + sym->st_value);
        }
    }
}

const Functions& Fn() {
    if (!g_fn.resolved) {
        g_fn.resolved = true;
        ResolveFunctions();
        if (g_fn.touchInit && g_fn.touchRead) g_fn.touchInit();
    }
    return g_fn;
}

struct Pad {
    uint64_t buttons = 0;
    float leftX = 0.0f, leftY = 0.0f;
    float rightX = 0.0f, rightY = 0.0f;
};

float Abs(float v) { return v < 0.0f ? -v : v; }

float Normalise(int32_t raw) {
    float v = static_cast<float>(raw) / 32767.0f;
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return v;
}

void KeepLarger(float& current, float candidate) {
    if (Abs(candidate) > Abs(current)) current = candidate;
}

template <class State>
void Merge(Pad& pad, const State& state) {
    pad.buttons |= state.buttons;
    KeepLarger(pad.leftX, Normalise(state.analogStickL.x));
    KeepLarger(pad.leftY, Normalise(state.analogStickL.y));
    KeepLarger(pad.rightX, Normalise(state.analogStickR.x));
    KeepLarger(pad.rightY, Normalise(state.analogStickR.y));
}

bool ReadPads(Pad& pad) {
    const Functions& fn = Fn();
    bool any = false;
    const struct { ReadFn read; NpadId id; } sources[] = {
        {fn.handheld, kNpadHandheld}, {fn.fullKey, kNpadNo1}, {fn.joyDual, kNpadNo1}};
    for (const auto& source : sources) {
        if (!source.read) continue;
        NpadState state{};
        source.read(&state, source.id);
        Merge(pad, state);
        any = true;
    }
    return any;
}

void Key(ImGuiIO& io, ImGuiKey key, const Pad& pad, uint64_t mask) {
    io.AddKeyEvent(key, (pad.buttons & mask) != 0);
}

void Axis(ImGuiIO& io, ImGuiKey negative, ImGuiKey positive, float value) {
    const float neg = value < 0.0f ? -value : 0.0f;
    const float pos = value > 0.0f ? value : 0.0f;
    io.AddKeyAnalogEvent(negative, neg > kStickDeadzone, neg);
    io.AddKeyAnalogEvent(positive, pos > kStickDeadzone, pos);
}

void FeedImGui(const Pad& pad) {
    ImGuiIO& io = ImGui::GetIO();
    Key(io, ImGuiKey_GamepadFaceDown, pad, kButtonA);    // confirm
    Key(io, ImGuiKey_GamepadFaceRight, pad, kButtonB);   // cancel
    Key(io, ImGuiKey_GamepadFaceUp, pad, kButtonX);
    Key(io, ImGuiKey_GamepadFaceLeft, pad, kButtonY);
    Key(io, ImGuiKey_GamepadDpadLeft, pad, kButtonLeft);
    Key(io, ImGuiKey_GamepadDpadRight, pad, kButtonRight);
    Key(io, ImGuiKey_GamepadDpadUp, pad, kButtonUp);
    Key(io, ImGuiKey_GamepadDpadDown, pad, kButtonDown);
    Key(io, ImGuiKey_GamepadL1, pad, kButtonL);
    Key(io, ImGuiKey_GamepadR1, pad, kButtonR);
    Key(io, ImGuiKey_GamepadL3, pad, kButtonStickL);
    Key(io, ImGuiKey_GamepadR3, pad, kButtonStickR);
    Key(io, ImGuiKey_GamepadStart, pad, kButtonPlus);
    Key(io, ImGuiKey_GamepadBack, pad, kButtonMinus);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, (pad.buttons & kButtonZL) != 0, (pad.buttons & kButtonZL) ? 1.0f : 0.0f);
    io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, (pad.buttons & kButtonZR) != 0, (pad.buttons & kButtonZR) ? 1.0f : 0.0f);
    Axis(io, ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight, pad.leftX);
    Axis(io, ImGuiKey_GamepadLStickDown, ImGuiKey_GamepadLStickUp, pad.leftY);   // stick up is positive
    Axis(io, ImGuiKey_GamepadRStickLeft, ImGuiKey_GamepadRStickRight, pad.rightX);
    Axis(io, ImGuiKey_GamepadRStickDown, ImGuiKey_GamepadRStickUp, pad.rightY);
}

bool g_comboWasDown = false;

bool ReadTouch(float& x, float& y) {
    const Functions& fn = Fn();
    if (!fn.touchRead) return false;
    TouchState state{};
    fn.touchRead(&state);
    if (state.count <= 0) return false;
    x = static_cast<float>(state.touches[0].x);
    y = static_cast<float>(state.touches[0].y);
    return true;
}

bool g_touchWasDown = false;

void FeedTouch(bool down, float x, float y, int width, int height) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
    if (down) {
        io.AddMousePosEvent(x * static_cast<float>(width) / kTouchScreenWidth,
                            y * static_cast<float>(height) / kTouchScreenHeight);
        io.AddMouseButtonEvent(0, true);
    } else if (g_touchWasDown) {
        io.AddMouseButtonEvent(0, false);
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }
    g_touchWasDown = down;
}

constexpr float kCornerWidth = 160.0f;
constexpr float kCornerHeight = 90.0f;
constexpr double kCornerHoldSeconds = 1.0;

double NowSeconds() {
#if defined(__aarch64__)
    uint64_t ticks;
    asm volatile("mrs %0, cntpct_el0" : "=r"(ticks));
    return static_cast<double>(ticks) / 19200000.0;
#else
    return 0.0;
#endif
}

double g_cornerSince = -1.0;

bool CornerHeldLongEnough(bool down, float x, float y) {
    if (!down || x >= kCornerWidth || y >= kCornerHeight) {
        g_cornerSince = -1.0;
        return false;
    }
    const double now = NowSeconds();
    if (g_cornerSince < 0.0) g_cornerSince = now;
    if (now - g_cornerSince < kCornerHoldSeconds) return false;
    g_cornerSince = -1.0;   // fire once per hold
    return true;
}

}  // namespace

bool Available() { return FoundFunctions() != 0; }

bool TouchAvailable() { return Fn().touchRead != nullptr; }

unsigned FoundFunctions() {
    const Functions& fn = Fn();
    return (fn.handheld ? kFoundHandheld : 0u) | (fn.fullKey ? kFoundFullKey : 0u) | (fn.joyDual ? kFoundJoyDual : 0u);
}

void EnableImGuiNavigation() {
    if (!Available()) return;
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
}

bool Poll(bool feedImGui, int width, int height) {
    Pad pad;
    const bool haveInput = ReadPads(pad);

    const bool comboDown = haveInput && (pad.buttons & kToggleCombo) == kToggleCombo;
    bool toggle = comboDown && !g_comboWasDown;
    g_comboWasDown = comboDown;

    float touchX = 0.0f, touchY = 0.0f;
    const bool touchDown = ReadTouch(touchX, touchY);
    if (feedImGui) {
        FeedImGui(pad);   // an empty pad releases anything that was held
        FeedTouch(touchDown, touchX, touchY, width, height);
        g_cornerSince = -1.0;
    } else {
        g_touchWasDown = false;
        if (CornerHeldLongEnough(touchDown, touchX, touchY)) toggle = true;
    }
    return toggle;
}

}  // namespace rh::input