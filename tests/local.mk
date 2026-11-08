# Per-agent tests. Each agent owns its own line; keep them independent.
# Example:  LOCAL_TEST_CMDS = ./dungeon_test
LOCAL_TEST_CMDS = $(CXX) -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -I port -o char_test tests/char_test.cpp port/char.cpp port/data.cpp && ./char_test
