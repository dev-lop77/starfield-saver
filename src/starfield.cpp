#include "starfield.h"

#include <algorithm>  // std::min, std::max

#include "config.h"
#include "util.h"

namespace ssaver {

namespace {
constexpr float kTwoPi = 6.2831853f;
}

Starfield::Starfield(int width, int height) : width_(width), height_(height) {
    // Scale the count by screen area so density stays consistent at any
    // resolution (kStarCount is tuned for ~1080p).
    double areaRatio = (static_cast<double>(width_) * height_) / (1920.0 * 1080.0);
    int count = static_cast<int>(cfg::kStarCount * areaRatio);
    count = std::max(150, std::min(count, 6000));
    stars_.resize(count);
    for (auto& s : stars_) respawn(s);
}

void Starfield::respawn(Star& s) {
    // Uniform over the whole screen, so stars also appear at the borders rather
    // than all emerging from the centre.
    s.x = frand(0.0f, static_cast<float>(width_));
    s.y = frand(0.0f, static_cast<float>(height_));

    // Depth biased toward "far" (u*u) so the sky is mostly faint distant stars
    // with a few close, bright ones.
    float u = frand();
    s.depth = cfg::kStarDepthMin + (1.0f - cfg::kStarDepthMin) * u * u;

    // Fixed outward (radial-from-centre) travel direction. A star spawned right
    // at the centre gets a random direction instead.
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

    // Mostly white with a faint blue/amber tint, like a real star census. A few
    // get a stronger colour so the field is not monochrome.
    float t = frand();
    if (t < 0.70f) {            // white
        s.r = s.g = s.b = 255;
    } else if (t < 0.85f) {     // cool blue-white
        s.r = 200; s.g = 220; s.b = 255;
    } else if (t < 0.95f) {     // warm amber
        s.r = 255; s.g = 225; s.b = 190;
    } else {                    // rare reddish giant
        s.r = 255; s.g = 180; s.b = 170;
    }
}

float Starfield::speedOf(const Star& s) const {
    // Speed is set by depth (near = faster), expressed as a fraction of the
    // smaller screen dimension so it looks the same at any resolution.
    const float minDim = static_cast<float>(std::min(width_, height_));
    return cfg::kStarSpeed * minDim * s.depth * warp_;
}

void Starfield::update(float dt) {
    const float margin = 4.0f;  // let a star fully clear the edge before recycling
    for (auto& s : stars_) {
        float step = speedOf(s) * dt;
        s.x += s.dirx * step;
        s.y += s.diry * step;
        if (s.x < -margin || s.x > width_ + margin ||
            s.y < -margin || s.y > height_ + margin) {
            respawn(s);
        }
    }
}

void Starfield::render(const RenderContext& ctx) {
    SDL_Renderer* r = ctx.renderer;
    for (const auto& s : stars_) {
        // Far stars are dim, near stars full intensity.
        float bright = 0.20f + 0.80f * s.depth;
        Uint8 cr = static_cast<Uint8>(s.r * bright);
        Uint8 cg = static_cast<Uint8>(s.g * bright);
        Uint8 cb = static_cast<Uint8>(s.b * bright);
        SDL_SetRenderDrawColor(r, cr, cg, cb, 255);

        if (s.depth > 0.35f) {
            // Near stars: a short motion trail (length = a fixed slice of travel
            // time, so it's framerate-independent and stays modest — no firework
            // streaks), plus a 2x2 core for the very closest.
            float trail = speedOf(s) * cfg::kStreakSeconds;
            float tx = s.x - s.dirx * trail;
            float ty = s.y - s.diry * trail;
            SDL_RenderDrawLineF(r, tx, ty, s.x, s.y);
            if (s.depth > 0.80f) {
                SDL_FRect core{s.x - 0.5f, s.y - 0.5f, 2.0f, 2.0f};
                SDL_RenderFillRectF(r, &core);
            }
        } else {
            // Far stars: a single faint point.
            SDL_RenderDrawPointF(r, s.x, s.y);
        }
    }
}

} // namespace ssaver
