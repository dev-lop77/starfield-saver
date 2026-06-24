#pragma once
// A rare "screen crash": a round star at the centre rushes toward the viewer
// (growing brighter), hits with a white flash, then jagged cracks shoot out from
// the impact point and slowly fade so the screen "heals" (loops cleanly). The
// crack geometry is randomised once at spawn and stored.

#include <vector>

#include "event_base.h"

namespace ssaver {

class ScreenCrash : public EventBase {
public:
    ScreenCrash(int width, int height);

    void render(const RenderContext& ctx) override;
    const char* name() const override { return "screencrash"; }

private:
    struct Crack {
        float x1, y1, x2, y2;  // a single crack segment
        float dist;            // far end's distance from centre (for growth)
    };
    void generateCracks();

    int width_, height_;
    std::vector<Crack> cracks_;
};

} // namespace ssaver
