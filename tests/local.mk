# Per-agent tests. Each agent owns its own line; keep them independent.
# Example:  LOCAL_TEST_CMDS = ./dungeon_test
# (Agent A — dungeons) DNGS decoder.
LOCAL_TEST_CMDS = $(CXX) -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -I port \
    -o dungeon_test tests/dungeon_test.cpp port/dungeon.cpp port/gfx.cpp && \
    ./dungeon_test game/DNGS
