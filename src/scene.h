#pragma once
// Common interface for everything that draws into the scene.
//
// The starfield is a Scene that lives forever in the background. Cinematic
// "events" (a passing planet, a ship, a wormhole, a supernova...) are also
// Scenes, but transient: the EventScheduler spawns one, lets it run, and
// destroys it when it reports that it is finished.

#include <SDL.h>

namespace ssaver {

// Rendering happens onto a fixed low-resolution target (see config.h) which is
// then upscaled to the display. All scenes draw in this low-res pixel space.
struct RenderContext {
    SDL_Renderer* renderer;  // target is already set to the low-res texture
    int width;               // low-res width  (e.g. 480)
    int height;              // low-res height (e.g. 270)
    double elapsed;          // seconds since program start (for shimmer/animation)
};

// Context for the rare GLSL events (wormhole, supernova, ...). These don't draw
// into the low-res target: they composite a raw-GL shader pass over the final,
// already-upscaled window image, as a *terminal* step right before present (see
// gl_shader.h for why it must be last). So they work in window pixels, not
// low-res ones, and layer over the starfield additively.
struct GLRenderContext {
    SDL_Renderer* renderer;  // render target is the window (default framebuffer)
    int outW;                // window drawable width
    int outH;                // window drawable height
    double elapsed;          // seconds since program start
};

class Scene {
public:
    virtual ~Scene() = default;

    // Advance simulation by dt seconds.
    virtual void update(float dt) = 0;

    // Draw into the low-res render target.
    virtual void render(const RenderContext& ctx) = 0;

    // Rare GLSL events override these: usesGL() returns true and renderGL() does
    // a raw-GL shader pass over the window (see GLRenderContext). Such events
    // typically leave render() empty. Most scenes use neither.
    virtual bool usesGL() const { return false; }
    virtual void renderGL(const GLRenderContext& /*ctx*/) {}

    // Transient events return true once they have fully played out and can be
    // removed. The persistent starfield always returns false.
    virtual bool finished() const { return false; }

    // Human-readable name, useful for logging / the scheduler.
    virtual const char* name() const { return "scene"; }
};

} // namespace ssaver
