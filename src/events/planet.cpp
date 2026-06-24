#include "planet.h"

#include <algorithm>  // std::min

#include "../sprite.h"
#include "../util.h"

namespace ssaver {

namespace {
constexpr int kGrid = 32;          // planet bitmap resolution
constexpr float kTwoPi = 6.2831853f;

struct Blob { float x, y, r; };    // a circular surface feature

Uint8 clampByte(float v) {
    return static_cast<Uint8>(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v));
}
}  // namespace

Planet::Planet(int width, int height)
    : EventBase(/*enter=*/1.5f, /*hold=*/6.0f, /*exit=*/1.8f),
      width_(width), height_(height) {
    float ang = frand(0.0f, kTwoPi);
    dirx_ = SDL_cosf(ang);
    diry_ = SDL_sinf(ang);

    grid_ = kGrid;
    rgba_.assign(static_cast<size_t>(grid_) * grid_ * 4, 0);

    float minDim = static_cast<float>(std::min(width_, height_));
    float sizeFrac = frand(0.22f, 0.55f);  // apparent size varies per planet
    maxScale_ = sizeFrac * minDim / grid_;

    generate();
}

void Planet::generate() {
    const float R = grid_ * 0.5f - 0.5f;
    const float cx = grid_ * 0.5f;
    const float cy = grid_ * 0.5f;

    // Light from the upper-left, slightly toward the viewer — gives the disc a
    // spherical, terminator-shaded look.
    float lx = -0.5f, ly = -0.62f, lz = 0.60f;
    float ll = SDL_sqrtf(lx * lx + ly * ly + lz * lz);
    lx /= ll; ly /= ll; lz /= ll;

    // --- Per-planet parameters, chosen once ------------------------------------
    int type = irand(0, 3);  // 0 rocky, 1 gas giant, 2 ocean+land, 3 ice
    auto vary = [](int base, int amt) { return base + irand(-amt, amt); };

    Uint8 baseR = 180, baseG = 170, baseB = 150;
    Uint8 landR = 0, landG = 0, landB = 0;
    float bandPhase = frand(0.0f, kTwoPi);
    float bandFreq = frand(4.0f, 8.0f);
    std::vector<Blob> features;
    auto addBlobs = [&](int n, float rmin, float rmax) {
        for (int i = 0; i < n; ++i) {
            float rr = frand(0.0f, R * 0.80f);
            float aa = frand(0.0f, kTwoPi);
            features.push_back({cx + SDL_cosf(aa) * rr, cy + SDL_sinf(aa) * rr,
                                frand(rmin, rmax)});
        }
    };

    if (type == 0) {  // rocky / cratered
        const int kPick[3][3] = {{150,145,140}, {165,115,85}, {175,150,110}};
        const int* c = kPick[irand(0, 2)];
        baseR = clampByte(vary(c[0], 18)); baseG = clampByte(vary(c[1], 18));
        baseB = clampByte(vary(c[2], 18));
        addBlobs(irand(5, 10), 1.5f, 4.0f);
    } else if (type == 1) {  // gas giant (banded)
        const int kPick[4][3] = {{205,170,120}, {210,150,95},
                                 {135,160,200}, {210,200,170}};
        const int* c = kPick[irand(0, 3)];
        baseR = clampByte(vary(c[0], 15)); baseG = clampByte(vary(c[1], 15));
        baseB = clampByte(vary(c[2], 15));
        addBlobs(1, R * 0.16f, R * 0.24f);  // a storm spot
    } else if (type == 2) {  // ocean + continents
        baseR = clampByte(vary(40, 12)); baseG = clampByte(vary(85, 12));
        baseB = clampByte(vary(160, 18));
        if (chance(0.5f)) { landR = clampByte(vary(70, 15));
                            landG = clampByte(vary(120, 15));
                            landB = clampByte(vary(60, 12)); }
        else { landR = clampByte(vary(150, 15)); landG = clampByte(vary(125, 15));
               landB = clampByte(vary(85, 12)); }
        addBlobs(irand(4, 8), 3.0f, 7.0f);
    } else {  // icy
        baseR = clampByte(vary(205, 12)); baseG = clampByte(vary(220, 10));
        baseB = clampByte(vary(235, 8));
        addBlobs(irand(5, 9), 2.0f, 5.0f);
    }

    // --- Rasterise the shaded sphere -------------------------------------------
    for (int y = 0; y < grid_; ++y) {
        for (int x = 0; x < grid_; ++x) {
            size_t idx = (static_cast<size_t>(y) * grid_ + x) * 4;
            float dx = x + 0.5f - cx;
            float dy = y + 0.5f - cy;
            float d2 = dx * dx + dy * dy;
            if (d2 > R * R) { rgba_[idx + 3] = 0; continue; }

            float z = SDL_sqrtf(R * R - d2);
            float diff = (dx / R) * lx + (dy / R) * ly + (z / R) * lz;
            float bright = clampf(0.22f + 0.95f * diff, 0.0f, 1.25f);
            float limb = 1.0f - 0.30f * SDL_powf(d2 / (R * R), 3.0f);
            float latU = dy / R;  // -1 (top) .. 1 (bottom)

            float cr = baseR, cg = baseG, cb = baseB;
            bool inFeature = false;
            for (const auto& b : features) {
                float fx = x + 0.5f - b.x, fy = y + 0.5f - b.y;
                if (fx * fx + fy * fy < b.r * b.r) { inFeature = true; break; }
            }

            if (type == 0) {                       // craters darken
                if (inFeature) { cr *= 0.62f; cg *= 0.62f; cb *= 0.62f; }
            } else if (type == 1) {                // latitude bands + storm
                float band = 0.82f + 0.18f * SDL_sinf(latU * bandFreq + bandPhase);
                cr *= band; cg *= band; cb *= band;
                if (inFeature) { cr *= 1.18f; cg *= 1.12f; cb *= 1.05f; }
            } else if (type == 2) {                // land blobs + polar caps
                if (inFeature) { cr = landR; cg = landG; cb = landB; }
                if (SDL_fabsf(latU) > 0.82f) { cr = 228; cg = 234; cb = 244; }
            } else {                               // ice mottling
                if (inFeature) { cr *= 0.88f; cg *= 0.91f; cb *= 0.96f; }
            }

            float shade = bright * limb;
            rgba_[idx + 0] = clampByte(cr * shade);
            rgba_[idx + 1] = clampByte(cg * shade);
            rgba_[idx + 2] = clampByte(cb * shade);
            rgba_[idx + 3] = 255;
        }
    }
}

void Planet::render(const RenderContext& ctx) {
    SDL_Renderer* r = ctx.renderer;
    float a = alpha();
    if (a <= 0.0f) return;

    const float cx = width_ * 0.5f;
    const float cy = height_ * 0.5f;
    const float diag = SDL_sqrtf(static_cast<float>(width_) * width_ +
                                 static_cast<float>(height_) * height_);

    float t = lifeT();
    float p = SDL_powf(t, 1.6f);  // accelerate outward (fly-by)
    float radius = lerpf(diag * 0.03f, diag * 0.55f, p);
    float scale = lerpf(maxScale_ * 0.30f, maxScale_, p);  // grows modestly

    float ox = cx + dirx_ * radius - grid_ * scale * 0.5f;
    float oy = cy + diry_ * radius - grid_ * scale * 0.5f;
    drawPixmap(r, rgba_.data(), grid_, grid_, ox, oy, scale, a);
}

} // namespace ssaver
