#include "postfx_crt.h"

#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>   // must precede gl.h for APIENTRY / WINGDIAPI
#endif
#include <GL/gl.h>

namespace ssaver {

// If the image ever comes out vertically mirrored on your hardware, flip this.
// (SDL render-target textures sampled via SDL_GL_BindTexture can differ by
// platform; left false for the common case — easy one-line toggle.)
static const bool kFlipY = false;

// --- GL shader entry points (loaded at runtime) -------------------------------
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif

typedef GLuint (APIENTRY *PFN_glCreateShader)(GLenum);
typedef void   (APIENTRY *PFN_glShaderSource)(GLuint, GLsizei, const char* const*, const GLint*);
typedef void   (APIENTRY *PFN_glCompileShader)(GLuint);
typedef void   (APIENTRY *PFN_glGetShaderiv)(GLuint, GLenum, GLint*);
typedef void   (APIENTRY *PFN_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, char*);
typedef void   (APIENTRY *PFN_glDeleteShader)(GLuint);
typedef GLuint (APIENTRY *PFN_glCreateProgram)(void);
typedef void   (APIENTRY *PFN_glAttachShader)(GLuint, GLuint);
typedef void   (APIENTRY *PFN_glLinkProgram)(GLuint);
typedef void   (APIENTRY *PFN_glGetProgramiv)(GLuint, GLenum, GLint*);
typedef void   (APIENTRY *PFN_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, char*);
typedef void   (APIENTRY *PFN_glUseProgram)(GLuint);
typedef GLint  (APIENTRY *PFN_glGetUniformLocation)(GLuint, const char*);
typedef void   (APIENTRY *PFN_glUniform1i)(GLint, GLint);
typedef void   (APIENTRY *PFN_glUniform1f)(GLint, GLfloat);
typedef void   (APIENTRY *PFN_glUniform2f)(GLint, GLfloat, GLfloat);

namespace {
PFN_glCreateShader      pCreateShader      = nullptr;
PFN_glShaderSource      pShaderSource      = nullptr;
PFN_glCompileShader     pCompileShader     = nullptr;
PFN_glGetShaderiv       pGetShaderiv       = nullptr;
PFN_glGetShaderInfoLog  pGetShaderInfoLog  = nullptr;
PFN_glDeleteShader      pDeleteShader      = nullptr;
PFN_glCreateProgram     pCreateProgram     = nullptr;
PFN_glAttachShader      pAttachShader      = nullptr;
PFN_glLinkProgram       pLinkProgram       = nullptr;
PFN_glGetProgramiv      pGetProgramiv      = nullptr;
PFN_glGetProgramInfoLog pGetProgramInfoLog = nullptr;
PFN_glUseProgram        pUseProgram        = nullptr;
PFN_glGetUniformLocation pGetUniformLocation = nullptr;
PFN_glUniform1i         pUniform1i         = nullptr;
PFN_glUniform1f         pUniform1f         = nullptr;
PFN_glUniform2f         pUniform2f         = nullptr;

template <typename T>
bool load(T& fn, const char* name) {
    fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    return fn != nullptr;
}

bool loadGlFunctions() {
    bool ok = true;
    ok &= load(pCreateShader,      "glCreateShader");
    ok &= load(pShaderSource,      "glShaderSource");
    ok &= load(pCompileShader,     "glCompileShader");
    ok &= load(pGetShaderiv,       "glGetShaderiv");
    ok &= load(pGetShaderInfoLog,  "glGetShaderInfoLog");
    ok &= load(pDeleteShader,      "glDeleteShader");
    ok &= load(pCreateProgram,     "glCreateProgram");
    ok &= load(pAttachShader,      "glAttachShader");
    ok &= load(pLinkProgram,       "glLinkProgram");
    ok &= load(pGetProgramiv,      "glGetProgramiv");
    ok &= load(pGetProgramInfoLog, "glGetProgramInfoLog");
    ok &= load(pUseProgram,        "glUseProgram");
    ok &= load(pGetUniformLocation,"glGetUniformLocation");
    ok &= load(pUniform1i,         "glUniform1i");
    ok &= load(pUniform1f,         "glUniform1f");
    ok &= load(pUniform2f,         "glUniform2f");
    return ok;
}

// --- Shader sources (GLSL 1.20 — matches SDL's legacy GL renderer context) ----
const char* kVertexSrc = R"GLSL(
#version 120
varying vec2 vUV;
void main() {
    vUV = gl_MultiTexCoord0.xy;
    gl_Position = gl_Vertex;   // verts are already in clip space (-1..1)
}
)GLSL";

const char* kFragmentSrc = R"GLSL(
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
)GLSL";

GLuint compile(GLenum type, const char* src) {
    GLuint sh = pCreateShader(type);
    pShaderSource(sh, 1, &src, nullptr);
    pCompileShader(sh);
    GLint okFlag = 0;
    pGetShaderiv(sh, GL_COMPILE_STATUS, &okFlag);
    if (!okFlag) {
        char log[1024];
        pGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        std::fprintf(stderr, "CRT shader compile failed: %s\n", log);
        pDeleteShader(sh);
        return 0;
    }
    return sh;
}

} // namespace

CrtEffect::~CrtEffect() {
    // Program leaks on shutdown are harmless (process exits); skip GL teardown
    // to avoid touching a possibly-gone context.
}

bool CrtEffect::init() {
    if (!SDL_GL_GetCurrentContext()) return false;  // not a GL renderer
    if (!loadGlFunctions()) {
        std::fprintf(stderr, "CRT: GL shader entry points unavailable\n");
        return false;
    }

    GLuint vs = compile(GL_VERTEX_SHADER, kVertexSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, kFragmentSrc);
    if (!vs || !fs) return false;

    program_ = pCreateProgram();
    pAttachShader(program_, vs);
    pAttachShader(program_, fs);
    pLinkProgram(program_);
    GLint linked = 0;
    pGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[1024];
        pGetProgramInfoLog(program_, sizeof(log), nullptr, log);
        std::fprintf(stderr, "CRT shader link failed: %s\n", log);
        return false;
    }
    pDeleteShader(vs);
    pDeleteShader(fs);

    uTex_      = pGetUniformLocation(program_, "uTex");
    uRes_      = pGetUniformLocation(program_, "uRes");
    uTexScale_ = pGetUniformLocation(program_, "uTexScale");
    uTime_     = pGetUniformLocation(program_, "uTime");

    ready_ = true;
    return true;
}

void CrtEffect::apply(SDL_Renderer* renderer, SDL_Texture* src,
                      int outW, int outH, int srcW, int srcH, float time) {
    if (!ready_) return;

    // Make sure SDL has flushed its own queued GL commands before we take over.
    SDL_RenderFlush(renderer);

    glViewport(0, 0, outW, outH);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    // Bind the SDL texture as GL_TEXTURE_2D on unit 0; texw/texh give the
    // 0..1 -> used-region scale (1,1 for power-of-two / modern GL).
    float texw = 1.0f, texh = 1.0f;
    SDL_GL_BindTexture(src, &texw, &texh);

    pUseProgram(program_);
    if (uTex_      >= 0) pUniform1i(uTex_, 0);
    if (uRes_      >= 0) pUniform2f(uRes_, static_cast<float>(srcW), static_cast<float>(srcH));
    if (uTexScale_ >= 0) pUniform2f(uTexScale_, texw, texh);
    if (uTime_     >= 0) pUniform1f(uTime_, time);

    // Fullscreen quad in clip space; texcoords 0..1 (V flipped if needed).
    const float v0 = kFlipY ? 1.0f : 0.0f;
    const float v1 = kFlipY ? 0.0f : 1.0f;
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, v0); glVertex2f(-1.0f,  1.0f);  // top-left
        glTexCoord2f(1.0f, v0); glVertex2f( 1.0f,  1.0f);  // top-right
        glTexCoord2f(1.0f, v1); glVertex2f( 1.0f, -1.0f);  // bottom-right
        glTexCoord2f(0.0f, v1); glVertex2f(-1.0f, -1.0f);  // bottom-left
    glEnd();

    pUseProgram(0);
    SDL_GL_UnbindTexture(src);
}

} // namespace ssaver
