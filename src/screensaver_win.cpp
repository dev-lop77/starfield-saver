// Windows screensaver shell. Compiled only on Windows (via the mingw target).
// Parses the standard .scr command line and launches the App in the right mode.
//
//   <prog> /s            fullscreen screensaver
//   <prog> /p <HWND>     preview inside the Display Settings dialog
//   <prog> /c[:<HWND>]   configuration dialog
//   <prog>               (no args) — treated as configuration
//
// There's no audition command-line flag here on purpose: a .scr can't be passed
// arguments through the Windows shell. Eye-review mode is enabled from the .ini
// instead (audition = 1), which the normal /s launch picks up (see settings.h /
// App::init).
//
// The actual rendering is platform-independent (App, in app.cpp); this file is
// only the Win32 glue.

#if defined(_WIN32)

#include <windows.h>
#include <SDL.h>
#include <cstdlib>
#include <cstring>

#include "app.h"
#include "config.h"

namespace {

// Parse an HWND that may be passed in decimal or hex on the command line.
HWND ParseHwnd(const char* s) {
    if (!s) return nullptr;
    // Windows passes the handle as a plain (possibly very large) integer.
    unsigned long long v = std::strtoull(s, nullptr, 0);
    return reinterpret_cast<HWND>(static_cast<uintptr_t>(v));
}

int RunFullscreen() {
    using namespace ssaver;
    AppConfig cfg;
    cfg.fullscreen = true;
    cfg.hideCursor = true;
    cfg.exitOnInput = true;
    App app(cfg);
    if (!app.init()) return 1;
    return app.run();
}

// Render the live screensaver inside the tiny preview pane. We wrap the HWND the
// OS hands us as an SDL window and run the same engine; input handling is off
// because the preview must not capture the user's mouse/keyboard.
int RunPreview(HWND parent) {
    using namespace ssaver;
    if (!parent || !IsWindow(parent)) return 0;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    SDL_Window* win = SDL_CreateWindowFrom(reinterpret_cast<const void*>(parent));
    if (!win) { SDL_Quit(); return 1; }

    AppConfig cfg;
    cfg.existingWindow = win;
    cfg.exitOnInput = false;  // never quit from preview activity
    App app(cfg);
    if (!app.init()) return 1;

    // The App loop exits on SDL_QUIT; in the preview, that arrives when the
    // parent dialog closes and the child window is destroyed.
    return app.run();
}

int RunConfig() {
    // Placeholder configuration UI. A proper dialog (star density, warp speed,
    // event frequency, CRT intensity) will replace this.
    MessageBoxW(nullptr,
                L"Starfield Saver\n\nConfiguration coming soon.",
                L"Starfield Saver", MB_OK | MB_ICONINFORMATION);
    return 0;
}

} // namespace

int RunWindowsScreensaver(int argc, char** argv) {
    // Default (double-clicked with no args) -> configuration.
    if (argc < 2) return RunConfig();

    // Arguments look like "/s", "/p", "/c" or "/c:1234"; the flag may use a
    // colon or pass the HWND as the next argv entry.
    const char* arg = argv[1];
    char flag = (arg[0] == '/' || arg[0] == '-') ? arg[1] : arg[0];

    switch (flag) {
        case 's': case 'S':
            return RunFullscreen();
        case 'p': case 'P': {
            // HWND may be argv[2] or embedded as "/p:1234".
            const char* h = (arg[2] == ':') ? arg + 3
                          : (argc >= 3 ? argv[2] : nullptr);
            return RunPreview(ParseHwnd(h));
        }
        case 'c': case 'C':
        default:
            return RunConfig();
    }
}

#endif // _WIN32
