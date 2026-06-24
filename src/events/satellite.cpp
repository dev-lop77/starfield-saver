#include "satellite.h"

#include <algorithm>  // std::min

#include "../sprite.h"
#include "../util.h"

namespace ssaver {

namespace {
constexpr int kW = 24;
constexpr int kH = 14;
constexpr float kTwoPi = 6.2831853f;

// Voyager-style probe: a dominant high-gain dish with a feed mast, the bus body,
// a science boom + instrument (camera) box, a long magnetometer boom reaching
// up-right, and an RTG below. '.' transparent. Easy to reshape — keep rows kW
// wide.
const char* const kRows[kH] = {
    ".....................A..",
    "....................A...",
    "....OOOOO..........A....",
    "...OOOOOOO........A.....",
    "..OOOOOOOOO......A......",
    ".OOOOOOOOOOO....A.......",
    ".OOOOODOOOOOBBBA........",
    ".OOOOODOOOOOBBB....III..",
    ".OOOOODOOOOOBBBAAAAIII..",
    "..OOOOOOOOO.BBB....III..",
    "...OOOOOOO...A..........",
    "....OOOOO....A..........",
    "............rrr.........",
    "............rrr.........",
};

const SpriteColor kPalette[] = {
    {'O', 225, 228, 236},  // high-gain dish
    {'D',  85,  88, 100},  // feed mast / sub-reflector
    {'B', 175, 178, 190},  // bus body
    {'A', 130, 134, 148},  // booms / struts
    {'I',  95,  98, 112},  // instrument (camera) box
    {'r', 200, 150,  85},  // RTG (warm gold)
};
constexpr int kPaletteN = sizeof(kPalette) / sizeof(kPalette[0]);

constexpr int kBeaconCol = 20;  // small blinking light on the camera platform
constexpr int kBeaconRow = 8;
}  // namespace

Satellite::Satellite(int width, int height)
    : EventBase(/*enter=*/1.2f, /*hold=*/4.0f, /*exit=*/1.5f),
      width_(width), height_(height) {
    // A random radial direction — the probe streams outward like the stars.
    float ang = frand(0.0f, kTwoPi);
    dirx_ = SDL_cosf(ang);
    diry_ = SDL_sinf(ang);
}

void Satellite::render(const RenderContext& ctx) {
    SDL_Renderer* r = ctx.renderer;
    float a = alpha();
    if (a <= 0.0f) return;

    const float cx = width_ * 0.5f;
    const float cy = height_ * 0.5f;
    const float minDim = static_cast<float>(std::min(width_, height_));
    const float diag = SDL_sqrtf(static_cast<float>(width_) * width_ +
                                 static_cast<float>(height_) * height_);

    // Accelerate outward (ease-in) so it drifts slowly when distant and sweeps
    // past as it nears — the fly-by feel.
    float t = lifeT();
    float p = SDL_powf(t, 1.7f);
    float radius = lerpf(diag * 0.02f, diag * 0.58f, p);   // centre -> off-screen
    float scale = lerpf(minDim * 0.0035f, minDim * 0.016f, p);  // far -> near
    if (scale < 1.0f) scale = 1.0f;

    // Place the sprite so its centre rides the path point.
    float ox = cx + dirx_ * radius - kW * scale * 0.5f;
    float oy = cy + diry_ * radius - kH * scale * 0.5f;
    drawSprite(r, kRows, kW, kH, kPalette, kPaletteN, ox, oy, scale, a);

    // Subtle blinking light on the camera platform.
    float pulse = 0.5f + 0.5f * SDL_sinf(age_ * 7.0f);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 70, 50, static_cast<Uint8>(255.0f * pulse * a));
    float bs = SDL_ceilf(scale);
    SDL_FRect beacon{SDL_floorf(ox + kBeaconCol * scale),
                     SDL_floorf(oy + kBeaconRow * scale), bs, bs};
    SDL_RenderFillRectF(r, &beacon);
}

} // namespace ssaver
