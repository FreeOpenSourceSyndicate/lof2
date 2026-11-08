#!/usr/bin/env bash
# check.sh — verify the repo is in a healthy, non-poisoned state.
# Safe to run any time; changes nothing. Exit 0 = healthy.
set -u
cd "$(dirname "$0")"
fail=0
ok()   { printf '  ok   %s\n' "$*"; }
bad()  { printf '  FAIL %s\n' "$*"; fail=1; }

echo "[check] repository"
[ "$(basename "$(git rev-parse --show-toplevel)")" = "lof2" ] \
    && ok "repo is lof2 (not the poisoned sibling 'lof')" \
    || bad "repo directory is not named lof2 — are you in the sibling repo?"

# Poison guard: no remote pointing at the sibling repo.
if git remote -v 2>/dev/null | grep -qE 'FreeOpenSourceSyndicate/lof(\.git)?([[:space:]]|$)'; then
    bad "a git remote points at the poisoned sibling repo FreeOpenSourceSyndicate/lof"
else
    ok "no git remote points at the poisoned sibling repo"
fi

echo "[check] game data"
if [ -f game/LOF.EXE ] && [ -d game/EGA ] && [ -d game/SYST ]; then
    ok "game data present"
else
    bad "game data missing — run ./bootstrap.sh"
fi

echo "[check] build"
if make -s >/dev/null 2>&1; then ok "make (emulator) builds"; else bad "make failed"; fi
if make -s port >/dev/null 2>&1; then ok "make port (drop-in) builds"; else bad "make port failed"; fi

echo "[check] tests"
if make -s test >/dev/null 2>&1; then ok "make test passes"; else bad "make test failed"; fi

echo "[check] hygiene"
if [ -n "$(git status --porcelain)" ]; then
    printf '  note uncommitted changes present (commit + push before you stop)\n'
else
    ok "working tree clean"
fi
if [ -z "$(git log --oneline @{u}..HEAD 2>/dev/null)" ]; then
    ok "no unpushed commits"
else
    printf '  note unpushed commits present — push them (reset risk)\n'
fi

echo
[ "$fail" = 0 ] && echo "[check] HEALTHY" || echo "[check] PROBLEMS FOUND"
exit $fail
