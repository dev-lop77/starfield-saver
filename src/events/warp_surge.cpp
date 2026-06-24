#include "warp_surge.h"

#include "../starfield.h"

namespace ssaver {

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kPeakWarp = 4.0f;  // top speed multiplier at the peak of the surge
}

WarpSurge::WarpSurge(Starfield* field)
    : EventBase(/*enter=*/3.0f, /*hold=*/1.5f, /*exit=*/3.0f), field_(field) {}

WarpSurge::~WarpSurge() {
    // Always leave the field at its normal cruise, even if reaped early or if a
    // surge overlaps another.
    if (field_) field_->setWarp(1.0f);
}

void WarpSurge::update(float dt) {
    EventBase::update(dt);
    if (!field_) return;
    // Smooth rise-and-fall pulse: 0 at both ends, 1 in the middle — so the field
    // accelerates up to kPeakWarp and eases back to 1.0 on its own.
    float pulse = SDL_sinf(kPi * lifeT());
    field_->setWarp(1.0f + (kPeakWarp - 1.0f) * pulse);
}

} // namespace ssaver
