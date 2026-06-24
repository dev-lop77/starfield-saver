#include "nebula.h"

#include "../gl_shader.h"
#include "../util.h"

namespace ssaver {

// Alpha-blended fullscreen overlay. The fragment outputs colour + coverage
// alpha; the GL_SRC_ALPHA/ONE_MINUS_SRC_ALPHA blend then lays it over the stars
// translucently. fbm clouds give the soft, billowy shape.
static const char* kNebulaFrag = R"GLSL(
#version 120
varying vec2 vUV;
uniform vec2  uRes;     // (aspect, 1)
uniform float uTime;
uniform float uAlpha;   // envelope fade 0..1
uniform vec2  uCenter;
uniform float uSize;    // cloud extent (fraction of min dim)
uniform float uOpacity; // peak coverage
uniform vec2  uDrift;   // slow drift velocity
uniform float uSeed;
uniform vec3  uColorA;  // body
uniform vec3  uColorB;  // filaments

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float a = hash(i), b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0)), d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}
float fbm(vec2 p) {
    float v = 0.0, amp = 0.5;
    for (int i = 0; i < 6; i++) {
        v += amp * noise(p);
        p *= 2.02;
        amp *= 0.5;
    }
    return v;
}

void main() {
    vec2 p = vUV - uCenter;
    p.x *= uRes.x / uRes.y;
    p /= max(uSize, 0.001);           // big extent: uSize is large

    // Domain-warped fbm so the cloud billows rather than looks like flat noise.
    vec2 drift = uDrift * uTime;
    vec2 w = vec2(fbm(p * 1.3 + uSeed + drift),
                  fbm(p * 1.3 + uSeed + 5.2 - drift));
    float f = fbm(p * 1.6 + 1.5 * w + uSeed);

    // Density with a soft radial falloff so the edges dissolve. Lower threshold
    // + a faint floor make the cloud read more clearly against black.
    float dens = smoothstep(0.32, 0.88, f);
    float rad = length(p);
    dens *= smoothstep(1.45, 0.15, rad);

    // Colour: body colour through the cloud, brighter filament colour where it
    // is densest; lifted internal glow on the thickest knots.
    vec3 col = mix(uColorA, uColorB, smoothstep(0.40, 0.90, f));
    col += uColorB * pow(dens, 2.5) * 0.9;

    float a = dens * uOpacity * uAlpha;
    gl_FragColor = vec4(col, a);      // straight alpha (not premultiplied)
}
)GLSL";

namespace {
struct RGB { float r, g, b; };
// Paired (body, filament) palettes — emission-nebula-ish combos.
struct Pair { RGB a, b; };
const Pair kPalettes[] = {
    {{0.55f, 0.10f, 0.45f}, {0.95f, 0.35f, 0.70f}},  // magenta / pink
    {{0.08f, 0.25f, 0.55f}, {0.35f, 0.70f, 1.00f}},  // deep blue / cyan
    {{0.55f, 0.22f, 0.08f}, {1.00f, 0.62f, 0.30f}},  // rust / amber
    {{0.10f, 0.45f, 0.35f}, {0.45f, 0.95f, 0.70f}},  // teal / green
    {{0.35f, 0.15f, 0.55f}, {0.70f, 0.55f, 1.00f}},  // indigo / violet
};
float jit(float v) { return clampf(v + frand(-0.06f, 0.06f), 0.0f, 1.0f); }
} // namespace

Nebula::Nebula(int /*width*/, int /*height*/)
    // The longest, slowest event: it should hang in the sky and drift.
    : EventBase(4.5f, 8.0f, 4.5f) {
    cx_ = frand(0.35f, 0.65f);
    cy_ = frand(0.35f, 0.65f);
    size_ = frand(0.7f, 1.15f);                 // big — can fill the screen
    opacity_ = frand(0.62f, 0.92f);            // more visible per user feedback
    driftx_ = frand(-0.025f, 0.025f);           // very slow drift
    drifty_ = frand(-0.025f, 0.025f);
    seed_ = frand(0.0f, 20.0f);

    const Pair pal = kPalettes[irand(0, static_cast<int>(sizeof(kPalettes) / sizeof(kPalettes[0])) - 1)];
    ar_ = jit(pal.a.r); ag_ = jit(pal.a.g); ab_ = jit(pal.a.b);
    br_ = jit(pal.b.r); bg_ = jit(pal.b.g); bb_ = jit(pal.b.b);
}

void Nebula::renderGL(const GLRenderContext& ctx) {
    static glsl::ShaderPass pass;
    if (!pass.ensure(kNebulaFrag)) return;

    const float aspect = ctx.outH > 0
                             ? static_cast<float>(ctx.outW) / ctx.outH
                             : 1.0f;

    pass.use(ctx.outW, ctx.outH, glsl::Blend::Alpha);
    pass.set2("uRes", aspect, 1.0f);
    pass.setf("uTime", static_cast<float>(ctx.elapsed));
    pass.setf("uAlpha", alpha());
    pass.set2("uCenter", cx_, cy_);
    pass.setf("uSize", size_);
    pass.setf("uOpacity", opacity_);
    pass.set2("uDrift", driftx_, drifty_);
    pass.setf("uSeed", seed_);
    pass.set3("uColorA", ar_, ag_, ab_);
    pass.set3("uColorB", br_, bg_, bb_);
    pass.drawQuad();
    pass.finish();
}

} // namespace ssaver
