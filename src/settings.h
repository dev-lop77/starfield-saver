#pragma once
// Optional runtime settings, read from a small "starfield-saver.ini" so the look
// can be changed on a machine that can't rebuild (the Windows target has no
// toolchain). The file is looked for first next to the executable (most
// discoverable — same folder as the .scr) and then in the per-user app-data
// folder (always writable, same place as any log):
//
//   <exe dir>\starfield-saver.ini
//   %APPDATA%\ssaver\StarfieldSaver\starfield-saver.ini
//
// Format: one `key = value` per line; `#` or `;` starts a comment. Missing file
// or keys just fall back to the built-in defaults. Restart to apply.

#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ssaver {

struct Settings {
    bool crt = false;       // CRT post-process on/off (default off; enable with crt=1)
    int  downscale = 0;     // 0 = use the config.h default; else render = native/this
    bool audition = false;  // dev eye-review: cycle every event in order (see App)
};

namespace detail {

inline char* trim(char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    char* end = p + std::strlen(p);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t' ||
                       end[-1] == '\r' || end[-1] == '\n')) --end;
    *end = '\0';
    return p;
}

inline void applyKV(Settings& s, const char* key, const char* val) {
    // strtol (not atoi) so overflow is well-defined; ranges are clamped where
    // the values are used (see App::init).
    if (std::strcmp(key, "crt") == 0)
        s.crt = std::strtol(val, nullptr, 10) != 0;
    else if (std::strcmp(key, "downscale") == 0)
        s.downscale = static_cast<int>(std::strtol(val, nullptr, 10));
    else if (std::strcmp(key, "audition") == 0)
        s.audition = std::strtol(val, nullptr, 10) != 0;
}

inline bool parseFile(const char* path, Settings& s) {
    std::FILE* f = std::fopen(path, "r");
    if (!f) return false;
    char line[256];
    int lines = 0;
    // Cap the work: this is a tiny hand-edited file; treat anything huge as
    // hostile/garbage and stop early.
    while (lines++ < 1000 && std::fgets(line, sizeof(line), f)) {
        if (char* c = std::strpbrk(line, "#;")) *c = '\0';  // strip comment
        char* eq = std::strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = trim(line);
        char* val = trim(eq + 1);
        if (*key) applyKV(s, key, val);
    }
    std::fclose(f);
    return true;
}

}  // namespace detail

inline Settings loadSettings() {
    Settings s;
    const char* kFile = "starfield-saver.ini";
    char path[1024];
    // 1) next to the executable (drop the .ini beside the .scr)
    if (char* base = SDL_GetBasePath()) {
        std::snprintf(path, sizeof(path), "%s%s", base, kFile);
        SDL_free(base);
        if (detail::parseFile(path, s)) return s;
    }
    // 2) per-user app-data fallback (always writable)
    if (char* pref = SDL_GetPrefPath("ssaver", "StarfieldSaver")) {
        std::snprintf(path, sizeof(path), "%s%s", pref, kFile);
        SDL_free(pref);
        detail::parseFile(path, s);
    }
    return s;
}

}  // namespace ssaver
