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
    bool crt = false;        // CRT post-process on/off (default off; enable with crt=1)
    int  downscale = 0;      // 0 = use the config.h default; else render = native/this
    int  starDensity = 100;  // star count, percent of the built-in default (clamped 25..200)
    int  eventFreq = 100;    // event spawn rate, percent of default (clamped 25..300)
    bool audition = false;   // dev eye-review: cycle every event in order (see App)
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
    else if (std::strcmp(key, "star_density") == 0)
        s.starDensity = static_cast<int>(std::strtol(val, nullptr, 10));
    else if (std::strcmp(key, "event_freq") == 0)
        s.eventFreq = static_cast<int>(std::strtol(val, nullptr, 10));
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
    // 1) next to the executable (a preset .ini dropped beside the .scr): used as
    //    the base layer.
    if (char* base = SDL_GetBasePath()) {
        std::snprintf(path, sizeof(path), "%s%s", base, kFile);
        SDL_free(base);
        detail::parseFile(path, s);
    }
    // 2) per-user app-data (always writable — this is where the configuration
    //    dialog writes). Applied last so the user's dialog choices override any
    //    preset that sits next to the .scr (which may be in a read-only folder).
    if (char* pref = SDL_GetPrefPath("ssaver", "StarfieldSaver")) {
        std::snprintf(path, sizeof(path), "%s%s", pref, kFile);
        SDL_free(pref);
        detail::parseFile(path, s);
    }
    return s;
}

// Persist settings to the per-user app-data .ini (the only reliably writable
// location: the .scr itself usually lives in a read-only system folder). The
// configuration dialog calls this; loadSettings() reads it back. Returns false
// if the file couldn't be written.
inline bool saveSettings(const Settings& s) {
    char* pref = SDL_GetPrefPath("ssaver", "StarfieldSaver");
    if (!pref) return false;
    char path[1024];
    std::snprintf(path, sizeof(path), "%sstarfield-saver.ini", pref);
    SDL_free(pref);

    std::FILE* f = std::fopen(path, "w");
    if (!f) return false;
    std::fprintf(f,
                 "# Starfield Saver settings (written by the configuration dialog).\n"
                 "# You can also edit this by hand; restart the screensaver to apply.\n"
                 "\n"
                 "crt = %d           ; CRT filter on/off\n"
                 "downscale = %d         ; pixel chunkiness: 1 = native (crispest) .. 4\n"
                 "star_density = %d    ; star count, percent of default (25..200)\n"
                 "event_freq = %d      ; event spawn rate, percent of default (25..300)\n"
                 "audition = %d          ; dev: cycle every event in order\n",
                 s.crt ? 1 : 0, s.downscale, s.starDensity, s.eventFreq,
                 s.audition ? 1 : 0);
    std::fclose(f);
    return true;
}

}  // namespace ssaver
