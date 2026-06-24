#pragma once
// Decides when cinematic events happen. Most of the time nothing extra is on
// screen — just the starfield. Every so often the scheduler picks one event
// from a weighted roster (common: comets/satellites/planets/ships;
// rare: wormhole/supernova/galaxy) and runs it to completion.

#include <functional>
#include <memory>
#include <vector>

#include "scene.h"

namespace ssaver {

class Starfield;

class EventScheduler {
public:
    EventScheduler(int width, int height, Starfield* starfield);

    void update(float dt);
    void render(const RenderContext& ctx);

    // Terminal raw-GL pass for the rare GLSL events. Called by App after the
    // scene has been composited to the window, just before present.
    bool hasGLEvents() const;
    void renderGL(const GLRenderContext& ctx);

    // Dev "audition" mode: instead of weighted-random spawns, play every event
    // in the roster once, in order, back-to-back (one on screen at a time), then
    // loop. The active event's name is printed to the console.
    void setAudition(bool on);
    void auditionNext();  // drop the current event and jump to the next now

private:
    // A spawnable event: relative weight + factory.
    struct EventDef {
        const char* name;
        float weight;
        std::function<std::unique_ptr<Scene>()> make;
    };

    void scheduleNext();    // pick the delay until the next event
    void spawn();           // instantiate a weighted-random event
    void spawnAudition();   // instantiate the next roster entry in order

    int width_, height_;
    Starfield* starfield_;  // not owned; events may nudge warp etc.

    std::vector<EventDef> roster_;
    std::vector<std::unique_ptr<Scene>> active_;

    float timeToNext_ = 0.0f;  // countdown to the next spawn

    bool   audition_ = false;
    size_t auditionIdx_ = 0;   // next roster entry to play in audition mode
};

} // namespace ssaver
