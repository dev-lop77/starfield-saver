#pragma once
// Original sci-fi starship pass. Flying away from you, all you really see is its
// cluster of glowing rear engine nozzles. They start out near an edge and recede
// toward the vanishing point (centre), accelerating — i.e. moving *against* the
// outward star flow, as if jumping away — with a warp streak toward the centre
// and a flash as it goes. Drawn procedurally (glowing discs); no rotation needed.

#include "event_base.h"

namespace ssaver {

class Starship : public EventBase {
public:
    Starship(int width, int height);

    void render(const RenderContext& ctx) override;
    const char* name() const override { return "starship"; }

private:
    int width_, height_;
    float dirx_, diry_;  // outward radial of its start point (it recedes inward)
};

} // namespace ssaver
