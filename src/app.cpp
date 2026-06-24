#include "app.h"

#include <algorithm>  // std::min, std::max
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>  // TerminateProcess on the fast-exit path
#endif

#include "config.h"
#include "settings.h"
#include "starfield.h"
#include "event_scheduler.h"
#include "postfx_crt.h"
#include "gl_shader.h"

namespace ssaver {

App::App(const AppConfig& config) : cfg_(config) {}

App::~App() {
    for (SDL_Window* w : blackWindows_) if (w) SDL_DestroyWindow(w);
    if (lowResTarget_) SDL_DestroyTexture(lowResTarget_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    // Only destroy the window if we created it (preview wraps an existing one).
    if (window_ && !cfg_.existingWindow) SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool App::init() {
    // Crisp, nearest-neighbour upscaling — essential for the chunky retro look.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    // Prefer the OpenGL backend so the CRT post-process shader can run. Falls
    // back gracefully if unavailable.
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    // Multi-monitor: by default SDL minimises a fullscreen window as soon as it
    // loses focus. When we create the black-out windows on the other monitors
    // they steal focus, the starfield window would minimise and stop presenting.
    // Keeping this disabled keeps the saver visible.
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    // Optional runtime overrides from starfield-saver.ini (so the look can be
    // changed on a machine with no build toolchain).
    const Settings settings = loadSettings();
    crtEnabled_ = settings.crt;
    // Audition can also be turned on from the .ini (handy on the Windows .scr,
    // which can't take a command-line flag through the shell). When on, force
    // exit-on-input off so N/Right can step between events; ESC still quits.
    if (settings.audition) {
        cfg_.audition = true;
        cfg_.exitOnInput = false;
    }
    // Clamp to a sane range: an out-of-range value (e.g. from a garbled .ini)
    // must not overflow the render-target size maths or collapse it to 1x1.
    int downscale =
        settings.downscale > 0 ? settings.downscale : cfg::kRenderDownscale;
    downscale = std::max(1, std::min(downscale, 16));

    if (cfg_.existingWindow) {
        window_ = cfg_.existingWindow;
    } else if (cfg_.fullscreen) {
        // A plain borderless, always-on-top window sized to the primary monitor
        // — NOT SDL_WINDOW_FULLSCREEN_DESKTOP, whose DWM mode transition is slow
        // to enter and to dismiss on some hardware (notably Intel on Windows).
        // A borderless window at the monitor's bounds covers the screen
        // identically but appears and closes instantly. No SDL_WINDOW_OPENGL
        // here — SDL_CreateRenderer adds it for the GL backend, and omitting it
        // keeps the software-renderer fallback (and headless dummy driver) working.
        SDL_Rect b{0, 0, 0, 0};
        Uint32 flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP |
                       SDL_WINDOW_ALLOW_HIGHDPI;
        if (SDL_GetDisplayBounds(0, &b) == 0 && b.w > 0 && b.h > 0) {
            window_ = SDL_CreateWindow("Starfield Saver", b.x, b.y, b.w, b.h,
                                       flags);
        } else {
            // Fallback if bounds are unavailable: real fullscreen-desktop.
            window_ = SDL_CreateWindow(
                "Starfield Saver", SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED, 0, 0,
                flags | SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
    } else {
        window_ = SDL_CreateWindow(
            "Starfield Saver", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            cfg::kDevWindowWidth, cfg::kDevWindowHeight,
            SDL_WINDOW_ALLOW_HIGHDPI);
    }
    if (!window_) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    // NOTE: deliberately no SDL_RENDERER_PRESENTVSYNC. When the window loses
    // focus (e.g. to the black-out window on another monitor) the compositor
    // throttles a vsync'd swap to ~1-2 Hz and many GL drivers *spin-wait* for
    // it, pegging a CPU core. We pace the loop with an explicit frame limiter
    // in run() instead.
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer_) {
        // Fall back to the software renderer (headless machines, no GPU accel).
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer_) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    // Size the low-res target to the display's native resolution divided by
    // kRenderDownscale, so the retro chunkiness is consistent on any monitor.
    // Fall back to the config size if the output size can't be queried (headless).
    int outW = 0, outH = 0;
    SDL_GetRendererOutputSize(renderer_, &outW, &outH);
    lowW_ = outW > 0 ? (outW + downscale - 1) / downscale : cfg::kLowWidth;
    lowH_ = outH > 0 ? (outH + downscale - 1) / downscale : cfg::kLowHeight;
    if (lowW_ < 1) lowW_ = 1;
    if (lowH_ < 1) lowH_ = 1;

    // Low-res target: everything is drawn here, then blitted up to the window.
    // Nearest-neighbour scaling keeps the pixels crisp and retro.
    lowResTarget_ = SDL_CreateTexture(
        renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
        lowW_, lowH_);
    if (!lowResTarget_) {
        // Some drivers refuse a large render-target texture; fall back to the
        // safe fixed size rather than aborting the whole screensaver.
        lowW_ = cfg::kLowWidth;
        lowH_ = cfg::kLowHeight;
        lowResTarget_ = SDL_CreateTexture(
            renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
            lowW_, lowH_);
    }
    if (!lowResTarget_) {
        std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }

    if (cfg_.hideCursor) SDL_ShowCursor(SDL_DISABLE);

    // Set up the CRT post-process, but only if we actually got a GL renderer.
    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer_, &info) == 0 &&
        std::strcmp(info.name, "opengl") == 0) {
        crt_ = std::make_unique<CrtEffect>();
        if (!crt_->init()) {
            crt_.reset();  // shader unavailable; fall back to plain upscale
        }
    }

    starfield_ = std::make_unique<Starfield>(lowW_, lowH_);
    scheduler_ = std::make_unique<EventScheduler>(lowW_, lowH_, starfield_.get());
    if (cfg_.audition) scheduler_->setAudition(true);

    // Black out every monitor other than the one we animate on, so a
    // multi-monitor desktop doesn't reveal its wallpaper next to the saver.
    if (cfg_.fullscreen && !cfg_.existingWindow) {
        createBlackoutWindows();
        paintBlackoutWindows();
        // The black-out windows were created after the main window and stole its
        // focus; give it back so the compositor keeps presenting it at full rate.
        SDL_RaiseWindow(window_);
    }

    lastTicks_ = SDL_GetPerformanceCounter();
    running_ = true;
    return true;
}

void App::createBlackoutWindows() {
    int mainDisplay = SDL_GetWindowDisplayIndex(window_);
    int displays = SDL_GetNumVideoDisplays();
    for (int i = 0; i < displays; ++i) {
        if (i == mainDisplay) continue;
        SDL_Rect b;
        if (SDL_GetDisplayBounds(i, &b) != 0) continue;
        // Borderless, always-on-top window placed exactly over the monitor's
        // bounds. ALWAYS_ON_TOP stops the desktop/explorer from repainting over
        // it. (Not FULLSCREEN_DESKTOP: that ignores the position and could land
        // on the wrong display.)
        SDL_Window* bw = SDL_CreateWindow(
            "Starfield Saver", b.x, b.y, b.w, b.h,
            SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP);
        if (bw) blackWindows_.push_back(bw);
    }
}

void App::paintBlackoutWindows() {
    for (SDL_Window* bw : blackWindows_) {
        if (!bw) continue;
        // Fill the window's own surface black — no renderer/GL involved, so it
        // can't stall or contend with the main OpenGL context.
        SDL_Surface* s = SDL_GetWindowSurface(bw);
        if (!s) continue;
        SDL_FillRect(s, nullptr, SDL_MapRGB(s->format, 0, 0, 0));
        SDL_UpdateWindowSurface(bw);
    }
}

void App::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                running_ = false;
                break;
            case SDL_KEYDOWN:
                // ESC always quits, even in dev mode.
                if (e.key.keysym.sym == SDLK_ESCAPE) running_ = false;
                // 'c' toggles the CRT filter for side-by-side comparison (dev).
                else if (e.key.keysym.sym == SDLK_c && !cfg_.exitOnInput) {
                    crtEnabled_ = !crtEnabled_;
                }
                // Audition: N / Right skips to the next event (dev only).
                else if ((e.key.keysym.sym == SDLK_n ||
                          e.key.keysym.sym == SDLK_RIGHT) &&
                         !cfg_.exitOnInput && cfg_.audition) {
                    scheduler_->auditionNext();
                }
                if (cfg_.exitOnInput) running_ = false;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (cfg_.exitOnInput) running_ = false;
                break;
            case SDL_WINDOWEVENT:
                // A blacked-out monitor was exposed / shown / focus changed —
                // re-fill it so the wallpaper can't flash through.
                switch (e.window.event) {
                    case SDL_WINDOWEVENT_EXPOSED:
                    case SDL_WINDOWEVENT_SHOWN:
                    case SDL_WINDOWEVENT_RESTORED:
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        paintBlackoutWindows();
                        break;
                    default:
                        break;
                }
                break;
            case SDL_MOUSEMOTION:
                if (cfg_.exitOnInput) {
                    // Ignore the first jitter; quit only on real movement.
                    if (!inputBaselineSet_) {
                        startMouseX_ = e.motion.x;
                        startMouseY_ = e.motion.y;
                        inputBaselineSet_ = true;
                    } else if (SDL_abs(e.motion.x - startMouseX_) > 6 ||
                               SDL_abs(e.motion.y - startMouseY_) > 6) {
                        running_ = false;
                    }
                }
                break;
            default:
                break;
        }
    }
}

void App::update(float dt) {
    starfield_->update(dt);
    scheduler_->update(dt);
}

void App::render() {
    // 1) Draw the scene into the low-res target.
    SDL_SetRenderTarget(renderer_, lowResTarget_);
    // Make the frame's blend state explicit: events (comet, star burst) switch
    // the renderer to BLEND for their fades and don't restore it, so reset here
    // before the opaque starfield draws thousands of stars.
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 8, 255);  // near-black space
    SDL_RenderClear(renderer_);

    RenderContext ctx{renderer_, lowW_, lowH_, elapsed_};
    starfield_->render(ctx);
    scheduler_->render(ctx);

    // 2) Compose the low-res scene onto the window.
    SDL_SetRenderTarget(renderer_, nullptr);
    int outW = 0, outH = 0;
    SDL_GetRendererOutputSize(renderer_, &outW, &outH);
    if (crt_ && crtEnabled_) {
        // CRT shader pass: samples the low-res target and draws the curved,
        // scanlined image straight to the window framebuffer.
        crt_->apply(renderer_, lowResTarget_, outW, outH,
                    lowW_, lowH_, static_cast<float>(elapsed_));
    } else {
        // Plain nearest-neighbour upscale (CRT off / unavailable).
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, lowResTarget_, nullptr, nullptr);
    }

    // 3) Rare GLSL events composite over the final image as a terminal raw-GL
    //    overlay. This MUST be the last GPU work before present: raw GL leaves
    //    SDL's renderer-state cache stale, so nothing else may draw afterwards
    //    (same constraint as the CRT pass). Skipped on the software renderer.
    if (scheduler_->hasGLEvents() && glsl::available()) {
        SDL_RenderFlush(renderer_);  // flush SDL's queued draws first
        GLRenderContext g{renderer_, outW, outH, elapsed_};
        scheduler_->renderGL(g);
    }

    SDL_RenderPresent(renderer_);  // swaps the buffer we drew into
}

int App::run() {
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    const double targetFrame = 1.0 / 60.0;  // cap at ~60 FPS

    while (running_) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(static_cast<double>(now - lastTicks_) / freq);
        lastTicks_ = now;
        // Clamp dt so a stall (e.g. window drag) doesn't teleport everything.
        if (dt > 0.1f) dt = 0.1f;
        elapsed_ += dt;

        handleEvents();
        update(dt);
        render();

        // Frame limiter. With vsync off, present returns immediately; without a
        // cap the loop would peg a CPU core. Sleep the remainder of the frame
        // budget to stay near 60 FPS.
        double frameSec = static_cast<double>(SDL_GetPerformanceCounter() - now) / freq;
        if (frameSec < targetFrame) {
            SDL_Delay(static_cast<Uint32>((targetFrame - frameSec) * 1000.0));
        }
    }

    // Dismiss the screensaver the instant the user touches anything. An orderly
    // GL/SDL teardown is slow on some drivers (Intel on Windows runs the GL
    // driver's DLL_PROCESS_DETACH on exit), leaving the screen black for a
    // moment. The process is about to die anyway, so terminate it immediately —
    // the OS reclaims every resource at once. Only for the real fullscreen
    // screensaver; dev/preview exit normally so destructors and the preview pane
    // behave.
    if (cfg_.fullscreen && !cfg_.existingWindow) {
#if defined(_WIN32)
        TerminateProcess(GetCurrentProcess(), 0);
#else
        std::_Exit(0);
#endif
    }

    return 0;
}

} // namespace ssaver
