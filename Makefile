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

# Copy the emulator into the game folder under the names START.BAT uses, so it can
# be dropped in place of the originals.
dropin: lof-native
	cp lof-native game/lof
	cp lof-native game/vor

clean:
	rm -f lof-native cpu_test

.PHONY: all headless test dropin clean
