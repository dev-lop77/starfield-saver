# Starfield Saver

A retro-flavoured Windows screensaver. Most of the time it is the classic
fly-through-space starfield — a calm depth-parallax drift where near stars are
bright and faster while distant ones are faint and slow. Every so often a
cinematic *event* drifts past — comets, planets, satellites, original sci-fi
starships, and shader-drawn deep-space phenomena (wormholes, supernovae, spiral
galaxies and translucent nebulae), each randomised in colour and size.

Built in C++17 with SDL2. The scene renders to a low-resolution target that is
upscaled with nearest-neighbour filtering for crisp, chunky retro pixels. An
optional CRT post-process pass (curvature, scanlines, aperture grille, chromatic
aberration, vignette, glow, flicker) can be applied on top via a GLSL shader —
off by default, enabled with `crt = 1` in the settings file (see below). The
internal resolution is the display's native size divided by
`cfg::kRenderDownscale` (default `1` = full native; raise it for chunkier
pixels), so the look is consistent on any monitor.

Developed and run natively on Linux; the same code cross-compiles to a Windows
`.scr` with MinGW-w64. **Confirmed working on Windows 11 (Intel Iris Xe).**

## Layout

```
src/
  main.cpp            entry point (dev window on Linux, .scr args on Windows)
  screensaver_win.cpp Win32 shell: /s fullscreen, /p preview, /c config
  app.{h,cpp}         core: window, low-res target, game loop, upscale, exit
  scene.h             Scene interface (update/render/finished)
  starfield.{h,cpp}   the persistent warp starfield
  event_scheduler.*   picks weighted-random events at random intervals
  events/
    event_base.h      enter/hold/exit lifetime + fade envelope
    comet.{h,cpp}     first concrete event (pipeline proof)
  postfx_crt.{h,cpp}  CRT shader post-process (raw GL via SDL_GL_GetProcAddress)
  config.h            tunables (render downscale, star count, speed)
  util.h              RNG + math helpers
shaders/crt.frag      reference copy of the CRT shader (embedded in postfx_crt)
```

## Building

### Linux (development)

Requires `libsdl2-dev`.

```
make            # -> build/starfield-saver
make run        # build and open the dev window
make audition   # cycle through every event back-to-back (eye review)
./build/starfield-saver --fullscreen   # preview the real presentation
```

Press **ESC** (or move the mouse in fullscreen) to quit. In the dev window,
press **c** to toggle the CRT effect for a side-by-side comparison. In audition
mode, press **N** / **Right** to skip to the next event (handy for the longer
ones); the current event's name is printed to the console.

### Windows screensaver (.scr), cross-compiled from Linux

Requires `g++-mingw-w64-x86-64`. One-time: fetch the SDL2 mingw libraries.

```
make sdl2-mingw     # downloads SDL2 devel libs into third_party/ (no sudo)
make windows        # -> build/StarfieldSaver.scr  (+ build/SDL2.dll)
```

Ship `StarfieldSaver.scr` together with `SDL2.dll` (keep them in the same folder).

## Installing & testing on Windows

The screensaver responds to the standard arguments: `/s` (run fullscreen),
`/p <HWND>` (preview pane), `/c` (configuration — currently a placeholder).

- **Quick test (no install):** run `StarfieldSaver.scr /s` from a command prompt,
  or right-click the `.scr` in Explorer → **Test**. Move the mouse / press a key
  to exit. (Double-clicking only opens the config box.)
- **Eye review (audition):** set `audition = 1` in `starfield-saver.ini` (see
  below), then launch the saver normally (right-click the `.scr` → **Test**, or
  *Settings → Screen saver → Preview*). It plays every event fullscreen, one at a
  time, in order — the four shader effects (wormhole, supernova, galaxy, nebula)
  come last. Press **N** / **Right** to skip to the next, **ESC** to quit. Input
  doesn't exit in this mode, so you can step through at your own pace. Set
  `audition = 0` again for normal screensaver behaviour. (No command-line flag is
  used because a `.scr` can't be passed arguments through the Windows shell.)
- **Install:** right-click the `.scr` → **Install**, or drop it in
  `C:\Windows\System32` and pick it in *Settings → Lock screen → Screen saver*.

On a multi-monitor setup the starfield animates on the primary display and the
other monitors are blacked out.

## Configuration (no rebuild needed)

Drop a `starfield-saver.ini` next to `StarfieldSaver.scr` (see
`starfield-saver.ini.example`) and restart the screensaver. If the `.scr` lives
in a non-writable folder, put the file in
`%APPDATA%\ssaver\StarfieldSaver\starfield-saver.ini` instead.

```ini
crt = 1        # turn the CRT effect on (default is off)
downscale = 1  # render = native / this: 1 crisp, 2 softer, 3 chunky pixels
audition = 0   # 1 = eye-review mode: cycle every event in order (N/Right skip)
```

Unknown or missing keys fall back to the built-in defaults.

> Note: there is a one-time ~1s warm-up on first launch (the GPU driver compiles
> the CRT shader on first use). It happens once, off-screen while you're away, so
> it isn't noticeable in normal screensaver use.

## Roadmap

- [x] Core engine, low-res render + upscale, dev window
- [x] Warp starfield
- [x] Event scheduler + lifetime framework + events: comet, star burst
      (temporary denser star region), warp surge (brief speed/trail jump),
      screen crash (rare: star hits the screen — flash + cracked glass that heals),
      satellite (Voyager-style pixel-art probe you fly past),
      planet (procedurally shaded pixel-art planet pass — varied terrain),
      starship (engine cluster warping off toward the vanishing point)
- [x] Windows `.scr` shell (`/s`, `/p`, `/c`)
- [x] CRT post-process shader (curvature, scanlines, grille, aberration, glow)
- [x] Adaptive native-resolution rendering (`kRenderDownscale`)
- [x] Multi-monitor: animate primary, black out the rest
- [x] Confirmed working on real Windows 11
- [x] Embedded pixel-art sprite system (char-grid + palette, no asset files)
- [x] Pixel-art events: satellite, planet, starship
- [x] Reusable GLSL event system (raw-GL shader overlay, software-renderer safe)
- [x] Rare GLSL events: wormhole, supernova, spiral galaxy, translucent nebula
      (each randomises colour and size per appearance)
- [ ] Configuration dialog (density, speed, event frequency, CRT intensity)
```
