#include "star_burst.h"

#include <algorithm>  // std::min, std::max

#include "../config.h"
#include "../util.h"

namespace ssaver {

namespace {
constexpr float kTwoPi = 6.2831853f;
// Extra stars to add at full presence, as a fraction of the (area-scaled) base
// count — ~0.9 nearly doubles the on-screen density at the peak.
constexpr float kBurstFraction = 0.9f;
}

StarBurst::StarBurst(int width, int height)
    : EventBase(/*enter=*/1.2f, /*hold=*/3.0f, /*exit=*/1.6f),
      width_(width), height_(height) {
    double areaRatio = (static_cast<double>(width_) * height_) / (1920.0 * 1080.0);
    int n = static_cast<int>(cfg::kStarCount * areaRatio * kBurstFraction);
    n = std::max(60, std::min(n, 5000));
    stars_.resize(n);
    for (auto& s : stars_) respawn(s);
}

void StarBurst::respawn(Star& s) {
    // Same depth-parallax model as the permanent field (see starfield.cpp).
    s.x = frand(0.0f, static_cast<float>(width_));
    s.y = frand(0.0f, static_cast<float>(height_));

    float u = frand();
    s.depth = cfg::kStarDepthMin + (1.0f - cfg::kStarDepthMin) * u * u;

    float dx = s.x - width_ * 0.5f;
    float dy = s.y - height_ * 0.5f;
    float len = SDL_sqrtf(dx * dx + dy * dy);
    if (len > 0.001f) {
        s.dirx = dx / len;
        s.diry = dy / len;
    } else {
        float a = frand(0.0f, kTwoPi);
        s.dirx = SDL_cosf(a);
        s.diry = SDL_sinf(a);
    }

    // Lean a touch whiter/cooler than the main census, so the cluster reads as
    // bright and crowded.
    float t = frand();
    if (t < 0.75f)      { s.r = s.g = s.b = 255; }
    else if (t < 0.90f) { s.r = 200; s.g = 220; s.b = 255; }
    else                { s.r = 255; s.g = 235; s.b = 210; }
}

void StarBurst::update(float dt) {
    EventBase::update(dt);
    const float minDim = static_cast<float>(std::min(width_, height_));
    const float margin = 4.0f;
    for (auto& s : stars_) {
        float step = cfg::kStarSpeed * minDim * s.depth * dt;
        s.x += s.dirx * step;
        s.y += s.diry * step;
        if (s.x < -margin || s.x > width_ + margin ||
            s.y < -margin || s.y > height_ + margin) {
            respawn(s);
        }
    }
}

void StarBurst::render(const RenderContext& ctx) {
    SDL_Renderer* r = ctx.renderer;
    float a = alpha();
    if (a <= 0.0f) return;

    const float minDim = static_cast<float>(std::min(width_, height_));
    Uint8 ca = static_cast<Uint8>(255.0f * a);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);  // fade with the envelope
    for (const auto& s : stars_) {
        float bright = 0.20f + 0.80f * s.depth;
        Uint8 cr = static_cast<Uint8>(s.r * bright);
        Uint8 cg = static_cast<Uint8>(s.g * bright);
        Uint8 cb = static_cast<Uint8>(s.b * bright);
        SDL_SetRenderDrawColor(r, cr, cg, cb, ca);

        if (s.depth > 0.35f) {
            float trail = cfg::kStarSpeed * minDim * s.depth * cfg::kStreakSeconds;
            SDL_RenderDrawLineF(r, s.x - s.dirx * trail, s.y - s.diry * trail,
                                s.x, s.y);
            if (s.depth > 0.80f) {
                SDL_FRect core{s.x - 0.5f, s.y - 0.5f, 2.0f, 2.0f};
                SDL_RenderFillRectF(r, &core);
            }
        } else {
            SDL_RenderDrawPointF(r, s.x, s.y);
        }
    }
}

} // namespace ssaver
