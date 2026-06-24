#pragma once
// Rare GLSL event: a supernova. A point of light swells, flares to a blinding
// white core, then blows out an expanding shock shell with radial rays before
// the debris fades. Colour, position, size and ray count are randomised per
// spawn. Drawn as an additive shader overlay (see gl_shader.h / Scene::renderGL).

#include "event_base.h"

namespace ssaver {

class Supernova : public EventBase {
public:
    Supernova(int width, int height);

    void render(const RenderContext& ctx) override { (void)ctx; }

    bool usesGL() const override { return true; }
    void renderGL(const GLRenderContext& ctx) override;

    const char* name() const override { return "supernova"; }

private:
    float cx_, cy_;        // centre in 0..1 screen UV
    float size_;           // final blast radius as a fraction of min screen dim
    float rays_;           // number of radial spikes
    float seed_;           // phase offset so rays/turbulence differ each spawn
    float cr_, cg_, cb_;   // blast colour (hot blue, gold, green, red, ...)
};

} // namespace ssaver
