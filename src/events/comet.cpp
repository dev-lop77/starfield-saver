#include "comet.h"

namespace ssaver {

Comet::Comet(int width, int height)
    : EventBase(/*enter=*/0.4f, /*hold=*/2.6f, /*exit=*/0.4f),
      width_(width), height_(height) {
    // Travel along a diagonal: pick a start just off one edge and an end just
    // off the opposite region, so the comet crosses a good chunk of sky.
    bool fromLeft = chance(0.5f);
    startX_ = fromLeft ? -20.0f : width_ + 20.0f;
    startY_ = frand(-10.0f, height_ * 0.5f);
    endX_   = fromLeft ? width_ + 20.0f : -20.0f;
    endY_   = frand(height_ * 0.5f, height_ + 10.0f);

    // Comets read as cool white/blue with a hint of cyan.
    r_ = 200; g_ = 230; b_ = 255;
}

void Comet::render(const RenderContext& ctx) {
    SDL_Renderer* r = ctx.renderer;
    float a = alpha();
    float t = lifeT();

    // Head position interpolated along the path.
    float hx = lerpf(startX_, endX_, t);
    float hy = lerpf(startY_, endY_, t);

    // Direction (normalised) for laying down the tail behind the head.
    float dx = endX_ - startX_;
    float dy = endY_ - startY_;
    float len = SDL_sqrtf(dx * dx + dy * dy);
    if (len > 0.0f) { dx /= len; dy /= len; }

    // Tail: a series of segments behind the head, dimming with distance.
    const int kTailSegments = 18;
    const float kSegLen = 4.0f;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = kTailSegments; i >= 1; --i) {
        float back = i * kSegLen;
        float tx = hx - dx * back;
        float ty = hy - dy * back;
        float fade = (1.0f - static_cast<float>(i) / kTailSegments) * a;
        SDL_SetRenderDrawColor(r, r_, g_, b_, static_cast<Uint8>(180 * fade));
        SDL_RenderDrawPointF(r, tx, ty);
    }

    // Glowing head: a small bright cross/core.
    SDL_SetRenderDrawColor(r, 255, 255, 255, static_cast<Uint8>(255 * a));
    SDL_FRect core{hx - 1.0f, hy - 1.0f, 2.0f, 2.0f};
    SDL_RenderFillRectF(r, &core);
    SDL_SetRenderDrawColor(r, r_, g_, b_, static_cast<Uint8>(160 * a));
    SDL_RenderDrawPointF(r, hx + 2, hy);
    SDL_RenderDrawPointF(r, hx - 2, hy);
    SDL_RenderDrawPointF(r, hx, hy + 2);
    SDL_RenderDrawPointF(r, hx, hy - 2);
}

} // namespace ssaver
