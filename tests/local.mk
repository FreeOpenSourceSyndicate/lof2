# Per-agent tests. Each agent owns its own line; keep them independent.
# Example:  LOCAL_TEST_CMDS = ./dungeon_test
# (Agent A — dungeons) DNGS decoder.
DUNGEON_TEST = $(CXX) -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -I port \
    -o dungeon_test tests/dungeon_test.cpp port/dungeon.cpp port/gfx.cpp && \
    ./dungeon_test game/DNGS; rc=$$?; rm -f dungeon_test; exit $$rc
# (Agent B — characters/items) SPEC/ROST readers.
CHAR_TEST = $(CXX) -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -I port -o char_test tests/char_test.cpp port/char.cpp port/data.cpp && ./char_test

LOCAL_TEST_CMDS = $(DUNGEON_TEST) && $(CHAR_TEST)
