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

test: tests/cpu_test.cpp src/cpu.cpp src/ega.cpp src/machine.cpp src/emu.h tests/font_test.cpp port/font.cpp port/gfx.cpp port/gfx.h port/font.h
	$(CXX) $(CXXFLAGS) -I src -o cpu_test tests/cpu_test.cpp src/cpu.cpp src/ega.cpp src/machine.cpp
	./cpu_test
	$(CXX) -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -I port -o font_test tests/font_test.cpp port/font.cpp port/gfx.cpp
	./font_test
	@$(MAKE) --no-print-directory local-test

golden-port:
	$(MAKE) -s port
	python3 tests/port_menu_test.py
	python3 tests/port_play_test.py
	python3 tests/port_town_test.py
	python3 tests/port_emporium_test.py
	python3 tests/port_ui_test.py
	python3 tests/port_walk_test.py
	python3 tests/port_vor_test.py

# The drop-in must run with the original EXEs absent (hard constraint #1).
golden-noexe:
	$(MAKE) -s port
	python3 tests/port_noexe_test.py

# --- Per-agent tests -------------------------------------------------------
# Agents add their own test commands to tests/local.mk (which they own) instead
# of editing this Makefile.  LOCAL_TEST_CMDS is a list of shell commands.
-include tests/local.mk
LOCAL_TEST_CMDS ?=
local-test:
	@$(LOCAL_TEST_CMDS)

# Health check: repo name, poison guard, build, tests, git hygiene.
check:
	./check.sh

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

lof: port/lof.cpp port/gfx.cpp port/data.cpp port/screens.cpp port/font.cpp port/char.cpp port/dungeon.cpp port/gfx.h port/data.h port/screens.h port/font.h port/char.h port/dungeon.h
	$(CXX) $(PORT_CFLAGS) -o $@ port/lof.cpp port/gfx.cpp port/data.cpp port/screens.cpp port/font.cpp port/char.cpp port/dungeon.cpp $(PORT_LIBS)

vor: port/vor.cpp port/gfx.cpp port/screens.cpp port/font.cpp port/gfx.h port/screens.h port/font.h
	$(CXX) $(PORT_CFLAGS) -o $@ port/vor.cpp port/gfx.cpp port/screens.cpp port/font.cpp $(PORT_LIBS)

# Regenerate the annotated disassembly listings (needs lof-native for the
# LZEXE-unpacked VOR payload).
asm: lof-native
	python3 tools/decompile.py lof
	python3 tools/decompile.py vor

# Install the native drop-in into the game folder under the names START.BAT uses.
dropin: lof vor
	cp lof game/lof
	cp vor game/vor

clean:
	rm -f lof-native cpu_test lof vor port/*.o

.PHONY: all headless test asm dropin port clean check local-test golden golden-port golden-noexe

# Oracle regression: emulator must render the same frames (needs game data).
golden:
	./tests/golden_test.sh
