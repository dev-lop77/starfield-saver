#pragma once
// Reusable raw-OpenGL fullscreen-quad shader pass, for the rare GLSL events
// (wormhole, supernova, galaxy, nebula). Same technique as the CRT post-process
// (postfx_crt): GL entry points are loaded via SDL_GL_GetProcAddress (no GLEW),
// a GLSL 1.20 program is compiled, and a fullscreen quad is drawn through it.
//
// IMPORTANT — these passes must be *terminal*: draw them only after all SDL
// rendering for the frame is done and just before SDL_RenderPresent. Raw GL
// leaves SDL's renderer state cache stale, so any SDL draw issued afterwards
// would misbehave (this is exactly why the CRT pass also runs last). Events
// therefore composite as an additive overlay over the already-presented image,
// not into the low-res render target.

namespace ssaver {
namespace glsl {

// True if there's a current GL context and the shader entry points loaded.
// Lazily loads the entry points on first call; cheap thereafter. Returns false
// on the software/headless renderer, so callers can simply skip GL work.
bool available();

// How the pass blends onto the already-rendered image.
//   Add   - additive glow (GL_ONE, GL_ONE); fade by scaling the emitted colour.
//           Best for pure light emission (wormhole, supernova, galaxy).
//   Alpha - standard transparency (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); the
//           fragment's alpha controls coverage. Best for translucent gas that
//           should dim/tint the stars behind it (nebula).
enum class Blend { Add, Alpha };

// A compiled fullscreen-quad program. Compile once (ensure), then reuse every
// frame. Designed to live in a function-local static per event type so the
// shader is compiled a single time for the whole process.
class ShaderPass {
public:
    // Compile + link the standard vertex shader with `fragSrc`. Idempotent:
    // the first successful call builds the program, later calls are no-ops.
    // Returns false if GL is unavailable or compilation failed.
    bool ensure(const char* fragSrc);
    bool ok() const { return ok_; }

    // Bind the program, set the viewport, and enable the requested blending so
    // the effect composites over whatever is already on screen.
    void use(int viewportW, int viewportH, Blend blend = Blend::Add);

    // Set a uniform by name (queried each call; fine for a rare event).
    void setf(const char* name, float v);
    void set2(const char* name, float a, float b);
    void set3(const char* name, float a, float b, float c);

    // Draw the fullscreen quad (clip space -1..1, texcoord 0..1 in vUV).
    void drawQuad();

    // Unbind the program and restore blending to off.
    void finish();

private:
    unsigned int program_ = 0;
    bool tried_ = false;
    bool ok_ = false;
};

} // namespace glsl
} // namespace ssaver
