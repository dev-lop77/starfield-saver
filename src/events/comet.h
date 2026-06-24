#pragma once
// A comet streaks diagonally across the field with a glowing head and a fading
// tail. This is the first concrete event — kept simple to validate the whole
// scheduler -> event -> render pipeline. More elaborate events (planets, ships,
// wormholes, supernovae) follow the same EventBase pattern.

#include "event_base.h"

namespace ssaver {

class Comet : public EventBase {
public:
    Comet(int width, int height);

    void render(const RenderContext& ctx) override;
    const char* name() const override { return "comet"; }

private:
    int width_, height_;
    float startX_, startY_;   // entry point (off-screen)
    float endX_, endY_;       // exit point (off-screen, opposite side)
    Uint8 r_, g_, b_;
};

} // namespace ssaver
