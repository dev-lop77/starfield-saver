#include "wormhole.h"

#include "../gl_shader.h"
#include "../util.h"

namespace ssaver {

// The whole look lives in this fragment shader. It runs as a fullscreen additive
// overlay (GLSL 1.20, to match SDL's legacy GL context). vUV is 0..1 across the
// window; uniforms drive the portal's position, size, swirl and fade.
static const char* kWormholeFrag = R"GLSL(
#version 120
varying vec2 vUV;
uniform vec2  uRes;     // window size (for aspect)
uniform float uTime;    // seconds (continuous animation)
uniform float uAlpha;   // envelope fade 0..1
uniform vec2  uCenter;  // portal centre in 0..1 UV
uniform float uRadius;  // portal radius as fraction of min screen dim
uniform float uSpin;    // swirl direction/speed
uniform float uHue;     // 0..1 colour bias

// Cheap value noise for tunnel turbulence.
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float a = hash(i), b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0)), d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main() {
    // Aspect-correct coordinates centred on the portal; q is radius in units of
    // the portal radius (q = 1 at the rim).
    vec2 p = vUV - uCenter;
    p.x *= uRes.x / uRes.y;
    float r = length(p);
    float ang = atan(p.y, p.x);
    float q = r / max(uRadius, 0.001);

    // Tunnel depth coordinate: small near the rim, large toward the centre, and
    // forever rushing inward (uTime) so it reads as falling in.
    float depth = 1.0 / (q + 0.16);
    float spiral = ang * 2.0 + uSpin * (depth * 2.2 + uTime * 1.3);

    // Layered turbulence streaming along the tunnel.
    float turb = noise(vec2(spiral, depth * 3.0 - uTime * 2.2));
    turb = 0.6 * turb + 0.4 * noise(vec2(spiral * 2.0, depth * 6.0 - uTime * 3.6));
    float swirl = 0.5 + 0.5 * sin(spiral + turb * 3.0);

    // Brightness profile: glowing tunnel walls in a mid annulus, a hot core, and
    // a sharp rim ring; everything dies out past ~3.5 radii so it isn't a wash.
    float walls = smoothstep(3.4, 0.5, q) * smoothstep(0.0, 0.30, q);
    float core  = exp(-q * q * 2.6);
    float rim   = exp(-pow((q - 1.0) * 3.6, 2.0));
    float reach = smoothstep(3.6, 0.0, q);

    // Colour: blue -> violet by uHue, with a teal shimmer from the turbulence.
    vec3 cool   = vec3(0.16, 0.36, 1.00);
    vec3 violet = vec3(0.72, 0.26, 1.00);
    vec3 teal   = vec3(0.10, 0.92, 0.82);
    vec3 base   = mix(cool, violet, uHue);
    base        = mix(base, teal, 0.30 * turb);

    vec3 col  = base * walls * (0.45 + 0.95 * swirl);
    col      += vec3(0.80, 0.90, 1.00) * core * core * 1.4;     // white-hot throat
    col      += mix(base, vec3(1.0), 0.5) * rim * 1.6;          // bright rim ring

    col *= reach * uAlpha;
    gl_FragColor = vec4(col, 1.0);
}
)GLSL";

Wormhole::Wormhole(int /*width*/, int /*height*/)
    // A slow, cinematic open -> hold -> collapse (~9s total).
    : EventBase(2.5f, 4.0f, 2.5f) {
    // Open a bit off-centre so it doesn't look perfectly symmetrical/static.
    cx_ = frand(0.40f, 0.60f);
    cy_ = frand(0.40f, 0.60f);
    radius_ = frand(0.20f, 0.32f);     // fraction of the min screen dimension
    spin_ = chance(0.5f) ? 1.0f : -1.0f;
    spin_ *= frand(0.85f, 1.25f);
    hue_ = frand(0.0f, 1.0f);
}

void Wormhole::renderGL(const GLRenderContext& ctx) {
    // Compile once for the whole process; instant on later spawns.
    static glsl::ShaderPass pass;
    if (!pass.ensure(kWormholeFrag)) return;

    // Portal opens during the enter phase and partially collapses on exit; the
    // alpha envelope handles the actual fade.
    float enterT = enter_ > 0.0f ? clampf(age_ / enter_, 0.0f, 1.0f) : 1.0f;
    float exitStart = enter_ + hold_;
    float exitT = exit_ > 0.0f ? clampf((age_ - exitStart) / exit_, 0.0f, 1.0f)
                               : 0.0f;
    float sizeMul = lerpf(0.25f, 1.0f, smoothstep(enterT)) *
                    (1.0f - 0.65f * smoothstep(exitT));

    const float aspect = ctx.outH > 0
                             ? static_cast<float>(ctx.outW) / ctx.outH
                             : 1.0f;

    pass.use(ctx.outW, ctx.outH);
    pass.set2("uRes", aspect, 1.0f);   // only the ratio matters in the shader
    pass.setf("uTime", static_cast<float>(ctx.elapsed));
    pass.setf("uAlpha", alpha());
    pass.set2("uCenter", cx_, cy_);
    pass.setf("uRadius", radius_ * sizeMul);
    pass.setf("uSpin", spin_);
    pass.setf("uHue", hue_);
    pass.drawQuad();
    pass.finish();
}

} // namespace ssaver
