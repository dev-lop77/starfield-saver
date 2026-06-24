#pragma once
// The application core: owns the SDL window/renderer, the low-resolution render
// target, the game loop, and the active scenes. Platform-independent — the
// Windows screensaver shell (and the Linux dev entry point) just construct an
// App and call run().

#include <SDL.h>
#include <memory>
#include <vector>

#include "scene.h"

namespace ssaver {

class Starfield;
class EventScheduler;
class CrtEffect;

struct AppConfig {
    bool fullscreen = false;       // true for the real screensaver (/s)
    bool hideCursor = false;       // hide the mouse pointer
    bool exitOnInput = false;      // quit on any mouse/key activity (screensaver)
    bool audition = false;         // dev: cycle through every event back-to-back
    SDL_Window* existingWindow = nullptr;  // for /p preview: wrap an existing HWND
};

class App {
public:
    explicit App(const AppConfig& config);
    ~App();

    // Initialise SDL, window, renderer, low-res target and scenes.
    // Returns false on failure (message already logged).
    bool init();

    // Run the game loop until the user quits / input is detected. Returns the
    // process exit code.
    int run();

private:
    void handleEvents();
    void update(float dt);
    void render();
    void createBlackoutWindows();  // cover non-primary monitors with black
    void paintBlackoutWindows();   // fill them black (once / on expose)

    AppConfig cfg_;
    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture*  lowResTarget_ = nullptr;  // native/kRenderDownscale render target
    int lowW_ = 0;  // low-res target width  (display native / kRenderDownscale)
    int lowH_ = 0;  // low-res target height

    // On a multi-monitor setup the screensaver only animates the primary
    // display; every other monitor is covered by an always-on-top black window
    // so the desktop wallpaper never shows through. We paint these via their
    // window surface (no second renderer — that was slow to create and fought
    // with the GL context). Empty in dev/preview modes.
    std::vector<SDL_Window*> blackWindows_;

    bool running_ = false;
    Uint64 lastTicks_ = 0;
    double elapsed_ = 0.0;

    std::unique_ptr<Starfield> starfield_;
    std::unique_ptr<EventScheduler> scheduler_;
    std::unique_ptr<CrtEffect> crt_;
    bool crtEnabled_ = false;  // default off; set by settings, 'c' toggles in dev

    // Input baseline so we can ignore the tiny mouse jitter that fires on
    // startup and only quit on genuine user activity.
    int  startMouseX_ = 0;
    int  startMouseY_ = 0;
    bool inputBaselineSet_ = false;
};

} // namespace ssaver
