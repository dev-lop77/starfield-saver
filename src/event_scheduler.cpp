#include "event_scheduler.h"

#include "starfield.h"
#include "util.h"
#include "events/comet.h"
#include "events/planet.h"
#include "events/satellite.h"
#include "events/screen_crash.h"
#include "events/star_burst.h"
#include "events/starship.h"
#include "events/warp_surge.h"
#include "events/wormhole.h"

namespace ssaver {

EventScheduler::EventScheduler(int width, int height, Starfield* starfield)
    : width_(width), height_(height), starfield_(starfield) {
    // The roster. For now only the comet is implemented; the remaining entries
    // are the planned line-up and will be wired in as each event lands.
    roster_.push_back({"comet", 1.0f,
                       [this] { return std::make_unique<Comet>(width_, height_); }});
    roster_.push_back({"starburst", 0.8f,
                       [this] { return std::make_unique<StarBurst>(width_, height_); }});
    roster_.push_back({"warpsurge", 0.7f,
                       [this] { return std::make_unique<WarpSurge>(starfield_); }});
    roster_.push_back({"satellite", 0.3f,
                       [this] { return std::make_unique<Satellite>(width_, height_); }});
    roster_.push_back({"planet", 0.5f,
                       [this] { return std::make_unique<Planet>(width_, height_); }});
    roster_.push_back({"starship", 0.25f,
                       [this] { return std::make_unique<Starship>(width_, height_); }});
    roster_.push_back({"screencrash", 0.15f,  // rare
                       [this] { return std::make_unique<ScreenCrash>(width_, height_); }});
    roster_.push_back({"wormhole", 0.18f,  // rare GLSL event
                       [this] { return std::make_unique<Wormhole>(width_, height_); }});
    // TODO: supernova, galaxy, nebula (rare GLSL events)

    scheduleNext();
}

void EventScheduler::scheduleNext() {
    // Mostly quiet sky: wait somewhere between a few seconds and ~half a minute
    // before the next event. Tunable as the roster grows.
    timeToNext_ = frand(6.0f, 22.0f);
}

void EventScheduler::spawn() {
    if (roster_.empty()) return;

    float total = 0.0f;
    for (const auto& e : roster_) total += e.weight;
    float pick = frand(0.0f, total);
    for (const auto& e : roster_) {
        if (pick < e.weight) {
            active_.push_back(e.make());
            return;
        }
        pick -= e.weight;
    }
}

void EventScheduler::update(float dt) {
    timeToNext_ -= dt;
    if (timeToNext_ <= 0.0f) {
        spawn();
        scheduleNext();
    }

    for (auto& ev : active_) ev->update(dt);

    // Reap finished events.
    for (size_t i = 0; i < active_.size();) {
        if (active_[i]->finished()) {
            active_.erase(active_.begin() + i);
        } else {
            ++i;
        }
    }
}

void EventScheduler::render(const RenderContext& ctx) {
    for (auto& ev : active_) ev->render(ctx);
}

bool EventScheduler::hasGLEvents() const {
    for (const auto& ev : active_)
        if (ev->usesGL()) return true;
    return false;
}

void EventScheduler::renderGL(const GLRenderContext& ctx) {
    for (auto& ev : active_)
        if (ev->usesGL()) ev->renderGL(ctx);
}

} // namespace ssaver
