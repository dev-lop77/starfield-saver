#pragma once
// First rare GLSL event: a wormhole / spacetime portal that opens ahead, a
// swirling tunnel of light you appear to be falling toward, then collapses.
//
// Unlike the pixel-art events, this is drawn with a fragment shader as a
// terminal additive overlay over the final image (see gl_shader.h and
// Scene::renderGL). On the software/headless renderer it simply draws nothing.

#include "event_base.h"

namespace ssaver {

class Wormhole : public EventBase {
public:
    Wormhole(int width, int height);

    // No low-res drawing: the visual is entirely in the GL pass.
    void render(const RenderContext& ctx) override { (void)ctx; }

    bool usesGL() const override { return true; }
    void renderGL(const GLRenderContext& ctx) override;

    const char* name() const override { return "wormhole"; }

private:
    float cx_, cy_;     // portal centre in 0..1 screen UV (slightly off-centre)
    float radius_;      // portal radius as a fraction of the min screen dimension
    float spin_;        // swirl direction/speed multiplier (sign randomised)
    float hue_;         // 0..1 colour bias (cool blue .. violet .. teal)
};

} // namespace ssaver
