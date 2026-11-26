#pragma once

namespace rh::crt {

enum class Mode : int {
    Off = 0,
    Scanlines,
    CrtGeom,
    CrtEasy, 
};
inline constexpr int kModeCount = 4;

struct Params {
    Mode mode = Mode::Off; //init

    // Scanlines
    float scanlineStrength = 0.45f;
    int scanlineCount = 480;          

    // CRT (crt-geom)
    int sourceWidth = 640;
    int sourceHeight = 480;
    float scanlineWeight = 0.3f;
    float dotMask = 0.3f;
    float sharper = 1.0f;
    bool curvature = false;
    float screenRadius = 2.0f;
    float screenDistance = 1.6f;
    float cornerSize = 0.03f;
    float cornerSmooth = 1000.0f;
    float tiltX = 0.0f;
    float tiltY = 0.0f;
    float overscan = 100.0f;
    float crtGamma = 2.4f;
    float monitorGamma = 2.2f;
    float saturation = 1.0f;
    float brightness = 1.0f;

    // CRT (easymode)
    int easySourceWidth = 640;
    int easySourceHeight = 480;
    float SHARPNESS_H = 0.5f;
    float SHARPNESS_V = 1.0f;
    float MASK_STRENGTH = 0.3f;
    float MASK_DOT_WIDTH = 0.8f;
    float MASK_DOT_HEIGHT = 0.8f;
    float MASK_STAGGER = 0.5f;
    float MASK_SIZE = 1.0f;
    float SCANLINE_STRENGTH = 0.3f;
    float SCANLINE_BEAM_WIDTH_MIN = 1.5f;
    float SCANLINE_BEAM_WIDTH_MAX = 1.5f;
    float SCANLINE_BRIGHT_MIN = 0.35f;
    float SCANLINE_BRIGHT_MAX = 1.0f;
    float SCANLINE_CUTOFF = 600.0f;
    float GAMMA_INPUT = 2.0f;
    float GAMMA_OUTPUT = 1.8f;
    float BRIGHT_BOOST = 1.2f;
    float DILATION = 1.0f;
};

struct Status {
    bool active = false;
    int width = 0;
    int height = 0;
    unsigned glError = 0;
    char message[192] = {};
};

inline Params g_params;
inline Status g_status;

inline Params& GetParams() { return g_params; }
inline Status& GetStatus() { return g_status; }

inline void ResetTuning() {
    const Mode mode = g_params.mode;
    g_params = Params{};
    g_params.mode = mode;
}

}  // namespace rh::crt
