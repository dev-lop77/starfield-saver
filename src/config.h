#pragma once
// Tunable constants for the whole screensaver. Kept in one place so the retro
// "feel" (resolution, palette weight, pacing) is easy to dial in.

namespace ssaver {
namespace cfg {

// --- Internal render resolution ----------------------------------------------
// Everything is drawn into a low-res target, then upscaled (nearest-neighbour)
// to the display. Rather than a fixed size, the target is the display's native
// resolution divided by kRenderDownscale — so the chunkiness is consistent on
// any monitor (2 => render at half native, a light retro chunk that still looks
// crisp on modern high-res panels). Larger value = chunkier + cheaper.
constexpr int kRenderDownscale = 1;

// Fallback target size, used only if the live output size can't be queried
// (e.g. the headless dummy video driver). Keeps a 16:9 aspect.
constexpr int kLowWidth  = 480;
constexpr int kLowHeight = 270;

// --- Development window -------------------------------------------------------
// Size of the window when running on the dev machine (Linux). The real
// screensaver goes fullscreen on every monitor instead.
constexpr int kDevWindowWidth  = 1280;
constexpr int kDevWindowHeight = 720;

// --- Starfield (depth-parallax model) ----------------------------------------
// Stars are scattered across the whole screen at random depths and drift slowly
// outward — a calm "cruising forward" parallax rather than a firework burst from
// the centre. Near stars are bright/large/faster; far stars are faint/slow
// points. There is no 1/z perspective, so nothing accelerates as it nears the
// edge. All easy to dial in.
constexpr int   kStarCount     = 1200;   // target count at ~1080p; scaled by area
constexpr float kStarSpeed     = 0.10f;  // depth-1 speed, as a fraction of the
                                         // smaller screen dimension, per second
constexpr float kStarDepthMin  = 0.08f;  // floor on the 0..1 depth (far stars
                                         // still creep instead of freezing)
constexpr float kStreakSeconds = 0.05f;  // motion-trail length, in seconds of
                                         // travel (bigger = longer streaks)

} // namespace cfg
} // namespace ssaver
