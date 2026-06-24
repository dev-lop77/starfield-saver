#pragma once
// Minimal embedded pixel-art sprites. A sprite is just a grid of characters
// (one C-string per row) plus a palette mapping characters to colours; '.' or
// ' ' (and any char not in the palette) is transparent. Each sprite pixel is
// drawn as a scale x scale block in the low-res target, so the art stays crisp
// through the nearest-neighbour upscale and the CRT pass.
//
// This keeps the art in code — no asset files, no SDL_image, nothing extra to
// ship — and gives every event a shared, copy-paste-free way to draw sprites.

#include <SDL.h>

namespace ssaver {

struct SpriteColor {
    char key;
    Uint8 r, g, b;
};

// Draw `rows[0..h)` (each a C-string of up to `w` chars) at (ox, oy), each cell
// a `scale`-sized block, with overall opacity `alpha` in [0,1].
inline void drawSprite(SDL_Renderer* renderer, const char* const* rows,
                       int w, int h, const SpriteColor* palette, int paletteN,
                       float ox, float oy, float scale, float alpha) {
    if (alpha <= 0.0f) return;
    Uint8 a = static_cast<Uint8>(255.0f * alpha);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int y = 0; y < h; ++y) {
        const char* row = rows[y];
        for (int x = 0; x < w; ++x) {
            char c = row[x];
            if (c == '\0') break;  // row ended early -> rest is transparent
            if (c == '.' || c == ' ') continue;
            const SpriteColor* col = nullptr;
            for (int i = 0; i < paletteN; ++i) {
                if (palette[i].key == c) { col = &palette[i]; break; }
            }
            if (!col) continue;
            SDL_SetRenderDrawColor(renderer, col->r, col->g, col->b, a);
            // Snap each block to the pixel grid and round its size up, so a
            // fractional (e.g. growing) scale leaves no seams between cells and
            // still looks crisp at integer scale.
            float bs = SDL_ceilf(scale);
            SDL_FRect px{SDL_floorf(ox + x * scale), SDL_floorf(oy + y * scale),
                         bs, bs};
            SDL_RenderFillRectF(renderer, &px);
        }
    }
}

// Draw a raw RGBA pixmap (w*h*4 bytes, row-major) the same way: each pixel a
// scale-sized, pixel-snapped block; per-pixel alpha is multiplied by `alpha`.
// For procedurally generated art (e.g. planets) where a fixed palette won't do.
inline void drawPixmap(SDL_Renderer* renderer, const Uint8* rgba, int w, int h,
                       float ox, float oy, float scale, float alpha) {
    if (alpha <= 0.0f) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    float bs = SDL_ceilf(scale);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const Uint8* p = rgba + (static_cast<size_t>(y) * w + x) * 4;
            if (p[3] == 0) continue;
            SDL_SetRenderDrawColor(renderer, p[0], p[1], p[2],
                                   static_cast<Uint8>(p[3] * alpha));
            SDL_FRect px{SDL_floorf(ox + x * scale), SDL_floorf(oy + y * scale),
                         bs, bs};
            SDL_RenderFillRectF(renderer, &px);
        }
    }
}

} // namespace ssaver
