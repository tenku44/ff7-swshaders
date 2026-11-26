#include "crt/crt_pass.hpp"

#include <cstdio>
#include <cstring>

#include "crt/crt_params.hpp"
#include "gl/gl_api.hpp"
#include "gl/gl_state_guard.hpp"
#include "crt/crt_shaders.hpp"
// #include "log.hpp"

namespace rh::crt {
namespace {

using namespace rh::gl;

constexpr int kMaxUniforms = 18;

struct Program {
    GLuint id = 0;
    bool failed = false;
    GLint loc[kMaxUniforms] = {};
};

Program g_programs[kModeCount - 1];

constexpr const char* kScanlineUniforms[] = {"Strength", "Lines"};
enum ScanlineUniform { kStrength, kLines, kScanlineUniformCount };

constexpr const char* kGeomUniforms[] = {"CRTgamma", "monitorgamma", "d",       "CURVATURE", "R",
                                         "cornersize", "cornersmooth", "overscan", "DOTMASK",
                                         "scanline_weight", "LUM", "SATURATION", "SHARPER",
                                         "x_tilt", "y_tilt", "SourceSize"};
enum GeomUniform {
    kCrtGamma, kMonitorGamma, kDistance, kCurvature, kRadius, kCornerSize, kCornerSmooth, kOverscan,
    kDotMask, kScanlineWeight, kLum, kSaturation, kSharper, kTiltX, kTiltY, kSourceSize,
    kGeomUniformCount
};
constexpr const char* kEzUniforms[] = {"SHARPNESS_H", "SHARPNESS_V", "MASK_STRENGTH", "MASK_DOT_WIDTH", "MASK_DOT_HEIGHT",
                                 "MASK_STAGGER", "MASK_SIZE", "SCANLINE_STRENGTH", "SCANLINE_BEAM_WIDTH_MAX",
                                 "SCANLINE_BEAM_WIDTH_MIN", "SourceSize", "SCANLINE_BRIGHT_MIN",
                                 "SCANLINE_BRIGHT_MAX", "SCANLINE_CUTOFF", "GAMMA_INPUT", "GAMMA_OUTPUT", "BRIGHT_BOOST",
                                 "DILATION"};
enum EzUniform {
    kEzSharpnessH, kEzSharpnessV, kEzMaskStrength, kEzMaskDotWidth, kEzMaskDotHeight,
    kEzMaskStagger, kEzMaskSize, kEzScanlineStrength, kScanlineBeamWidthMax, kScanlineBeamWidthMin, kEzSourceSize,
    kScanlineBrightMin, kScanlineBrightMax, kScanlineCutoff, kGammaInput, kGammaOutput, kBrightBoost, kDilation,
    kEzUniformCount
};
static_assert(kGeomUniformCount == sizeof(kGeomUniforms) / sizeof(kGeomUniforms[0]), "uniform name list out of sync");
static_assert(kScanlineUniformCount == sizeof(kScanlineUniforms) / sizeof(kScanlineUniforms[0]), "uniform name list out of sync");
static_assert(kEzUniformCount == sizeof(kEzUniforms) / sizeof(kEzUniforms[0]), "uniform name list out of sync");

void SetError(const char* what, const char* detail) {
    Status& status = GetStatus();
    std::snprintf(status.message, sizeof(status.message), "%s: %s", what, detail ? detail : "");
    for (char* p = status.message; *p; ++p)
        if (*p == '\n' || *p == '\r') *p = ' ';
    // Log("[rh] shader pass: %s", status.message);
}

GLuint CompileShader(GLenum type, const char* source, const char* label) {
    const GLuint shader = glCreateShader(type);
    const GLchar* text = source;
    const GLint length = static_cast<GLint>(std::strlen(source));
    glShaderSource(shader, 1, &text, &length);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, kCompileStatus, &ok);
    if (ok) return shader;

    char info[128] = {};
    GLsizei written = 0;
    glGetShaderInfoLog(shader, sizeof(info) - 1, &written, info);
    info[(written > 0 && written < static_cast<GLsizei>(sizeof(info))) ? written : 0] = '\0';
    SetError(label, info);
    glDeleteShader(shader);
    return 0;
}

bool BuildProgram(Program& program, const char* vertexSource, const char* fragmentSource,
                  const char* label, const char* const* uniformNames, int uniformCount) {
    const GLuint vs = CompileShader(kVertexShader, vertexSource, label);
    const GLuint fs = vs ? CompileShader(kFragmentShader, fragmentSource, label) : 0;
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    const GLuint id = glCreateProgram();
    glAttachShader(id, vs);
    glAttachShader(id, fs);
    glLinkProgram(id);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(id, kLinkStatus, &ok);
    if (!ok) {
        char info[128] = {};
        GLsizei written = 0;
        glGetProgramInfoLog(id, sizeof(info) - 1, &written, info);
        info[(written > 0 && written < static_cast<GLsizei>(sizeof(info))) ? written : 0] = '\0';
        SetError(label, info);
        return false;
    }

    program.id = id;
    for (int i = 0; i < uniformCount; i++) program.loc[i] = glGetUniformLocation(id, uniformNames[i]);
    return true;
}

Program* EnsureProgram(Mode mode) {
    Program& program = g_programs[static_cast<int>(mode) - 1];
    if (program.id) return &program;
    if (program.failed) return nullptr;

    bool ok = false;
    switch (mode) {
        case Mode::Scanlines:
            ok = BuildProgram(program, shaders::kFullscreenTriangleVs, shaders::kScanlinesFs, "scanlines",
                              kScanlineUniforms, kScanlineUniformCount);
            break;
        case Mode::CrtGeom:
            ok = BuildProgram(program, shaders::kCrtGeomVs, shaders::kCrtGeomFs, "crt-geom",
                              kGeomUniforms, kGeomUniformCount);
            break;
        case Mode::CrtEasy:
            ok = BuildProgram(program, shaders::kFullscreenTriangleVs, shaders::kCrtEzFs, "crt-easy",
                              kEzUniforms, kEzUniformCount);
            break;
        default:
            break;
    }
    if (!ok) program.failed = true;
    return ok ? &program : nullptr;
}

GLuint g_texture = 0;
GLuint g_vertexArray = 0;
int g_textureWidth = 0;
int g_textureHeight = 0;

void EnsureResources(int width, int height) {
    if (!g_vertexArray) glGenVertexArrays(1, &g_vertexArray);
    if (g_texture && g_textureWidth == width && g_textureHeight == height) return;

    if (g_texture) glDeleteTextures(1, &g_texture);
    glGenTextures(1, &g_texture);
    glActiveTexture(kTexture0);
    glBindTexture(kTexture2D, g_texture);
    glTexStorage2D(kTexture2D, 1, kRgba8, width, height);
    g_textureWidth = width;
    g_textureHeight = height;
}

void Set1f(GLint location, float value) { if (location >= 0) glUniform1f(location, value); }
void Set2f(GLint location, float x, float y) { if (location >= 0) glUniform2f(location, x, y); }

void SetUniforms(Mode mode, const Program& program, const Params& p) {
    const GLint* u = program.loc;
    if (mode == Mode::Scanlines) {
        Set1f(u[kStrength], p.scanlineStrength);
        Set1f(u[kLines], static_cast<float>(p.scanlineCount));
    } else if (mode == Mode::CrtGeom) {
        Set1f(u[kCrtGamma], p.crtGamma);
        Set1f(u[kMonitorGamma], p.monitorGamma);
        Set1f(u[kDistance], p.screenDistance);
        Set1f(u[kCurvature], p.curvature ? 1.0f : 0.0f);
        Set1f(u[kRadius], p.screenRadius);
        Set1f(u[kCornerSize], p.cornerSize);
        Set1f(u[kCornerSmooth], p.cornerSmooth);
        Set1f(u[kOverscan], p.overscan);
        Set1f(u[kDotMask], p.dotMask);
        Set1f(u[kScanlineWeight], p.scanlineWeight);
        Set1f(u[kLum], p.brightness);
        Set1f(u[kSaturation], p.saturation);
        Set1f(u[kSharper], p.sharper);
        Set1f(u[kTiltX], p.tiltX);
        Set1f(u[kTiltY], p.tiltY);
        Set2f(u[kSourceSize], static_cast<float>(p.sourceWidth), static_cast<float>(p.sourceHeight));
    } else if (mode == Mode::CrtEasy) {
        Set1f(u[kEzSharpnessH], p.SHARPNESS_H);
        Set1f(u[kEzSharpnessV], p.SHARPNESS_V);
        Set1f(u[kEzMaskStrength], p.MASK_STRENGTH);
        Set1f(u[kEzMaskDotWidth], p.MASK_DOT_WIDTH);
        Set1f(u[kEzMaskDotHeight], p.MASK_DOT_HEIGHT);
        Set1f(u[kEzMaskStagger], p.MASK_STAGGER);
        Set1f(u[kEzMaskSize], p.MASK_SIZE);
        Set1f(u[kEzScanlineStrength], p.SCANLINE_STRENGTH);
        Set1f(u[kScanlineBeamWidthMax], p.SCANLINE_BEAM_WIDTH_MAX);
        Set1f(u[kScanlineBeamWidthMin], p.SCANLINE_BEAM_WIDTH_MIN);
        Set2f(u[kEzSourceSize], static_cast<float>(p.easySourceWidth), static_cast<float>(p.easySourceHeight));
        Set1f(u[kScanlineBrightMin], p.SCANLINE_BRIGHT_MIN);
        Set1f(u[kScanlineBrightMax], p.SCANLINE_BRIGHT_MAX);
        Set1f(u[kScanlineCutoff], p.SCANLINE_CUTOFF);
        Set1f(u[kGammaInput], p.GAMMA_INPUT);
        Set1f(u[kGammaOutput], p.GAMMA_OUTPUT);
        Set1f(u[kBrightBoost], p.BRIGHT_BOOST);
        Set1f(u[kDilation], p.DILATION);
    }
}

Mode g_lastMode = Mode::Off;

}

bool HandleBlit(int width, int height) {
    const Params& params = GetParams();
    Status& status = GetStatus();

    if (params.mode == Mode::Off) {
        status.active = false;
        return false;
    }

    // Selecting a mode again gives a failed build another chance and clears the old message.
    if (params.mode != g_lastMode) {
        g_lastMode = params.mode;
        g_programs[static_cast<int>(params.mode) - 1].failed = false;
        status.message[0] = '\0';
    }

    GlStateGuard guard;  // everything below is undone when this function returns

    Program* program = EnsureProgram(params.mode);
    if (!program) {
        status.active = false;
        return false;
    }
    EnsureResources(width, height);

    glActiveTexture(kTexture0);
    glBindTexture(kTexture2D, g_texture);
    const GLint filter = (params.mode == Mode::CrtGeom) ? kLinear : kNearest;
    glTexParameteri(kTexture2D, kTextureMinFilter, filter);
    glTexParameteri(kTexture2D, kTextureMagFilter, filter);
    glTexParameteri(kTexture2D, kTextureWrapS, kClampToEdge);
    glTexParameteri(kTexture2D, kTextureWrapT, kClampToEdge);
    glCopyTexSubImage2D(kTexture2D, 0, 0, 0, 0, 0, width, height);

    glBindFramebuffer(kDrawFramebuffer, 0);
    glViewport(0, 0, width, height);
    GlStateGuard::DisableFullscreenBlockers();
    glUseProgram(program->id);
    SetUniforms(params.mode, *program, params);
    glBindVertexArray(g_vertexArray);
    glDrawArrays(kTriangles, 0, 3);

    const GLenum error = glGetError();
    if (error != 0) status.glError = error;
    status.active = true;
    status.width = width;
    status.height = height;
    return true;
}

}
