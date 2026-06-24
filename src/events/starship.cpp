#include "starship.h"

#include <algorithm>  // std::min

#include "../util.h"

namespace ssaver {

namespace {
constexpr float kTwoPi = 6.2831853f;

// A filled disc via horizontal spans (no SDL circle primitive), pixel-snapped.
void disc(SDL_Renderer* r, float cx, float cy, float rad,
          Uint8 cr, Uint8 cg, Uint8 cb, Uint8 a) {
    if (a == 0) return;
    SDL_SetRenderDrawColor(r, cr, cg, cb, a);
    if (rad < 0.8f) {
        SDL_FRect px{SDL_floorf(cx - 0.5f), SDL_floorf(cy - 0.5f), 1.0f, 1.0f};
        SDL_RenderFillRectF(r, &px);
        return;
    }
    int R = static_cast<int>(rad);
    for (int dy = -R; dy <= R; ++dy) {
        float dx = SDL_sqrtf(rad * rad - static_cast<float>(dy) * dy);
        SDL_FRect row{SDL_floorf(cx - dx), SDL_floorf(cy + dy),
                      SDL_ceilf(2.0f * dx), 1.0f};
        SDL_RenderFillRectF(r, &row);
    }
}

// A glowing engine nozzle: a few translucent halo layers + a hot core.
void nozzle(SDL_Renderer* r, float x, float y, float coreR, float glowR,
            float intensity) {
    if (intensity <= 0.0f) return;
    for (int i = 3; i >= 1; --i) {
        float rr = glowR * i / 3.0f;
        Uint8 a = static_cast<Uint8>(intensity * 60.0f * (4 - i) / 3.0f);
        disc(r, x, y, rr, 80, 150, 255, a);
    }
    disc(r, x, y, coreR, 215, 240, 255, static_cast<Uint8>(intensity * 255.0f));
}
}  // namespace

Starship::Starship(int width, int height)
    : EventBase(/*enter=*/1.0f, /*hold=*/3.0f, /*exit=*/1.2f),
      width_(width), height_(height) {
    float ang = frand(0.0f, kTwoPi);
    dirx_ = SDL_cosf(ang);
    diry_ = SDL_sinf(ang);
}

void Starship::render(const RenderContext& ctx) {
    SDL_Renderer* r = ctx.renderer;
    float a = alpha();
    if (a <= 0.0f) return;

    const float cx = width_ * 0.5f;
    const float cy = height_ * 0.5f;
    const float minDim = static_cast<float>(std::min(width_, height_));
    const float diag = SDL_sqrtf(static_cast<float>(width_) * width_ +
                                 static_cast<float>(height_) * height_);

    // Accelerate inward: lingers far out, then warps toward the centre.
    float t = lifeT();
    float p = SDL_powf(t, 2.2f);
    float radius = diag * 0.42f * (1.0f - p);
    float shipX = cx + dirx_ * radius;
    float shipY = cy + diry_ * radius;

    float coreR = lerpf(minDim * 0.020f, minDim * 0.020f * 0.12f, p);  // shrinks
    float glowR = coreR * 2.8f;
    float spacing = coreR * 2.6f;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // Warp streak stretching toward the centre, growing late in the pass.
    float streakP = clampf((p - 0.40f) / 0.60f, 0.0f, 1.0f);
    if (streakP > 0.0f) {
        float inx = -dirx_, iny = -diry_;  // toward the centre
        float len = lerpf(coreR * 4.0f, diag * 0.40f, streakP * streakP);
        const int seg = 9;
        for (int i = 1; i <= seg; ++i) {
            float f = static_cast<float>(i) / seg;
            float bw = coreR * (1.6f * (1.0f - f) + 0.4f);
            disc(r, shipX + inx * len * f, shipY + iny * len * f, bw * 0.5f,
                 200, 235, 255,
                 static_cast<Uint8>(210.0f * (1.0f - f) * streakP * a));
        }
    }

    // Three engine nozzles spread across the rear (perpendicular to travel),
    // pulsing and brighter as it accelerates.
    float intensity = clampf((0.55f + 0.45f * SDL_sinf(age_ * 14.0f)) *
                                 (0.5f + 0.8f * p),
                             0.0f, 1.0f) * a;
    float px = -diry_, py = dirx_;  // perpendicular unit (the rear spreads here)
    for (int k = -1; k <= 1; ++k) {
        float nx = shipX + px * spacing * k;
        float ny = shipY + py * spacing * k;
        float cR = coreR * (k == 0 ? 1.15f : 0.85f);  // centre engine a bit bigger
        nozzle(r, nx, ny, cR, glowR, intensity);
    }

    // Warp-out flash as it vanishes at the centre.
    if (p > 0.82f) {
        float fl = (p - 0.82f) / 0.18f;
        disc(r, shipX, shipY, minDim * 0.022f * fl, 230, 245, 255,
             static_cast<Uint8>(210.0f * fl * a));
    }
}

} // namespace ssaver
