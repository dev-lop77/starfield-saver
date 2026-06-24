#pragma once
// A temporary "denser star region": a burst of extra parallax stars fades in,
// drifts outward exactly like the main field, and fades out — as if cruising
// briefly through a star cluster. Self-contained (owns its stars, like Comet
// owns its tail) but reuses the starfield tuning constants from config.h so the
// extra stars move and look identical to the permanent ones.

#include <vector>

#include "event_base.h"

namespace ssaver {

class StarBurst : public EventBase {
public:
    StarBurst(int width, int height);

    void update(float dt) override;
    void render(const RenderContext& ctx) override;
    const char* name() const override { return "starburst"; }

private:
    struct Star {
        float x, y;        // screen position
        float dirx, diry;  // unit outward direction
        float depth;       // 0 far .. 1 near
        Uint8 r, g, b;     // colour tint
    };
    void respawn(Star& s);

    int width_, height_;
    std::vector<Star> stars_;
};

} // namespace ssaver
