#!/usr/bin/env bash
# bootstrap.sh — restore a working build from a fresh checkout.
#
# The game data (game/, downloads/) is proprietary and git-ignored, so a fresh
# clone or a reset workspace has NO game files.  This script re-downloads them
# from the Internet Archive (the same copy the project was developed against)
# and builds everything.  It is idempotent: re-running is safe.
#
# Usage:  ./bootstrap.sh          # download (if missing) + build
#         ./bootstrap.sh --check  # report what is missing, change nothing
set -u

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

ARCHIVE_ITEM="msdos_Legend_of_Faerghail_1990"
ARCHIVE_ZIP="Legend_of_Faerghail_1990.zip"
WANT_MD5="52eedcec208a1ae4677c6abf5ce57eb0"
URL="https://archive.org/download/${ARCHIVE_ITEM}/${ARCHIVE_ZIP}"

CHECK=0
[ "${1:-}" = "--check" ] && CHECK=1

say() { printf '%s\n' "$*" >&2; }

missing=0

# --- 1. game data -----------------------------------------------------------
if [ -f game/LOF.EXE ] && [ -d game/EGA ] && [ -d game/SYST ]; then
    say "[bootstrap] game data present (game/LOF.EXE, game/EGA, game/SYST)"
else
    missing=1
    if [ "$CHECK" = 1 ]; then
        say "[bootstrap] MISSING game data (game/) — run ./bootstrap.sh to fetch it"
    else
        say "[bootstrap] game data missing — downloading from Internet Archive"
        mkdir -p downloads
        if [ ! -f "downloads/$ARCHIVE_ZIP" ]; then
            if ! curl -fL --retry 3 -o "downloads/$ARCHIVE_ZIP.part" "$URL"; then
                say "[bootstrap] FAILED to download $URL"
                say "[bootstrap] If the network is blocked, copy the zip manually to"
                say "[bootstrap]   downloads/$ARCHIVE_ZIP  (md5 $WANT_MD5)  and re-run."
                exit 2
            fi
            mv "downloads/$ARCHIVE_ZIP.part" "downloads/$ARCHIVE_ZIP"
        fi
        got="$(md5sum "downloads/$ARCHIVE_ZIP" | awk '{print $1}')"
        if [ "$got" != "$WANT_MD5" ]; then
            say "[bootstrap] WARNING: md5 $got != expected $WANT_MD5 (continuing)"
        fi
        rm -rf downloads/ext_msdos && mkdir -p downloads/ext_msdos
        if command -v unzip >/dev/null 2>&1; then
            unzip -q -o "downloads/$ARCHIVE_ZIP" -d downloads/ext_msdos
        else
            python3 -c "import zipfile,sys; zipfile.ZipFile('downloads/$ARCHIVE_ZIP').extractall('downloads/ext_msdos')"
        fi
        # The archive nests the files under a folder (usually 'legfaer'); find it.
        src="$(dirname "$(find downloads/ext_msdos -iname 'LOF.EXE' | head -1)")"
        [ -n "$src" ] || { say "[bootstrap] LOF.EXE not found in archive"; exit 3; }
        rm -rf game && mkdir -p game
        cp -a "$src"/. game/
        # Dummy disk-id files satisfy VOR's disk check for a hard-disk install.
        for n in 1 2 3; do : > "game/disk$n.id"; done
        say "[bootstrap] game data restored to game/"
    fi
fi

# The engine opens saves as GAMES\GAME%3d (printf "%3d"), e.g. "GAME  2" with
# two padding spaces.  The emulator's path resolver strips those spaces (DOS
# ignores them), so the shipped GAME1..GAME5 are found directly and no renamed
# copies are needed.
#
# A GAME1 dropped at the repo root is a *working* save (slot 1 = the Temple
# dungeon, slot 2 = the wilderness); it is NOT committed because saves are
# proprietary game data.  Install it as slot 1 when present.
if [ -f GAME1 ]; then
    cp -f GAME1 game/GAMES/GAME1
    say "[bootstrap] installed root GAME1 as save slot 1"
fi

# GAME4 at the repo root is the owner-supplied *Cyldane* save (the party stands
# in front of Cyldane, not Thyn).  It is not in the archive, so it must be
# installed here or it is lost on a workspace reset.  It is what lets the town
# test pin the town-name selector (G-001/OT-001), so `tests/port_town_test.py`
# reads it from the repo root directly.  Install it into slot 4 too, for manual
# play.
if [ -f GAME4 ]; then
    cp -f GAME4 game/GAMES/GAME4
    say "[bootstrap] installed root GAME4 as save slot 4 (Cyldane)"
fi

# A GAME5 dropped at the repo root is the owner-supplied *dungeon* save (a full
# six-character party standing in a dungeon where wandering monsters spawn, so
# it is the reliable way to trigger combat).  It is not in the archive, so it
# must be installed here or it is lost on a workspace reset.
if [ -f GAME5 ]; then
    cp -f GAME5 game/GAMES/GAME5
    say "[bootstrap] installed root GAME5 as save slot 5 (dungeon, monsters spawn)"
fi

# --- 2. tooling -------------------------------------------------------------
# The verification tests (`make golden-port`, `check.sh`) need Python numpy +
# Pillow; the interactive front-end builds an SDL2 window when libsdl2-dev is
# present.  Install what is missing, best effort (never fatal: the drop-in
# itself builds and runs without either).
have_py_deps() { python3 -c "import numpy, PIL" >/dev/null 2>&1; }
if ! have_py_deps; then
    if [ "$CHECK" = 1 ]; then
        say "[bootstrap] MISSING Python deps (numpy, Pillow) — needed by make golden-port"
    else
        say "[bootstrap] installing Python deps (numpy, Pillow) ..."
        pip install --quiet numpy pillow >/dev/null 2>&1 \
            || python3 -m pip install --quiet numpy pillow >/dev/null 2>&1 \
            || say "[bootstrap] WARNING: could not install numpy/Pillow (frame tests will fail)"
    fi
fi
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists sdl2 2>/dev/null; then
    if [ "$CHECK" != 1 ]; then
        say "[bootstrap] note: SDL2 dev files absent — building the head-less front-end"
        say "[bootstrap]       (install libsdl2-dev + pkg-config for a live window)"
    fi
fi

# --- 3. build ---------------------------------------------------------------
if [ "$CHECK" = 1 ]; then
    [ "$missing" = 0 ] && say "[bootstrap] everything present" || true
    exit 0
fi

say "[bootstrap] building (make) ..."
make -s            || { say "[bootstrap] 'make' failed"; exit 4; }
say "[bootstrap] building drop-in binaries (make port) ..."
make -s port       || { say "[bootstrap] 'make port' failed"; exit 5; }
say "[bootstrap] running tests (make test) ..."
make -s test       || { say "[bootstrap] 'make test' failed"; exit 6; }
say "[bootstrap] OK — ./lof-native (emulator oracle), ./lof and ./vor (drop-in)"
say "[bootstrap] AGENTS: read docs/RULES.md (owner rules + changelog) and docs/GUESSWORK.md (guess register) before working."
