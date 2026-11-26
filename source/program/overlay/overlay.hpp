#pragma once

namespace rh::overlay {

// Call once per presented frame, from the eglSwapBuffers hook, immediately before the original swap.
void OnSwap(void* dpy, void* surface);

}  // namespace rh::overlay
