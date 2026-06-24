// Reference copy of the CRT fragment shader.
//
// NOTE: the authoritative version is embedded as a string literal in
// src/postfx_crt.cpp (kFragmentSrc) so the screensaver has no runtime file
// dependencies. Keep this copy in sync when tweaking; it exists for easy
// reading/experimentation.

#version 120
varying vec2 vUV;
uniform sampler2D uTex;
uniform vec2  uRes;        // low-res scene size (scanline frequency)
uniform vec2  uTexScale;   // maps 0..1 to the texture's used region
uniform float uTime;

// Barrel distortion to fake the bulge of a CRT tube.
vec2 curve(vec2 uv) {
    uv = uv * 2.0 - 1.0;
    vec2 off = abs(uv.yx) / vec2(6.0, 5.0);
    uv = uv + uv * off * off;
    return uv * 0.5 + 0.5;
}

void main() {
    vec2 uv = curve(vUV);

    // Outside the curved screen -> black bezel.
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec2 t = uv * uTexScale;

    // Chromatic aberration: pull R and B apart slightly.
    float ca = 0.0016;
    float r = texture2D(uTex, vec2(t.x + ca, t.y)).r;
    float g = texture2D(uTex, t).g;
    float b = texture2D(uTex, vec2(t.x - ca, t.y)).b;
    vec3 col = vec3(r, g, b);

    // Horizontal scanlines.
    float scan = sin(uv.y * uRes.y * 3.14159265) * 0.5 + 0.5;
    col *= mix(1.0, scan, 0.20);

    // Faint vertical aperture grille.
    float grille = sin(uv.x * uRes.x * 3.14159265) * 0.5 + 0.5;
    col *= mix(1.0, grille, 0.07);

    // Vignette toward the corners.
    float vig = uv.x * (1.0 - uv.x) * uv.y * (1.0 - uv.y) * 16.0;
    vig = clamp(pow(vig, 0.22), 0.0, 1.0);
    col *= mix(0.65, 1.0, vig);

    // Compensate brightness lost to scanlines, plus a soft glow lift.
    col *= 1.25;

    // Subtle phosphor flicker.
    col *= 1.0 + 0.02 * sin(uTime * 9.0);

    gl_FragColor = vec4(col, 1.0);
}
