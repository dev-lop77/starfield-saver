# Starfield Saver — build for Linux (dev) and Windows (.scr via mingw-w64).
#
#   make            # native Linux build  -> build/starfield-saver
#   make run        # build + run the dev window
#   make windows    # cross-compile       -> build/StarfieldSaver.scr
#   make clean
#
# The Windows target needs the SDL2 mingw development libraries. Fetch them with:
#   make sdl2-mingw
# which downloads & extracts them under third_party/ (no sudo needed).

# ------------------------------------------------------------------ sources ---
SRC := $(wildcard src/*.cpp src/events/*.cpp)
# screensaver_win.cpp is guarded by #ifdef _WIN32, so it is harmless on Linux
# (compiles to an empty translation unit).

CXXFLAGS_COMMON := -std=c++17 -O2 -Wall -Wextra -Isrc

# ------------------------------------------------------------------- native ---
NATIVE_CXX   := g++
NATIVE_OUT   := build/starfield-saver
NATIVE_FLAGS := $(CXXFLAGS_COMMON) $(shell pkg-config --cflags sdl2)
NATIVE_LIBS  := $(shell pkg-config --libs sdl2) -lGL

# ------------------------------------------------------------------ windows ---
WIN_CXX    := x86_64-w64-mingw32-g++
WIN_RC     := x86_64-w64-mingw32-windres
SDL2_MINGW := third_party/SDL2/x86_64-w64-mingw32
WIN_OUT    := build/StarfieldSaver.scr
# Version resource (Properties -> Details on Windows). Windows-only; compiled by
# windres into build/version.o and linked into the .scr. Bump src/version.rc on
# each release.
WIN_RES_SRC := src/version.rc
WIN_RES_OBJ := build/version.o
# -mwindows => GUI subsystem (no console window). Fully static link so the .scr
# is a SINGLE self-contained file (no SDL2.dll, no libgcc/libstdc++ DLLs) — a
# screensaver must be one file the user can drop into System32 / right-click
# Install. We link libSDL2.a BY PATH (not -lSDL2, which would prefer the import
# lib libSDL2.dll.a) and then spell out SDL2's own Win32 dependencies, which a
# static SDL no longer pulls in for us. This dependency list is the canonical
# one from `sdl2-config --static-libs` (third_party/SDL2/.../bin/sdl2-config);
# if you bump SDL2_VER, re-check it from that script. -lopengl32 is OURS (CRT +
# GLSL events). Order matters for static linking: SDL2.a before its deps.
WIN_FLAGS  := $(CXXFLAGS_COMMON) -I$(SDL2_MINGW)/include/SDL2 -Dmain=SDL_main
WIN_LIBS   := -L$(SDL2_MINGW)/lib -lmingw32 -lSDL2main $(SDL2_MINGW)/lib/libSDL2.a \
              -mwindows -lopengl32 \
              -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va \
              -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 \
              -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid \
              -static -static-libgcc -static-libstdc++ -s

# --------------------------------------------------------------------- rules ---
.PHONY: all run audition windows clean sdl2-mingw

all: $(NATIVE_OUT)

$(NATIVE_OUT): $(SRC)
	@mkdir -p build
	$(NATIVE_CXX) $(NATIVE_FLAGS) $(SRC) -o $@ $(NATIVE_LIBS)

run: $(NATIVE_OUT)
	./$(NATIVE_OUT)

# Eye review: open the dev window and cycle through every event back-to-back,
# one at a time. Press N / Right to skip to the next; ESC to quit. Each event's
# name is printed to the console.
audition: $(NATIVE_OUT)
	./$(NATIVE_OUT) --audition

windows: $(SRC) $(WIN_RES_SRC)
	@test -d $(SDL2_MINGW) || { echo "SDL2 mingw libs missing. Run: make sdl2-mingw"; exit 1; }
	@mkdir -p build
	$(WIN_RC) $(WIN_RES_SRC) -O coff -o $(WIN_RES_OBJ)
	$(WIN_CXX) $(WIN_FLAGS) $(SRC) $(WIN_RES_OBJ) -o $(WIN_OUT) $(WIN_LIBS)
	@rm -f build/SDL2.dll
	@echo "Built $(WIN_OUT) — single self-contained file, ship it ALONE."
	@if command -v x86_64-w64-mingw32-objdump >/dev/null 2>&1; then \
	  echo "Non-system DLL imports (should be NONE besides KERNEL32/USER32/GDI32/OPENGL32/etc.):"; \
	  x86_64-w64-mingw32-objdump -p $(WIN_OUT) | grep -i 'DLL Name' | grep -iv -E 'kernel32|user32|gdi32|opengl32|winmm|imm32|ole32|oleaut32|shell32|setupapi|version|advapi32|msvcrt|shlwapi|rpcrt4|comdlg32|comctl32|ws2_32|hid|dwmapi|cfgmgr32' || echo "  (none — fully self-contained)"; \
	fi

# Download the official SDL2 mingw development libraries into third_party/.
SDL2_VER := 2.30.0
sdl2-mingw:
	@mkdir -p third_party
	@echo "Fetching SDL2 $(SDL2_VER) mingw devel libraries..."
	curl -L -o third_party/sdl2-mingw.tar.gz \
	  https://github.com/libsdl-org/SDL/releases/download/release-$(SDL2_VER)/SDL2-devel-$(SDL2_VER)-mingw.tar.gz
	tar -xzf third_party/sdl2-mingw.tar.gz -C third_party
	rm -rf third_party/SDL2
	mv third_party/SDL2-$(SDL2_VER) third_party/SDL2
	rm -f third_party/sdl2-mingw.tar.gz
	@echo "SDL2 mingw libs ready under third_party/SDL2"

clean:
	rm -rf build
