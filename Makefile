# Native Legend of Faerghail (DOS -> Ubuntu) build.
#
#   make            build lof-native (SDL2 window, falls back to headless)
#   make headless   build without SDL2
#   make test       build and run the CPU self-test
CXX      ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter
SRC       = src/cpu.cpp src/ega.cpp src/machine.cpp src/main.cpp

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)

ifneq ($(SDL_LIBS),)
CXXFLAGS += -DUSE_SDL $(SDL_CFLAGS)
LDLIBS    = $(SDL_LIBS)
endif

all: lof-native

lof-native: $(SRC) src/emu.h
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(LDLIBS)

headless: $(SRC) src/emu.h
	$(CXX) -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -o lof-native $(SRC)

test: tests/cpu_test.cpp src/cpu.cpp src/ega.cpp src/machine.cpp src/emu.h
	$(CXX) $(CXXFLAGS) -I src -o cpu_test tests/cpu_test.cpp src/cpu.cpp src/ega.cpp src/machine.cpp
	./cpu_test

# --- Native drop-in (the deliverable) -------------------------------------
# These binaries re-implement the engine and read the game data files; they do
# NOT run LOF.EXE / VOR.EXE.
PORT_SRC  = port/gfx.cpp port/lof.cpp port/vor.cpp
PORT_CFLAGS = -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -I port
PORT_SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
PORT_SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)
ifneq ($(PORT_SDL_LIBS),)
PORT_CFLAGS += -DUSE_SDL $(PORT_SDL_CFLAGS)
PORT_LIBS    = $(PORT_SDL_LIBS)
endif

port: lof vor

lof: port/lof.cpp port/gfx.cpp port/data.cpp port/gfx.h port/data.h
	$(CXX) $(PORT_CFLAGS) -o $@ port/lof.cpp port/gfx.cpp port/data.cpp $(PORT_LIBS)

vor: port/vor.cpp port/gfx.cpp port/gfx.h
	$(CXX) $(PORT_CFLAGS) -o $@ port/vor.cpp port/gfx.cpp $(PORT_LIBS)

# Install the native drop-in into the game folder under the names START.BAT uses.
dropin: lof vor
	cp lof game/lof
	cp vor game/vor

clean:
	rm -f lof-native cpu_test lof vor port/*.o

.PHONY: all headless test dropin port clean
