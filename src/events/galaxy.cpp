#include "galaxy.h"

#include "../gl_shader.h"
#include "../util.h"

namespace ssaver {

static const char* kGalaxyFrag = R"GLSL(
#version 120
varying vec2 vUV;
uniform vec2  uRes;     // (aspect, 1)
uniform float uTime;
uniform float uAlpha;
uniform vec2  uCenter;
uniform float uSize;    // disc radius (fraction of min dim)
uniform float uArms;
uniform float uWind;    // arm tightness
uniform float uSpin;    // rotation speed/direction
uniform float uTilt;    // inclination (>1 flattens)
uniform float uRoll;    // fixed disc rotation
uniform vec3  uColor;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float a = hash(i), b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0)), d = hash(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main() {
    // Aspect-correct, then roll + tilt the disc to fake a 3D inclination. The
    // roll advances with time so the whole galaxy visibly rotates.
    vec2 p = vUV - uCenter;
    p.x *= uRes.x / uRes.y;
    float roll = uRoll + uTime * uSpin;
    float cs = cos(roll), sn = sin(roll);
    p = vec2(p.x * cs - p.y * sn, p.x * sn + p.y * cs);
    p.y /= max(uTilt, 0.001);

    float r = length(p);
    float ang = atan(p.y, p.x);
    float q = r / max(uSize, 0.001);

    // Logarithmic spiral arms (they turn rigidly with the rolling disc above).
    float spiral = sin(uArms * ang + log(q + 0.05) * uWind);
    float arm = pow(0.5 + 0.5 * spiral, 2.0);

    // Radial light profile: exponential disc + tight central bulge.
    float disc  = exp(-q * 2.2);
    float bulge = exp(-q * q * 11.0);

    // Dust/star mottling along the arms, slowly churning (motion).
    float dust = 0.6 + 0.6 * noise(vec2(ang * 6.0, q * 9.0 + uTime * 0.4));

    vec3 col = uColor * disc * arm * dust * 0.95;
    col += mix(uColor, vec3(1.0, 0.9, 0.72), 0.7) * bulge * 1.10;  // warm core
    col += uColor * disc * 0.10;                                   // faint halo

    // Translucent so the starfield shows through the disc.
    float reach = smoothstep(1.6, 0.0, q);
    col *= reach * uAlpha * 0.72;
    gl_FragColor = vec4(col, 1.0);
}
)GLSL";

namespace {
struct RGB { float r, g, b; };
const RGB kTints[] = {
    {0.65f, 0.78f, 1.00f},  // blue-white
    {1.00f, 0.88f, 0.70f},  // warm gold
    {0.85f, 0.70f, 1.00f},  // lilac
    {0.70f, 1.00f, 0.92f},  // pale teal
};
} // namespace

Galaxy::Galaxy(int /*width*/, int /*height*/)
    // Slow and majestic: a long, gentle drift-through.
    : EventBase(3.5f, 6.0f, 3.5f) {
    cx_ = frand(0.32f, 0.68f);
    cy_ = frand(0.32f, 0.68f);
    size_ = frand(0.32f, 0.52f);
    arms_ = static_cast<float>(irand(2, 4));
    wind_ = frand(4.0f, 8.0f);
    spin_ = (chance(0.5f) ? 1.0f : -1.0f) * frand(0.35f, 0.65f);
    tilt_ = frand(1.6f, 3.2f);                 // inclined disc
    roll_ = frand(0.0f, 6.2831f);
    const RGB t = kTints[irand(0, static_cast<int>(sizeof(kTints) / sizeof(kTints[0])) - 1)];
    cr_ = clampf(t.r + frand(-0.08f, 0.08f), 0.0f, 1.0f);
    cg_ = clampf(t.g + frand(-0.08f, 0.08f), 0.0f, 1.0f);
    cb_ = clampf(t.b + frand(-0.08f, 0.08f), 0.0f, 1.0f);
}

void Galaxy::renderGL(const GLRenderContext& ctx) {
    static glsl::ShaderPass pass;
    if (!pass.ensure(kGalaxyFrag)) return;

    const float aspect = ctx.outH > 0
                             ? static_cast<float>(ctx.outW) / ctx.outH
                             : 1.0f;

    pass.use(ctx.outW, ctx.outH, glsl::Blend::Add);
    pass.set2("uRes", aspect, 1.0f);
    pass.setf("uTime", static_cast<float>(ctx.elapsed));
    pass.setf("uAlpha", alpha());
    pass.set2("uCenter", cx_, cy_);
    pass.setf("uSize", size_);
    pass.setf("uArms", arms_);
    pass.setf("uWind", wind_);
    pass.setf("uSpin", spin_);
    pass.setf("uTilt", tilt_);
    pass.setf("uRoll", roll_);
    pass.set3("uColor", cr_, cg_, cb_);
    pass.drawQuad();
    pass.finish();
}

} // namespace ssaver
