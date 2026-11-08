#!/usr/bin/env bash
# check.sh — verify the repo is in a healthy, non-poisoned state.
# Safe to run any time; changes nothing. Exit 0 = healthy.
set -u
cd "$(dirname "$0")"
fail=0
ok()   { printf '  ok   %s\n' "$*"; }
bad()  { printf '  FAIL %s\n' "$*"; fail=1; }

echo "[check] repository"
# The reliable identity is the git remote, not the directory name.
remote_url="$(git remote get-url origin 2>/dev/null || echo '')"
if printf '%s' "$remote_url" | grep -qE 'FreeOpenSourceSyndicate/lof2(\.git)?$'; then
    ok "origin is lof2 (the correct repo)"
elif [ "$(basename "$(git rev-parse --show-toplevel)")" = "lof2" ]; then
    ok "repo directory is lof2"
else
    printf '  note origin is %s (expected .../lof2) — confirm this is not the sibling repo\n' "${remote_url:-<none>}"
fi

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

echo "[check] oracle golden frames"
if ./tests/golden_test.sh 2>&1 | grep -q 'golden_test: ok'; then
    ok "golden_test passes (emulator output unchanged)"
elif ./tests/golden_test.sh 2>&1 | grep -q 'SKIP'; then
    printf '  note golden_test skipped (no game data)\n'
else
    bad "golden_test failed — the emulator output changed; see tests/golden_test.sh"
fi

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
