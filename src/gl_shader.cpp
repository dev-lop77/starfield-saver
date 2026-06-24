#include "gl_shader.h"

#include <SDL.h>
#include <cstdio>

#if defined(_WIN32)
#include <windows.h>   // must precede gl.h for APIENTRY / WINGDIAPI
#endif
#include <GL/gl.h>

namespace ssaver {
namespace glsl {

// --- GL enum fallbacks (legacy <GL/gl.h> lacks the shader tokens) -------------
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

// --- GL shader entry points (loaded at runtime via SDL) -----------------------
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
typedef void   (APIENTRY *PFN_glUniform1f)(GLint, GLfloat);
typedef void   (APIENTRY *PFN_glUniform2f)(GLint, GLfloat, GLfloat);

namespace {
PFN_glCreateShader       pCreateShader       = nullptr;
PFN_glShaderSource       pShaderSource       = nullptr;
PFN_glCompileShader      pCompileShader      = nullptr;
PFN_glGetShaderiv        pGetShaderiv        = nullptr;
PFN_glGetShaderInfoLog   pGetShaderInfoLog   = nullptr;
PFN_glDeleteShader       pDeleteShader       = nullptr;
PFN_glCreateProgram      pCreateProgram      = nullptr;
PFN_glAttachShader       pAttachShader       = nullptr;
PFN_glLinkProgram        pLinkProgram        = nullptr;
PFN_glGetProgramiv       pGetProgramiv       = nullptr;
PFN_glGetProgramInfoLog  pGetProgramInfoLog  = nullptr;
PFN_glUseProgram         pUseProgram         = nullptr;
PFN_glGetUniformLocation pGetUniformLocation = nullptr;
PFN_glUniform1f          pUniform1f          = nullptr;
PFN_glUniform2f          pUniform2f          = nullptr;

enum class LoadState { Unknown, Ok, Failed };
LoadState g_state = LoadState::Unknown;

template <typename T>
bool load(T& fn, const char* name) {
    fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    return fn != nullptr;
}

bool loadEntryPoints() {
    bool ok = true;
    ok &= load(pCreateShader,       "glCreateShader");
    ok &= load(pShaderSource,       "glShaderSource");
    ok &= load(pCompileShader,      "glCompileShader");
    ok &= load(pGetShaderiv,        "glGetShaderiv");
    ok &= load(pGetShaderInfoLog,   "glGetShaderInfoLog");
    ok &= load(pDeleteShader,       "glDeleteShader");
    ok &= load(pCreateProgram,      "glCreateProgram");
    ok &= load(pAttachShader,       "glAttachShader");
    ok &= load(pLinkProgram,        "glLinkProgram");
    ok &= load(pGetProgramiv,       "glGetProgramiv");
    ok &= load(pGetProgramInfoLog,  "glGetProgramInfoLog");
    ok &= load(pUseProgram,         "glUseProgram");
    ok &= load(pGetUniformLocation, "glGetUniformLocation");
    ok &= load(pUniform1f,          "glUniform1f");
    ok &= load(pUniform2f,          "glUniform2f");
    return ok;
}

// Standard vertex shader: verts are already in clip space, texcoord -> vUV.
const char* kVertexSrc = R"GLSL(
#version 120
varying vec2 vUV;
void main() {
    vUV = gl_MultiTexCoord0.xy;
    gl_Position = gl_Vertex;
}
)GLSL";

GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = pCreateShader(type);
    pShaderSource(sh, 1, &src, nullptr);
    pCompileShader(sh);
    GLint okFlag = 0;
    pGetShaderiv(sh, GL_COMPILE_STATUS, &okFlag);
    if (!okFlag) {
        char log[1024];
        pGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        std::fprintf(stderr, "GLSL event shader compile failed: %s\n", log);
        pDeleteShader(sh);
        return 0;
    }
    return sh;
}

} // namespace

bool available() {
    if (g_state == LoadState::Unknown) {
        if (SDL_GL_GetCurrentContext() && loadEntryPoints()) {
            g_state = LoadState::Ok;
        } else {
            g_state = LoadState::Failed;  // software/headless renderer
        }
    }
    return g_state == LoadState::Ok;
}

bool ShaderPass::ensure(const char* fragSrc) {
    if (tried_) return ok_;
    tried_ = true;
    if (!available()) return false;

    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
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
        std::fprintf(stderr, "GLSL event shader link failed: %s\n", log);
        return false;
    }
    pDeleteShader(vs);
    pDeleteShader(fs);
    ok_ = true;
    return true;
}

void ShaderPass::use(int viewportW, int viewportH) {
    if (!ok_) return;
    // Make sure SDL has flushed its queued GL commands before we take over.
    glViewport(0, 0, viewportW, viewportH);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);  // additive glow; fade via premultiplied colour
    pUseProgram(program_);
}

void ShaderPass::setf(const char* name, float v) {
    if (!ok_) return;
    GLint l = pGetUniformLocation(program_, name);
    if (l >= 0) pUniform1f(l, v);
}

void ShaderPass::set2(const char* name, float a, float b) {
    if (!ok_) return;
    GLint l = pGetUniformLocation(program_, name);
    if (l >= 0) pUniform2f(l, a, b);
}

void ShaderPass::drawQuad() {
    if (!ok_) return;
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
    glEnd();
}

void ShaderPass::finish() {
    if (!ok_) return;
    pUseProgram(0);
    glDisable(GL_BLEND);
}

} // namespace glsl
} // namespace ssaver
