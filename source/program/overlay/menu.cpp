#include "overlay/menu.hpp"

#include "crt/crt_params.hpp"
#include "imgui.h"
#include "overlay/arena_allocator.hpp"
#include "overlay/input.hpp"

namespace rh::overlay {
namespace {

const char* const kModeNames[crt::kModeCount] = {
    "Off (game output)", "Scanlines", "CRT (crt-geom)", "CRT (easymode)"};

const ImVec4 kErrorColour(1.0f, 0.45f, 0.45f, 1.0f);

void StatusLine() {
    const crt::Params& params = crt::GetParams();
    const crt::Status& status = crt::GetStatus();

    if (params.mode == crt::Mode::Off)
        ImGui::TextDisabled("No shaders running.");
    else if (status.active)
        ImGui::Text("Active at %d x %d", status.width, status.height);
    else
        ImGui::TextDisabled("Not running (waiting for the next frame, or the shader failed).");

    if (status.message[0]) ImGui::TextColored(kErrorColour, "%s", status.message);
    if (status.glError) ImGui::Text("Last GL error: 0x%x", status.glError);
}

void ScanlineControls(crt::Params& p) {
    ImGui::SliderFloat("Strength", &p.scanlineStrength, 0.0f, 1.0f);
    ImGui::SliderInt("Lines", &p.scanlineCount, 120, 720);
}

void SourcePresets(int& width, int& height) {
    if (ImGui::Button("640x480")) { width = 640; height = 480; }
    ImGui::SameLine();
    if (ImGui::Button("640x240")) { width = 640; height = 240; }
    ImGui::SameLine();
    if (ImGui::Button("320x240")) { width = 320; height = 240; }
    ImGui::SameLine();
    if (ImGui::Button("1280x720")) { width = 1280; height = 720; }
}

void CrtControls(crt::Params& p) {
    ImGui::SeparatorText("Virtual resolution");
    ImGui::SliderInt("Width", &p.sourceWidth, 160, 1280);
    ImGui::SliderInt("Height", &p.sourceHeight, 120, 720);
    SourcePresets(p.sourceWidth, p.sourceHeight);

    ImGui::SeparatorText("Scanlines and mask");
    ImGui::SliderFloat("Scanline weight", &p.scanlineWeight, 0.1f, 0.5f);
    ImGui::SliderFloat("Dot mask", &p.dotMask, 0.0f, 0.6f);
    ImGui::SliderFloat("Sharpness", &p.sharper, 1.0f, 3.0f);

    ImGui::SeparatorText("Screen shape");
    ImGui::Checkbox("Curvature", &p.curvature);
    ImGui::SliderFloat("Radius", &p.screenRadius, 0.5f, 10.0f);
    ImGui::SliderFloat("Distance", &p.screenDistance, 0.1f, 3.0f);
    ImGui::SliderFloat("Corner size", &p.cornerSize, 0.001f, 0.2f);
    ImGui::SliderFloat("Corner smoothness", &p.cornerSmooth, 80.0f, 2000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Tilt X", &p.tiltX, -0.5f, 0.5f);
    ImGui::SliderFloat("Tilt Y", &p.tiltY, -0.5f, 0.5f);
    ImGui::SliderFloat("Overscan (%)", &p.overscan, 80.0f, 120.0f, "%.0f");

    ImGui::SeparatorText("Colour");
    ImGui::SliderFloat("CRT gamma", &p.crtGamma, 1.0f, 4.0f);
    ImGui::SliderFloat("Monitor gamma", &p.monitorGamma, 1.0f, 4.0f);
    ImGui::SliderFloat("Saturation", &p.saturation, 0.0f, 2.0f);
    ImGui::SliderFloat("Brightness", &p.brightness, -0.5f, 1.0f);
}

void CrtEzControls(crt::Params& p) {
    ImGui::SeparatorText("Virtual resolution");
    ImGui::SliderInt("Width", &p.easySourceWidth, 160, 1280);
    ImGui::SliderInt("Height", &p.easySourceHeight, 120, 720);
    SourcePresets(p.easySourceWidth, p.easySourceHeight);

    ImGui::SeparatorText("Scanlines and mask");
    ImGui::SliderFloat("Horizontal sharpness", &p.SHARPNESS_H, 0.1f, 3.0f);
    ImGui::SliderFloat("Vertical sharpness", &p.SHARPNESS_V, 0.1f, 3.0f);
    ImGui::SliderFloat("Mask strength", &p.MASK_STRENGTH, 0.0f, 1.0f);
    ImGui::SliderFloat("Mask dot width", &p.MASK_DOT_WIDTH, 0.1f, 2.0f);
    ImGui::SliderFloat("Mask dot height", &p.MASK_DOT_HEIGHT, 0.1f, 2.0f);
    ImGui::SliderFloat("Mask stagger", &p.MASK_STAGGER, 0.0f, 1.0f);
    ImGui::SliderFloat("Mask size", &p.MASK_SIZE, 0.5f, 2.0f);
    ImGui::SliderFloat("Scanline strength", &p.SCANLINE_STRENGTH, 0.0f, 1.0f);
    ImGui::SliderFloat("Scanline beam width min", &p.SCANLINE_BEAM_WIDTH_MIN, 0.1f, 3.0f);
    ImGui::SliderFloat("Scanline beam width max", &p.SCANLINE_BEAM_WIDTH_MAX, 0.1f, 3.0f);
    ImGui::SliderFloat("Scanline brightness min", &p.SCANLINE_BRIGHT_MIN, 0.0f, 1.0f);
    ImGui::SliderFloat("Scanline brightness max", &p.SCANLINE_BRIGHT_MAX, 0.0f, 1.0f);
    ImGui::SliderFloat("Scanline cutoff", &p.SCANLINE_CUTOFF, 100.0f, 1000.0f);
    ImGui::SliderFloat("Input gamma", &p.GAMMA_INPUT, 0.1f, 4.0f);
    ImGui::SliderFloat("Output gamma", &p.GAMMA_OUTPUT  , 0.1f, 4.0f);
    ImGui::SliderFloat("Brightness boost", &p.BRIGHT_BOOST, 0.1f, 3.0f);
    ImGui::SliderFloat("Dilation", &p.DILATION, 0.1f, 3.0f);
}

void ShaderTab() {
    crt::Params& params = crt::GetParams();

    int mode = static_cast<int>(params.mode);
    if (ImGui::Combo("Mode", &mode, kModeNames, crt::kModeCount)) params.mode = static_cast<crt::Mode>(mode);
    StatusLine();
    ImGui::Separator();

    switch (params.mode) {
        case crt::Mode::Off:
            ImGui::TextWrapped("Pick a shader.");
            break;
        case crt::Mode::Scanlines:
            ScanlineControls(params);
            break;
        case crt::Mode::CrtGeom:
            CrtControls(params);
            break;
        case crt::Mode::CrtEasy:
            CrtEzControls(params);
            break;
    }

    ImGui::Separator();
    if (ImGui::Button("Reset parameters")) crt::ResetTuning();
}

void InfoTab(bool controller, bool touch) {
    const ImGuiIO& io = ImGui::GetIO();
    const int fps = static_cast<int>(io.Framerate + 0.5f);
    const int tenthsOfMs = io.Framerate > 0.0f ? static_cast<int>(10000.0f / io.Framerate) : 0;

    ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
    ImGui::Text("%d FPS (%d.%d ms)", fps, tenthsOfMs / 10, tenthsOfMs % 10);
    ImGui::Text("Window %d x %d", static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
    ImGui::Text("Overlay memory %d KB used, %d KB peak",
                static_cast<int>(arena::g_used / 1024u), static_cast<int>(arena::g_peak / 1024u));
    ImGui::Separator();
    ImGui::Text("Controller input: %s", controller ? "available" : "not found");
    if (controller) ImGui::Text("Show/hide menu: %s", input::kToggleComboName);
    ImGui::Text("Touch screen: %s", touch ? "available" : "not found");
    if (touch) ImGui::TextWrapped("While the menu is hidden, hold a finger in the top-left corner for one second to show it.");
}

}  // namespace

bool DrawMenu(bool controller, bool touch) {
    bool keepOpen = true;

    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(440.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("FF7 Shaders")) {
        if (ImGui::BeginTabBar("tabs")) {
            if (ImGui::BeginTabItem("Shaders")) {
                ShaderTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Info")) {
                InfoTab(controller, touch);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (controller || touch) {   // either can bring the menu back
            ImGui::Separator();
            if (ImGui::Button("Hide menu")) keepOpen = false;
        }
    }
    ImGui::End();
    return keepOpen;
}

}  // namespace rh::overlay
