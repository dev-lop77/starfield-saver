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
SDL2_MINGW := third_party/SDL2/x86_64-w64-mingw32
WIN_OUT    := build/StarfieldSaver.scr
# -mwindows => GUI subsystem (no console window). Static libstdc++/gcc so the
# .scr only needs SDL2.dll beside it.
WIN_FLAGS  := $(CXXFLAGS_COMMON) -I$(SDL2_MINGW)/include/SDL2 -Dmain=SDL_main
WIN_LIBS   := -L$(SDL2_MINGW)/lib -lmingw32 -lSDL2main -lSDL2 \
              -lopengl32 -mwindows -static-libgcc -static-libstdc++

# --------------------------------------------------------------------- rules ---
.PHONY: all run windows clean sdl2-mingw

all: $(NATIVE_OUT)

$(NATIVE_OUT): $(SRC)
	@mkdir -p build
	$(NATIVE_CXX) $(NATIVE_FLAGS) $(SRC) -o $@ $(NATIVE_LIBS)

run: $(NATIVE_OUT)
	./$(NATIVE_OUT)

windows: $(SRC)
	@test -d $(SDL2_MINGW) || { echo "SDL2 mingw libs missing. Run: make sdl2-mingw"; exit 1; }
	@mkdir -p build
	$(WIN_CXX) $(WIN_FLAGS) $(SRC) -o $(WIN_OUT) $(WIN_LIBS)
	@cp $(SDL2_MINGW)/bin/SDL2.dll build/ 2>/dev/null || true
	@echo "Built $(WIN_OUT) (ship it together with build/SDL2.dll)"

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
