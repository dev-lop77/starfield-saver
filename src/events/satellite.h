#pragma once
// First pixel-art event: a Voyager-style probe you fly past. It emerges small
// near the centre, drifts outward along a random radial (the same direction the
// parallax stars stream) and grows as it sweeps off the edge — as if the camera
// were passing it. Built from an embedded char-grid sprite (see sprite.h), no
// asset files. The template to copy for future sprite events.

#include "event_base.h"

namespace ssaver {

class Satellite : public EventBase {
public:
    Satellite(int width, int height);

    void render(const RenderContext& ctx) override;
    const char* name() const override { return "satellite"; }

private:
    int width_, height_;
    float dirx_, diry_;  // outward travel direction (radial from centre)
};

} // namespace ssaver
