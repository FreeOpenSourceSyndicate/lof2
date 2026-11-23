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

# --- Owner-supplied saves ---------------------------------------------------
# Owner saves live in saves/ under content-addressed names
# (<map>-<party-lead>-<md5>.sav); see saves/README.md.  They are *not* named
# after a slot, so uploading a new GAME<n> can never overwrite an archived one.
#
# Any new save the owner drops in saves/inbox/ or at the repo root is filed
# into saves/ first (never overwriting anything), then installed into a slot.
if [ -d saves ]; then
    mkdir -p saves/inbox
    if [ "$CHECK" = 1 ]; then
        tools/ingest-save.sh --check || true
    else
        tools/ingest-save.sh || say "[bootstrap] WARNING: save ingest reported a problem"
    fi
fi

# Install archived saves into game/GAMES/ slots for manual play.  These copies
# are a convenience only; tests read the archived files directly, so a slot is
# free to hold whatever the owner last uploaded.
install_slot() {  # <archive-file> <slot-number> <label>
    if [ -f "$1" ]; then
        mkdir -p game/GAMES
        cp -f "$1" "game/GAMES/GAME$2"
        say "[bootstrap] installed $1 as save slot $2 ($3)"
    fi
}
# Stable, well-known fixtures by content hash (see saves/manifest.tsv):
if [ "$CHECK" = 0 ]; then
    install_slot saves/dng05-Decker-3cf7cace.sav 1 "Temple start (archive GAME1)"
    install_slot saves/dng00-Conan-393da5b1.sav  3 "dead char 1 + rich char 2 (Temple revive)"
    install_slot saves/dng00-Conan-d8b1cad7.sav  4 "Cyldane (OT-001 fixture)"
    install_slot saves/dng05-Conan-e6c8c8bb.sav  5 "dungeon, monsters spawn"
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
