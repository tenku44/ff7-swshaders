#include "lib.hpp"

#include "crt/crt_params.hpp"
#include "crt/crt_pass.hpp"
#include "overlay/overlay.hpp"
#include "gl/gl_api.hpp"

HOOK_DEFINE_TRAMPOLINE(SwapHook){
    static EGLBoolean Callback(void* dpy, void* surface){
        rh::overlay::OnSwap(dpy, surface);
        return Orig(dpy, surface);
    }
};

HOOK_DEFINE_TRAMPOLINE(BlitHook) {
    static void Callback(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                         GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                         GLbitfield mask, GLenum filter) {
        // The game's final blit: colour only, 1:1, from (0,0), to the window (framebuffer 0).
        const bool oneToOneColourBlit = (mask & rh::gl::kColorBufferBit) != 0 &&
                                        srcX0 == 0 && srcY0 == 0 && dstX0 == 0 && dstY0 == 0 &&
                                        srcX1 == dstX1 && srcY1 == dstY1 && srcX1 > 0 && srcY1 > 0;
        if (oneToOneColourBlit && rh::crt::GetParams().mode != rh::crt::Mode::Off) {
            GLint drawFramebuffer = -1;
            glGetIntegerv(rh::gl::kDrawFramebufferBinding, &drawFramebuffer);
            if (drawFramebuffer == 0 && rh::crt::HandleBlit(srcX1, srcY1)) return;  // the pass drew the frame
        }
        Orig(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    }
};

extern "C" void exl_main(void* x0, void* x1) {
    /* Setup hooking environment. */
    exl::hook::Initialize();

    BlitHook::InstallAtFuncPtr(glBlitFramebuffer);
    SwapHook::InstallAtFuncPtr(eglSwapBuffers);
}

extern "C" NORETURN void exl_exception_entry() {
    /* Note: this is only applicable in the context of applets/sysmodules. */
    EXL_ABORT("Default exception handler called!");
}