# Per-agent tests. Each agent owns its own line; keep them independent.
# Example:  LOCAL_TEST_CMDS = ./dungeon_test
# (Agent A — dungeons) DNGS decoder.
DUNGEON_TEST = $(CXX) -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -I port \
    -o dungeon_test tests/dungeon_test.cpp port/dungeon.cpp port/gfx.cpp && \
    ./dungeon_test game/DNGS; rc=$$?; rm -f dungeon_test; exit $$rc
# (Agent B — characters/items) SPEC/ROST readers.
CHAR_TEST = $(CXX) -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -I port \
    -o char_test tests/char_test.cpp port/char.cpp port/data.cpp && \
    ./char_test; rc=$$?; rm -f char_test; exit $$rc

# Each agent's block is wrapped in a subshell so one line's `exit` (used to
# clean up its binary and propagate its status) cannot terminate the recipe and
# silently skip the agents that follow.  Fail-fast is preserved: a non-zero
# status short-circuits the `&&`.
# (Maintainer — shared/engine) EGA image decoder: the byte-1/skip-10 image path
# and the byte-11 font path, plus a header-straddling run (the CH25 truncation).
EGA_DECODE_TEST = $(CXX) -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -I port \
    -o ega_decode_test tests/ega_decode_test.cpp port/gfx.cpp && \
    ./ega_decode_test; rc=$$?; rm -f ega_decode_test; exit $$rc

# (Maintainer — combat) the derived melee formula, generator and round model.
COMBAT_TEST = $(CXX) -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -I port \
    -o combat_test tests/combat_test.cpp port/combat.cpp && \
    ./combat_test; rc=$$?; rm -f combat_test; exit $$rc

LOCAL_TEST_CMDS = \
    ( $(DUNGEON_TEST) ) && \
    ( $(CHAR_TEST) ) && \
    ( $(EGA_DECODE_TEST) ) && \
    ( $(COMBAT_TEST) ) && \
    ( python3 tests/saves_test.py )
