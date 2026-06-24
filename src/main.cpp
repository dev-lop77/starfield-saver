// Entry point.
//
//  * On Windows the binary is renamed to .scr and is launched by the OS with:
//      /s            run the screensaver fullscreen
//      /p <HWND>     render a small preview inside the Display Settings dialog
//      /c[:<HWND>]   show the configuration dialog
//    (Windows-specific plumbing lives in screensaver_win.cpp.)
//
//  * On Linux (the dev environment) we just open a normal window so the visuals
//    can be built and tested locally.

#include <cstring>
#include <string>

#include "app.h"

#if defined(_WIN32)
// Implemented in screensaver_win.cpp — parses the .scr arguments and runs the
// appropriate mode (fullscreen / preview / config).
int RunWindowsScreensaver(int argc, char** argv);
#endif

namespace {

#if !defined(_WIN32)
int RunDev(int argc, char** argv) {
    using namespace ssaver;
    AppConfig cfg;
    // Allow "--fullscreen" on the dev build to eyeball the real presentation.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--fullscreen") == 0) {
            cfg.fullscreen = true;
            cfg.hideCursor = true;
            cfg.exitOnInput = true;
        }
    }
    App app(cfg);
    if (!app.init()) return 1;
    return app.run();
}
#endif // !_WIN32

} // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
    return RunWindowsScreensaver(argc, argv);
#else
    return RunDev(argc, argv);
#endif
}
