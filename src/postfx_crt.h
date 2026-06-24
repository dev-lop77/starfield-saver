#pragma once
// CRT post-process. Takes the low-res scene texture and draws it to the window
// through a GLSL shader that adds the classic CRT look: barrel curvature,
// scanlines, an aperture-grille tint, chromatic aberration, vignette, a gentle
// glow and a faint flicker.
//
// Implemented in raw OpenGL (the shader functions are loaded via
// SDL_GL_GetProcAddress, so no GLEW dependency — keeps the Windows cross-build
// clean). It only works when SDL's renderer is the OpenGL backend; the App
// falls back to a plain upscale otherwise.

#include <SDL.h>

namespace ssaver {

class CrtEffect {
public:
    ~CrtEffect();

    // Load GL entry points and compile the shader program. Returns false if a
    // usable GL context / shaders are unavailable (caller then skips the CRT).
    bool init();
    bool ok() const { return ready_; }

    // Draw `src` (the low-res scene) onto the current framebuffer through the
    // CRT shader. srcW/srcH are the low-res dimensions (drive scanline spacing);
    // outW/outH the window's drawable size; time is seconds for the flicker.
    void apply(SDL_Renderer* renderer, SDL_Texture* src,
               int outW, int outH, int srcW, int srcH, float time);

private:
    bool ready_ = false;
    unsigned int program_ = 0;
    int uTex_ = -1, uRes_ = -1, uTexScale_ = -1, uTime_ = -1;
};

} // namespace ssaver
