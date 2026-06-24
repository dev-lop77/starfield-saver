#pragma once
// Pixel-art planet pass. Like the satellite it's a radial fly-by (emerges near
// the centre, drifts outward with the star flow, grows as it sweeps off), but
// the planet itself is generated procedurally at spawn into a small RGBA bitmap
// — a spherically-shaded disc with one of several terrain styles (rocky/cratered,
// gas-giant bands, ocean + continents, icy) and randomised colour and size.

#include <vector>

#include "event_base.h"

namespace ssaver {

class Planet : public EventBase {
public:
    Planet(int width, int height);

    void render(const RenderContext& ctx) override;
    const char* name() const override { return "planet"; }

private:
    void generate();  // build the shaded planet bitmap into rgba_

    int width_, height_;
    float dirx_, diry_;        // radial fly-by direction
    int grid_;                 // bitmap is grid_ x grid_
    std::vector<Uint8> rgba_;  // grid_*grid_*4, alpha 0 = outside the disc
    float maxScale_;           // block size at the closest point of the pass
};

} // namespace ssaver
