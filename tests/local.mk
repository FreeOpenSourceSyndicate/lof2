# Per-agent tests. Each agent owns its own line; keep them independent.
# Example:  LOCAL_TEST_CMDS = ./dungeon_test
# (Agent A — dungeons) DNGS decoder.
# Links char.cpp as well: the event tests resolve a level's key table through
# SYST/SPEC, and pinning the record text (rather than just the index) is what
# keeps the selector honest.
DUNGEON_TEST = $(CXX) -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -I port \
    -o dungeon_test tests/dungeon_test.cpp port/dungeon.cpp port/gfx.cpp \
    port/char.cpp port/data.cpp && \
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

# (Agent A — dungeons) the event fire path: a real save, one forward step, and
# the plain-text effect must print the SPEC record the dispatcher selects.  This
# is the end-to-end check that the walk loop consults the event table at all; a
# no-step control asserts the text does not appear without a move.
EVENT_FIRE_TEST = python3 tests/event_fire_test.py
ANSWER_PATH_TEST = python3 tests/answer_path_test.py

# (Agent A — dungeons) the recruit event the player reported, end to end: the
# warrior's question, the speech block it names via +0xc9 (resolved against the
# *scenario's* TXT file, not the index as a file number), and the joining class
# named from SYST/SNSC via +0xeb.
RECRUIT_EVENT_TEST = python3 tests/recruit_event_test.py

# (Agent A — dungeons) the once-only event gate: a record with +0xe4 == 1 and a
# type that takes the 0 flag is skipped entirely when the party's +0x38 union
# already covers its +0xe6 index.  Exactly one shipped record is gated.
ONCE_ONLY_TEST = python3 tests/once_only_test.py

# (Agent A — dungeons) the type-11 teleport event: walking onto a type-11 record
# writes its +0xe4 into the party cell (same map, facing unchanged).  End to end
# on DNG17's Air Elemental (SPEC7[13], +0xe4=629), with a no-step control.
TELEPORT_EVENT_TEST = python3 tests/teleport_event_test.py

# (Maintainer — disassembly) the two address conventions for LOF.EXE.  Mixing
# them silently lands in-image mid-instruction and is what hid the event
# dispatcher's callers for a whole session.
TARGET_CONVENTION_TEST = python3 tests/target_convention_test.py

LOCAL_TEST_CMDS = \
    ( $(DUNGEON_TEST) ) && \
    ( $(CHAR_TEST) ) && \
    ( $(EGA_DECODE_TEST) ) && \
    ( $(COMBAT_TEST) ) && \
    ( $(TARGET_CONVENTION_TEST) ) && \
    ( $(EVENT_FIRE_TEST) ) && \
    ( $(ANSWER_PATH_TEST) ) && \
    ( $(RECRUIT_EVENT_TEST) ) && \
    ( $(ONCE_ONLY_TEST) ) && \
    ( $(TELEPORT_EVENT_TEST) ) && \
    ( python3 tests/saves_test.py )
