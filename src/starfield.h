#pragma once
// Depth-parallax starfield. Stars are scattered across the whole screen at
// random depths and drift slowly outward, giving a calm "cruising through space"
// parallax: near stars are bright, large and faster (with a short trail); far
// stars are faint, slow single points. Unlike a 1/z perspective warp, nothing
// accelerates as it approaches the edge, so there's no firework-burst feel.

#include <vector>

#include "scene.h"

namespace ssaver {

class Starfield : public Scene {
public:
    // densityScale multiplies the built-in star count (1.0 = default); set from
    // the runtime config so the user can thin out or pack the sky.
    Starfield(int width, int height, float densityScale = 1.0f);

    void update(float dt) override;
    void render(const RenderContext& ctx) override;
    const char* name() const override { return "starfield"; }

    // Global warp factor: 1.0 normal cruise. Events (e.g. a wormhole) can crank
    // this up for a dramatic acceleration (faster drift + longer trails).
    void setWarp(float w) { warp_ = w; }
    float warp() const { return warp_; }

private:
    struct Star {
        float x, y;        // screen position, in low-res pixels
        float dirx, diry;  // unit outward travel direction (fixed for its life)
        float depth;       // 0 = far (faint/slow), 1 = near (bright/fast)
        Uint8 r, g, b;     // subtle colour tint
    };

    void respawn(Star& s);
    // Per-star screen speed (px/sec) for the current warp; shared by update and
    // render so the motion and the trail length always agree.
    float speedOf(const Star& s) const;

    int width_;
    int height_;
    float warp_ = 1.0f;
    std::vector<Star> stars_;
};

} // namespace ssaver
