// gl_state_guard.hpp: saves the OpenGL state and restores it on scope exit.
// Saved: current program, active texture unit and the 2D texture on unit 0, vertex array,
// draw/read framebuffers, viewport, colour write mask, and the scissor/blend/depth/cull/stencil
// switches.

#pragma once

#include "gl/gl_api.hpp"

namespace rh {

class GlStateGuard {
public:
    GlStateGuard() {
        using namespace rh::gl;
        glGetIntegerv(kCurrentProgram, &program_);
        glGetIntegerv(kActiveTexture, &activeTexture_);
        glGetIntegerv(kDrawFramebufferBinding, &drawFramebuffer_);
        glGetIntegerv(kReadFramebufferBinding, &readFramebuffer_);
        glGetIntegerv(kVertexArrayBinding, &vertexArray_);
        glGetIntegerv(kViewport, viewport_);
        glGetBooleanv(kColorWritemask, colorMask_);
        for (int i = 0; i < kCapCount; i++) capEnabled_[i] = glIsEnabled(kCaps[i]);
        glActiveTexture(kTexture0);
        glGetIntegerv(kTextureBinding2D, &texture0_);
    }

    ~GlStateGuard() {
        using namespace rh::gl;
        glBindVertexArray(static_cast<GLuint>(vertexArray_));
        glUseProgram(static_cast<GLuint>(program_));
        glActiveTexture(kTexture0);
        glBindTexture(kTexture2D, static_cast<GLuint>(texture0_));
        glActiveTexture(static_cast<GLenum>(activeTexture_));
        glBindFramebuffer(kDrawFramebuffer, static_cast<GLuint>(drawFramebuffer_));
        glBindFramebuffer(kReadFramebuffer, static_cast<GLuint>(readFramebuffer_));
        glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
        glColorMask(colorMask_[0], colorMask_[1], colorMask_[2], colorMask_[3]);
        for (int i = 0; i < kCapCount; i++) {
            if (capEnabled_[i]) glEnable(kCaps[i]); else glDisable(kCaps[i]);
        }
    }

    GlStateGuard(const GlStateGuard&) = delete;
    GlStateGuard& operator=(const GlStateGuard&) = delete;

    // Turns off everything that could stop a full-screen draw from reaching every pixel.
    static void DisableFullscreenBlockers() {
        using namespace rh::gl;
        for (int i = 0; i < kCapCount; i++) glDisable(kCaps[i]);
        glColorMask(1, 1, 1, 1);
    }

private:
    static constexpr int kCapCount = 5;
    static constexpr GLenum kCaps[kCapCount] = {rh::gl::kScissorTest, rh::gl::kBlend, rh::gl::kDepthTest,
                                                rh::gl::kCullFace, rh::gl::kStencilTest};

    GLint program_ = 0;
    GLint activeTexture_ = rh::gl::kTexture0;
    GLint texture0_ = 0;
    GLint vertexArray_ = 0;
    GLint drawFramebuffer_ = 0;
    GLint readFramebuffer_ = 0;
    GLint viewport_[4] = {0, 0, 0, 0};
    GLboolean colorMask_[4] = {1, 1, 1, 1};
    GLboolean capEnabled_[kCapCount] = {};
};

}  // namespace rh
