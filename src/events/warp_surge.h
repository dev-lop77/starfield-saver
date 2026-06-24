#pragma once
// A temporary warp surge: smoothly accelerates the whole starfield (faster drift
// and, since the trail length scales with warp, longer streaks) up to a peak,
// then eases back to the normal cruise — a brief "jump to lightspeed". It drives
// the shared Starfield via setWarp() and draws nothing of its own.

#include "event_base.h"

namespace ssaver {

class Starfield;

class WarpSurge : public EventBase {
public:
    explicit WarpSurge(Starfield* field);
    ~WarpSurge() override;

    void update(float dt) override;
    void render(const RenderContext&) override {}  // affects the field, not pixels
    const char* name() const override { return "warpsurge"; }

private:
    Starfield* field_;  // not owned
};

} // namespace ssaver
