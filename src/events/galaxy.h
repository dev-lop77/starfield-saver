#pragma once
// Rare GLSL event: a distant spiral galaxy that drifts past, slowly rotating —
// a bright warm bulge, logarithmic spiral arms with dust mottling, and a faint
// halo. Arm count, tightness, inclination (tilt), spin direction/speed, colour
// and size are randomised per spawn. Additive shader overlay.

#include "event_base.h"

namespace ssaver {

class Galaxy : public EventBase {
public:
    Galaxy(int width, int height);

    void render(const RenderContext& ctx) override { (void)ctx; }

    bool usesGL() const override { return true; }
    void renderGL(const GLRenderContext& ctx) override;

    const char* name() const override { return "galaxy"; }

private:
    float cx_, cy_;        // centre in 0..1 screen UV
    float size_;           // disc radius as a fraction of min screen dim
    float arms_;           // spiral arm count (2..4)
    float wind_;           // how tightly the arms wind
    float spin_;           // rotation speed/direction
    float tilt_;           // inclination: >1 flattens the disc
    float roll_;           // fixed rotation of the whole disc
    float cr_, cg_, cb_;   // arm/disc colour
};

} // namespace ssaver
