// Windows screensaver shell. Compiled only on Windows (via the mingw target).
// Parses the standard .scr command line and launches the App in the right mode.
//
//   <prog> /s            fullscreen screensaver
//   <prog> /p <HWND>     preview inside the Display Settings dialog
//   <prog> /c[:<HWND>]   configuration dialog
//   <prog>               (no args) — treated as configuration
//
// There's no audition command-line flag here on purpose: a .scr can't be passed
// arguments through the Windows shell. Eye-review mode is enabled from the .ini
// instead (audition = 1), which the normal /s launch picks up (see settings.h /
// App::init).
//
// The actual rendering is platform-independent (App, in app.cpp); this file is
// only the Win32 glue.

#if defined(_WIN32)

#include <windows.h>
#include <SDL.h>
#include <cstdlib>
#include <cstring>

#include "app.h"
#include "config.h"
#include "settings.h"

namespace {

// Parse an HWND that may be passed in decimal or hex on the command line.
HWND ParseHwnd(const char* s) {
    if (!s) return nullptr;
    // Windows passes the handle as a plain (possibly very large) integer.
    unsigned long long v = std::strtoull(s, nullptr, 0);
    return reinterpret_cast<HWND>(static_cast<uintptr_t>(v));
}

int RunFullscreen() {
    using namespace ssaver;
    AppConfig cfg;
    cfg.fullscreen = true;
    cfg.hideCursor = true;
    cfg.exitOnInput = true;
    App app(cfg);
    if (!app.init()) return 1;
    return app.run();
}

// Render the live screensaver inside the tiny preview pane. We wrap the HWND the
// OS hands us as an SDL window and run the same engine; input handling is off
// because the preview must not capture the user's mouse/keyboard.
int RunPreview(HWND parent) {
    using namespace ssaver;
    if (!parent || !IsWindow(parent)) return 0;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    SDL_Window* win = SDL_CreateWindowFrom(reinterpret_cast<const void*>(parent));
    if (!win) { SDL_Quit(); return 1; }

    AppConfig cfg;
    cfg.existingWindow = win;
    cfg.exitOnInput = false;  // never quit from preview activity
    App app(cfg);
    if (!app.init()) return 1;

    // The App loop exits on SDL_QUIT; in the preview, that arrives when the
    // parent dialog closes and the child window is destroyed.
    return app.run();
}

// ----------------------------------------------------------- config dialog ---
// The /c configuration dialog. Built programmatically (no .rc resource, to keep
// the toolchain-free build) from plain user32 controls. It edits the same
// runtime settings the engine reads at startup (see settings.h) and writes them
// to the per-user .ini, so changes apply on the next launch without a rebuild.

enum {
    IDC_CRT = 1001,
    IDC_DOWNSCALE,
    IDC_DENSITY,
    IDC_FREQ,
    IDC_SAVE,
    // Cancel uses the standard IDCANCEL (2) so Esc closes the dialog.
};

// The percentage values the density / frequency combo entries map to (index ->
// percent), in the same order the items are added.
const int kDensityVals[] = {50, 100, 150, 200};
const int kFreqVals[]    = {50, 100, 150, 200};

struct ConfigUI {
    ssaver::Settings settings;
    HWND crt = nullptr, downscale = nullptr, density = nullptr, freq = nullptr;
    bool saved = false;
};

// Index of the array entry closest to v (so a hand-edited .ini value still maps
// to the nearest combo choice).
int NearestIndex(const int* vals, int n, int v) {
    int best = 0, bestDist = 1 << 30;
    for (int i = 0; i < n; ++i) {
        int d = vals[i] > v ? vals[i] - v : v - vals[i];
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

void ComboAdd(HWND combo, const wchar_t* text) {
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
}

void SetCtrlFont(HWND ctrl, HFONT font) {
    SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

LRESULT CALLBACK ConfigWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ConfigUI* ui =
        reinterpret_cast<ConfigUI*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            ui = reinterpret_cast<ConfigUI*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(ui));
            HINSTANCE inst = cs->hInstance;
            HFONT font =
                reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

            const int kLabelX = 16, kCtrlX = 150, kCtrlW = 150, kRowH = 30;
            int y = 14;

            auto addLabel = [&](const wchar_t* t, int yy) {
                HWND l = CreateWindowExW(
                    0, L"STATIC", t, WS_CHILD | WS_VISIBLE, kLabelX, yy + 4,
                    kCtrlX - kLabelX - 8, 18, hwnd, nullptr, inst, nullptr);
                SetCtrlFont(l, font);
            };
            auto addCombo = [&](int id, int yy) {
                HWND c = CreateWindowExW(
                    0, L"COMBOBOX", nullptr,
                    WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
                    kCtrlX, yy, kCtrlW, 160, hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst,
                    nullptr);
                SetCtrlFont(c, font);
                return c;
            };

            // CRT on/off (a single full-width checkbox).
            ui->crt = CreateWindowExW(
                0, L"BUTTON", L"Enable CRT filter",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, kLabelX, y,
                260, 20, hwnd, reinterpret_cast<HMENU>(IDC_CRT), inst, nullptr);
            SetCtrlFont(ui->crt, font);
            SendMessageW(ui->crt, BM_SETCHECK,
                         ui->settings.crt ? BST_CHECKED : BST_UNCHECKED, 0);
            y += kRowH;

            addLabel(L"Pixel chunkiness:", y);
            ui->downscale = addCombo(IDC_DOWNSCALE, y);
            ComboAdd(ui->downscale, L"Native (crispest)");
            ComboAdd(ui->downscale, L"Half");
            ComboAdd(ui->downscale, L"Third");
            ComboAdd(ui->downscale, L"Quarter (chunkiest)");
            {  // downscale 0/1 => Native; clamp to the 4 offered steps.
                int ds = ui->settings.downscale <= 1 ? 1 : ui->settings.downscale;
                if (ds > 4) ds = 4;
                SendMessageW(ui->downscale, CB_SETCURSEL, ds - 1, 0);
            }
            y += kRowH;

            addLabel(L"Star density:", y);
            ui->density = addCombo(IDC_DENSITY, y);
            ComboAdd(ui->density, L"Sparse (50%)");
            ComboAdd(ui->density, L"Normal (100%)");
            ComboAdd(ui->density, L"Dense (150%)");
            ComboAdd(ui->density, L"Very dense (200%)");
            SendMessageW(ui->density, CB_SETCURSEL,
                         NearestIndex(kDensityVals, 4, ui->settings.starDensity),
                         0);
            y += kRowH;

            addLabel(L"Event frequency:", y);
            ui->freq = addCombo(IDC_FREQ, y);
            ComboAdd(ui->freq, L"Rare (50%)");
            ComboAdd(ui->freq, L"Normal (100%)");
            ComboAdd(ui->freq, L"Frequent (150%)");
            ComboAdd(ui->freq, L"Very frequent (200%)");
            SendMessageW(ui->freq, CB_SETCURSEL,
                         NearestIndex(kFreqVals, 4, ui->settings.eventFreq), 0);
            y += kRowH + 10;

            HWND save = CreateWindowExW(
                0, L"BUTTON", L"Save",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, kCtrlX, y,
                70, 26, hwnd, reinterpret_cast<HMENU>(IDC_SAVE), inst, nullptr);
            SetCtrlFont(save, font);
            HWND cancel = CreateWindowExW(
                0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                kCtrlX + 80, y, 70, 26, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), inst,
                nullptr);
            SetCtrlFont(cancel, font);
            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wp)) {
                case IDC_SAVE: {
                    if (ui) {
                        ssaver::Settings& s = ui->settings;
                        s.crt = SendMessageW(ui->crt, BM_GETCHECK, 0, 0) ==
                                BST_CHECKED;
                        int dsi = static_cast<int>(
                            SendMessageW(ui->downscale, CB_GETCURSEL, 0, 0));
                        s.downscale = (dsi < 0 ? 0 : dsi) + 1;
                        int di = static_cast<int>(
                            SendMessageW(ui->density, CB_GETCURSEL, 0, 0));
                        if (di >= 0) s.starDensity = kDensityVals[di];
                        int fi = static_cast<int>(
                            SendMessageW(ui->freq, CB_GETCURSEL, 0, 0));
                        if (fi >= 0) s.eventFreq = kFreqVals[fi];
                        ui->saved = ssaver::saveSettings(s);
                        if (!ui->saved)
                            MessageBoxW(hwnd,
                                        L"Could not write the settings file.",
                                        L"Starfield Saver",
                                        MB_OK | MB_ICONWARNING);
                    }
                    DestroyWindow(hwnd);
                    return 0;
                }
                case IDCANCEL:
                    DestroyWindow(hwnd);
                    return 0;
                default:
                    break;
            }
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int RunConfig(HWND parent = nullptr) {
    // The path helpers in settings.h use SDL; bring SDL up with no subsystems.
    const bool sdlReady = SDL_Init(0) == 0;

    ConfigUI ui;
    ui.settings = ssaver::loadSettings();

    HINSTANCE inst = GetModuleHandleW(nullptr);
    const wchar_t* kClass = L"StarfieldSaverConfig";
    WNDCLASSW wc{};
    wc.lpfnWndProc = ConfigWndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kClass;
    RegisterClassW(&wc);

    // Size the window so the client area fits the controls, then centre it.
    RECT rc{0, 0, 316, 188};
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRectEx(&rc, style, FALSE, WS_EX_DLGMODALFRAME);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int yy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kClass,
                                L"Starfield Saver — Settings", style, x, yy,
                                w, h, parent, nullptr, inst, &ui);
    if (!hwnd) {
        if (sdlReady) SDL_Quit();
        return 0;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Standard modal-ish loop; IsDialogMessage gives Tab navigation plus
    // Enter (default = Save) and Esc (IDCANCEL) handling.
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (sdlReady) SDL_Quit();
    return 0;
}

} // namespace

int RunWindowsScreensaver(int argc, char** argv) {
    // Default (double-clicked with no args) -> configuration.
    if (argc < 2) return RunConfig();

    // Arguments look like "/s", "/p", "/c" or "/c:1234"; the flag may use a
    // colon or pass the HWND as the next argv entry.
    const char* arg = argv[1];
    char flag = (arg[0] == '/' || arg[0] == '-') ? arg[1] : arg[0];

    switch (flag) {
        case 's': case 'S':
            return RunFullscreen();
        case 'p': case 'P': {
            // HWND may be argv[2] or embedded as "/p:1234".
            const char* h = (arg[2] == ':') ? arg + 3
                          : (argc >= 3 ? argv[2] : nullptr);
            return RunPreview(ParseHwnd(h));
        }
        case 'c': case 'C':
        default: {
            // The configuration parent HWND may be embedded ("/c:1234") or
            // passed as the next argv entry; it's optional.
            const char* h = (arg[2] == ':') ? arg + 3
                          : (argc >= 3 ? argv[2] : nullptr);
            return RunConfig(ParseHwnd(h));
        }
    }
}

#endif // _WIN32
