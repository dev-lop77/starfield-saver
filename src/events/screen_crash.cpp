#include "screen_crash.h"

#include <algorithm>  // std::min

#include "../util.h"  // frand, irand, clampf, lerpf

namespace ssaver {

namespace {
constexpr float kApproach = 1.2f;   // round star grows toward the viewer
constexpr float kFlash    = 0.18f;  // brief white flash on impact
constexpr float kGrow     = 0.30f;  // cracks shoot outward over this
constexpr float kHold     = 0.50f;  // cracks fully visible
constexpr float kCracks   = 2.20f;  // total crack life (grow + hold + fade)
constexpr float kTwoPi    = 6.2831853f;

// SDL has no filled-circle primitive: fill a disc with horizontal spans so the
// approaching star reads as round, not square.
void drawFilledDisc(SDL_Renderer* r, float cx, float cy, float rad) {
    int R = static_cast<int>(rad);
    for (int dy = -R; dy <= R; ++dy) {
        float dx = SDL_sqrtf(rad * rad - static_cast<float>(dy) * dy);
        SDL_RenderDrawLineF(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}
}  // namespace

ScreenCrash::ScreenCrash(int width, int height)
    : EventBase(/*enter=*/kApproach, /*hold=*/kCracks, /*exit=*/0.0f),
      width_(width), height_(height) {
    generateCracks();
}

void ScreenCrash::generateCracks() {
    const float cx = width_ * 0.5f;
    const float cy = height_ * 0.5f;
    const float minDim = static_cast<float>(std::min(width_, height_));
    const float maxR = 0.5f * SDL_sqrtf(static_cast<float>(width_) * width_ +
                                        static_cast<float>(height_) * height_) *
                       1.05f;  // a little past the corners
    const float step = minDim * 0.04f;

    // Walk a jagged polyline outward from a point, wiggling around a base angle.
    auto walk = [&](float x, float y, float baseAngle, float len) {
        float dist = SDL_sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy));
        while (dist < len) {
            float ang = baseAngle + frand(-0.30f, 0.30f);
            float seg = step * frand(0.7f, 1.3f);
            float nx = x + SDL_cosf(ang) * seg;
            float ny = y + SDL_sinf(ang) * seg;
            float endDist = SDL_sqrtf((nx - cx) * (nx - cx) + (ny - cy) * (ny - cy));
            cracks_.push_back({x, y, nx, ny, endDist});
            x = nx; y = ny; dist = endDist;
            if (x < -10 || x > width_ + 10 || y < -10 || y > height_ + 10) break;
        }
    };

    // Main cracks radiating from the centre.
    int mains = irand(7, 11);
    for (int i = 0; i < mains; ++i) {
        float ang = (static_cast<float>(i) / mains) * kTwoPi + frand(-0.25f, 0.25f);
        walk(cx, cy, ang, maxR * frand(0.55f, 1.0f));
    }

    // A few shorter branch cracks starting partway out (so they appear to split
    // off the mains as the fracture spreads).
    int branches = irand(4, 8);
    for (int i = 0; i < branches; ++i) {
        float ang = frand(0.0f, kTwoPi);
        float startR = frand(0.10f, 0.50f) * maxR;
        float x = cx + SDL_cosf(ang) * startR;
        float y = cy + SDL_sinf(ang) * startR;
        walk(x, y, ang + frand(-0.8f, 0.8f), startR + frand(0.15f, 0.40f) * maxR);
    }
}

void ScreenCrash::render(const RenderContext& ctx) {
    SDL_Renderer* r = ctx.renderer;
    const float cx = width_ * 0.5f;
    const float cy = height_ * 0.5f;
    const float minDim = static_cast<float>(std::min(width_, height_));
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // --- Phase 1: approach (a round star grows toward the viewer) ---------------
    if (age_ < kApproach) {
        float p = age_ / kApproach;
        float ease = p * p;  // accelerate as it nears the screen
        float radius = lerpf(0.5f, minDim * 0.03f, ease);
        Uint8 a = static_cast<Uint8>(lerpf(40.0f, 255.0f, ease));
        SDL_SetRenderDrawColor(r, 245, 248, 255, a);
        drawFilledDisc(r, cx, cy, radius);
        // A simple 4-point sparkle so it reads as a star.
        float spike = radius * 2.6f;
        SDL_RenderDrawLineF(r, cx - spike, cy, cx + spike, cy);
        SDL_RenderDrawLineF(r, cx, cy - spike, cx, cy + spike);
        return;
    }

    // --- Phase 2: impact flash --------------------------------------------------
    float ta = age_ - kApproach;
    if (ta < kFlash) {
        float f = 1.0f - ta / kFlash;
        SDL_SetRenderDrawColor(r, 255, 255, 255, static_cast<Uint8>(220.0f * f));
        SDL_FRect full{0.0f, 0.0f, static_cast<float>(width_),
                       static_cast<float>(height_)};
        SDL_RenderFillRectF(r, &full);
    }

    // --- Phase 3: cracks shoot outward, then fade -------------------------------
    float maxR = 0.5f * SDL_sqrtf(static_cast<float>(width_) * width_ +
                                  static_cast<float>(height_) * height_) * 1.05f;
    float growthRadius = clampf(ta / kGrow, 0.0f, 1.0f) * maxR;
    float crackAlpha = ta < kHold
                           ? 1.0f
                           : clampf(1.0f - (ta - kHold) / (kCracks - kHold),
                                    0.0f, 1.0f);
    if (crackAlpha > 0.0f) {
        SDL_SetRenderDrawColor(r, 235, 242, 255,
                               static_cast<Uint8>(255.0f * crackAlpha));
        for (const auto& c : cracks_) {
            if (c.dist <= growthRadius) {
                SDL_RenderDrawLineF(r, c.x1, c.y1, c.x2, c.y2);
            }
        }
    }
}

} // namespace ssaver
