
#include "overlay/overlay.hpp"

#include <cstddef>
#include <cstdint>

#include "gl/gl_api.hpp"
#include "gl/gl_state_guard.hpp"
#include "imgui.h"
#if __has_include("backends/imgui_impl_opengl3.h")
#include "backends/imgui_impl_opengl3.h"
#else
#include "imgui_impl_opengl3.h"
#endif
#include "overlay/arena_allocator.hpp"
#include "overlay/input.hpp"
#include "overlay/menu.hpp"
#include "log.hpp"

namespace rh::overlay {
namespace {

constexpr const char* kGlslVersion = "#version 300 es";
enum class State : uint8_t { Uninitialised, Initialising, Ready, Failed };

State g_state = State::Uninitialised;
bool g_visible = true;
double g_lastTime = 0.0;
int g_frameCount = 0;

double NowSeconds() {
    uint64_t ticks;
    asm volatile("mrs %0, cntpct_el0" : "=r"(ticks));
    return static_cast<double>(ticks) / 19200000.0;
}

void* ArenaAlloc(size_t size, void*) {
    void* p = arena::Alloc(size);
    if (!p) Log("[rh] overlay arena out of memory (%u bytes requested)", static_cast<unsigned>(size));
    return p;
}

void ArenaFree(void* ptr, void*) { arena::Free(ptr); }

bool Init(int width, int height) {
    ImGui::SetAllocatorFunctions(ArenaAlloc, ArenaFree, nullptr);  // must precede CreateContext
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    io.BackendPlatformName = "switch (exlaunch)";

    ImGui::StyleColorsDark();
    ImGui::GetStyle().FrameRounding = 3.0f;
    ImGui::GetStyle().GrabRounding = 3.0f;

    input::EnableImGuiNavigation();

    if (!ImGui_ImplOpenGL3_Init(kGlslVersion)) {
        ImGui::DestroyContext();
        return false;
    }
    return true;
}

}  // namespace

void OnSwap(void* dpy, void* surface) {
    if (g_state == State::Failed || g_state == State::Initialising) return;
    int width = 0, height = 0;
    eglQuerySurface(dpy, surface, rh::gl::kEglWidth, &width);
    eglQuerySurface(dpy, surface, rh::gl::kEglHeight, &height);
    if (width <= 0 || height <= 0) return;

    if (g_state == State::Uninitialised) {
        g_state = State::Initialising;
        g_state = Init(width, height) ? State::Ready : State::Failed;
        Log("[rh] overlay %s (%dx%d window, controller input %s, touch %s)",
            g_state == State::Ready ? "ready" : "FAILED to initialise", width, height,
            input::Available() ? "available" : "not found", input::TouchAvailable() ? "available" : "not found");
        if (g_state != State::Ready) return;
        const unsigned pads = input::FoundFunctions();
        Log("[rh] pad functions found: handheld=%d full-key=%d joy-dual=%d", (pads & input::kFoundHandheld) != 0,
            (pads & input::kFoundFullKey) != 0, (pads & input::kFoundJoyDual) != 0);
        g_lastTime = NowSeconds();
    }

    if (input::Poll(g_visible, width, height)) {
        g_visible = !g_visible;
        Log("[rh] menu %s", g_visible ? "shown" : "hidden");
    }
    if (!g_visible) {
        g_lastTime = NowSeconds();
        return;
    }

    GlStateGuard guard;  // restores the game's framebuffer, colour mask and the rest on return

    glBindFramebuffer(rh::gl::kDrawFramebuffer, 0);
    glColorMask(1, 1, 1, 1);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    const double now = NowSeconds();
    float deltaTime = static_cast<float>(now - g_lastTime);
    g_lastTime = now;
    if (!(deltaTime > 0.0001f)) deltaTime = 1.0f / 60.0f;  // first frame, or no clock
    if (deltaTime > 0.25f) deltaTime = 0.25f;               // after a long stall
    io.DeltaTime = deltaTime;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    if (!DrawMenu(input::Available(), input::TouchAvailable())) g_visible = false;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // The first few frames report what ImGui built and whether GL complained, which separates
    // 'nothing was drawn' from 'drawn but not visible' when the menu does not show up.
    if (g_frameCount < 5) {
        const ImDrawData* drawData = ImGui::GetDrawData();
        Log("[rh] overlay frame %d: %d vertices in %d lists, window %dx%d, glGetError=0x%x", g_frameCount,
            drawData->TotalVtxCount, drawData->CmdListsCount, width, height, glGetError());
    }
    g_frameCount++;
}

}  // namespace rh::overlay
