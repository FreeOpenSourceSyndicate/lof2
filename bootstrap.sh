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

# --- 2. build ---------------------------------------------------------------
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
