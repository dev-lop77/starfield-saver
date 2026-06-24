#include "supernova.h"

#include "../gl_shader.h"
#include "../util.h"

namespace ssaver {

// Additive fullscreen overlay. uProg (0..1) drives the explosion independently
// of the alpha envelope, so the shock can keep expanding while the whole thing
// fades out.
static const char* kSupernovaFrag = R"GLSL(
#version 120
varying vec2 vUV;
uniform vec2  uRes;     // (aspect, 1)
uniform float uTime;    // seconds (turbulence shimmer)
uniform float uAlpha;   // envelope fade 0..1
uniform float uProg;    // 0..1 blast progress
uniform vec2  uCenter;
uniform float uSize;    // final blast radius (fraction of min dim)
uniform float uRays;    // spike count
uniform float uSeed;
uniform vec3  uColor;   // blast colour

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float a = hash(i), b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0)), d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main() {
    vec2 p = vUV - uCenter;
    p.x *= uRes.x / uRes.y;
    float r = length(p);
    float ang = atan(p.y, p.x);
    float q = r / max(uSize, 0.001);

    float prog = uProg;

    // Expanding shock shell: a bright ring whose radius grows with prog and
    // thins/dims as it ages.
    float shellR = prog;
    float shellW = mix(0.06, 0.32, prog);                 // widens as it expands
    float shell = exp(-pow((q - shellR) / shellW, 2.0));
    shell *= (1.0 - 0.6 * prog);

    // Radial rays/spikes, slowly rotating (motion) and brighter early.
    float rays = 0.5 + 0.5 * sin(ang * uRays + uSeed + uTime * 0.7);
    rays = pow(rays, 3.0);
    float spikes = exp(-pow((q - shellR) / (shellW * 1.6), 2.0)) *
                   rays * (1.0 - prog);

    // Hot core: a flash early that collapses quickly, with a faint flicker.
    float core = exp(-q * q * 10.0) * (1.0 - smoothstep(0.0, 0.45, prog));
    core *= 0.9 + 0.1 * sin(uTime * 11.0);

    // Glowing fill inside the shell, fading as it thins out.
    float fill = smoothstep(shellR, shellR - 0.5, q) * (1.0 - prog) * 0.35;

    // Debris turbulence churning on the shell (motion).
    float turb = noise(vec2(ang * 5.0 + uSeed + uTime * 0.6, q * 6.0 - uTime * 1.8));

    vec3 col = uColor * (shell * (0.6 + 0.6 * turb) + spikes * 0.8 + fill);
    col += vec3(1.0, 0.96, 0.9) * core * 1.35;             // flash (softer)
    col += mix(uColor, vec3(1.0), 0.5) * core * 0.6;

    // Keep it translucent so the stars read through the blast rather than it
    // being a solid bright disc.
    float reach = smoothstep(1.5, 0.0, q);
    col *= reach * uAlpha * 0.68;
    gl_FragColor = vec4(col, 1.0);
}
)GLSL";

namespace {
// A handful of plausible supernova tints; one is picked and jittered per spawn.
struct RGB { float r, g, b; };
const RGB kTints[] = {
    {0.55f, 0.70f, 1.00f},  // hot blue-white
    {1.00f, 0.80f, 0.45f},  // gold
    {1.00f, 0.45f, 0.35f},  // red giant
    {0.55f, 1.00f, 0.70f},  // teal/green
    {0.85f, 0.55f, 1.00f},  // violet
};
} // namespace

Supernova::Supernova(int /*width*/, int /*height*/)
    // Quick swell, brief blinding hold, long fading debris.
    : EventBase(1.6f, 0.6f, 4.2f) {
    cx_ = frand(0.30f, 0.70f);
    cy_ = frand(0.30f, 0.70f);
    size_ = frand(0.30f, 0.55f);              // randomised blast dimension
    rays_ = static_cast<float>(irand(8, 18));
    seed_ = frand(0.0f, 6.2831f);

    const RGB t = kTints[irand(0, static_cast<int>(sizeof(kTints) / sizeof(kTints[0])) - 1)];
    cr_ = clampf(t.r + frand(-0.10f, 0.10f), 0.0f, 1.0f);
    cg_ = clampf(t.g + frand(-0.10f, 0.10f), 0.0f, 1.0f);
    cb_ = clampf(t.b + frand(-0.10f, 0.10f), 0.0f, 1.0f);
}

void Supernova::renderGL(const GLRenderContext& ctx) {
    static glsl::ShaderPass pass;
    if (!pass.ensure(kSupernovaFrag)) return;

    const float total = enter_ + hold_ + exit_;
    const float prog = total > 0.0f ? clampf(age_ / total, 0.0f, 1.0f) : 1.0f;
    const float aspect = ctx.outH > 0
                             ? static_cast<float>(ctx.outW) / ctx.outH
                             : 1.0f;

    pass.use(ctx.outW, ctx.outH, glsl::Blend::Add);
    pass.set2("uRes", aspect, 1.0f);
    pass.setf("uTime", static_cast<float>(ctx.elapsed));
    pass.setf("uAlpha", alpha());
    pass.setf("uProg", prog);
    pass.set2("uCenter", cx_, cy_);
    pass.setf("uSize", size_);
    pass.setf("uRays", rays_);
    pass.setf("uSeed", seed_);
    pass.set3("uColor", cr_, cg_, cb_);
    pass.drawQuad();
    pass.finish();
}

} // namespace ssaver
