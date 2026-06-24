#pragma once
// Rare GLSL event: a large, slow-drifting interstellar nebula. Unlike the other
// GLSL events this uses real *transparency* (alpha blending) rather than additive
// glow, so it dims and tints the stars behind it like a genuine gas cloud. It is
// deliberately big — it can cover most of the screen — and soft-edged. The two
// colours, overall size, opacity and drift are randomised per spawn.

#include "event_base.h"

namespace ssaver {

class Nebula : public EventBase {
public:
    Nebula(int width, int height);

    void render(const RenderContext& ctx) override { (void)ctx; }

    bool usesGL() const override { return true; }
    void renderGL(const GLRenderContext& ctx) override;

    const char* name() const override { return "nebula"; }

private:
    float cx_, cy_;          // centre in 0..1 screen UV
    float size_;             // cloud extent as a fraction of min screen dim (big)
    float opacity_;          // peak transparency
    float driftx_, drifty_;  // slow drift direction
    float seed_;             // decorrelate the noise field per spawn
    float ar_, ag_, ab_;     // colour A (cloud body)
    float br_, bg_, bb_;     // colour B (filament highlights)
};

} // namespace ssaver
