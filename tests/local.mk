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
LOCAL_TEST_CMDS = \
    ( $(DUNGEON_TEST) ) && \
    ( $(CHAR_TEST) )
