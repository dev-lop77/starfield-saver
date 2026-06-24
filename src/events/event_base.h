#pragma once
// Base class for transient cinematic events. Handles the common lifetime:
//   enter  -> fade/scale in
//   hold   -> full presence
//   exit   -> fade/scale out  -> finished()
//
// Concrete events (planet, ship, satellite, wormhole, supernova, ...) subclass
// this and use alpha() / phaseT() to drive their visuals, so they all share a
// consistent, non-jarring appearance and disappearance.

#include "../scene.h"
#include "../util.h"

namespace ssaver {

class EventBase : public Scene {
public:
    EventBase(float enter, float hold, float exit)
        : enter_(enter), hold_(hold), exit_(exit) {}

    void update(float dt) override { age_ += dt; }

    bool finished() const override { return age_ >= enter_ + hold_ + exit_; }

protected:
    // Overall opacity in [0,1] following the enter/hold/exit envelope.
    float alpha() const {
        if (age_ < enter_) return smoothstep(age_ / enter_);
        if (age_ < enter_ + hold_) return 1.0f;
        float t = (age_ - enter_ - hold_) / exit_;
        return 1.0f - smoothstep(t);
    }

    // Normalised progress across the whole lifetime, [0,1]. Useful for events
    // that travel across the screen regardless of fade.
    float lifeT() const {
        float total = enter_ + hold_ + exit_;
        return total > 0.0f ? clampf(age_ / total, 0.0f, 1.0f) : 1.0f;
    }

    float age_ = 0.0f;
    float enter_, hold_, exit_;
};

} // namespace ssaver
