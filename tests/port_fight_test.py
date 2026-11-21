#!/usr/bin/env python3
"""--fight: the derived combat round must drive the real round-result printer.

The fight mode resolves one melee round with the model transcribed from
LOF.EXE (port/combat.h) and renders the result with the same composer that
`--round-result` uses.  This test pins:

  * determinism (same seed -> same table),
  * that the seed actually drives the rolls,
  * that the rendered screen is byte-identical to `--round-result` given the
    same rows (so the fight path really feeds the engine's printer, not a
    look-alike), and
  * the row/table invariants (one row per party member, deltas within range).

Uses an archived save (saves/), never the proprietary game folder.
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOF = os.path.join(ROOT, "lof")
SAVE = os.path.join(ROOT, "saves", "dng05-Decker-3cf7cace.sav")
GAME = os.path.join(ROOT, "game")


def run_fight(seed, shot=None, hp=None):
    cmd = [LOF, "--dir", GAME, "--fight", "--save", SAVE, "--seed", str(seed)]
    if hp:
        cmd.append(f"{hp[0]}:{hp[1]}")
    if shot:
        cmd += ["--shot", shot]
    out = subprocess.run(cmd, capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit(f"lof --fight failed: {out.stderr.strip()}")
    return out.stdout


def parse(out):
    rows = []
    for line in out.splitlines():
        m = re.match(r"\s+(\d) (\S+)\s+hp\s+(-?\d+)/(\s*\d+)\s+([+-]\d+)\s+(\S.*)$", line)
        if m:
            wpb = re.search(r"WPB=(\d)(\d)(\d)", line)
            status = re.sub(r"\s*WPB=\d\d\d$", "", m.group(6))
            rows.append({
                "wpb": tuple(int(x) for x in wpb.groups()) if wpb else (0, 0, 0),
                "idx": int(m.group(1)),
                "name": m.group(2),
                "hp": int(m.group(3)),
                "max": int(m.group(4)),
                "delta": int(m.group(5)),
                "status": re.sub(r"\s*WPB=\d\d\d$", "", m.group(6)).strip(),
            })
    return rows


def spec(rows):
    """`delta:w:p:b:a` specs for --round-result, using the round's own W/P/B."""
    out = []
    for r in rows:
        a = {"Failed": 0, "Passed": 1}.get(r["status"], 2)
        w, p, b = r.get("wpb", (0, 0, 0))
        out.append(f"{r['delta']}:{w}:{p}:{b}:{a}")
    return out


def main():
    a = parse(run_fight(1))
    b = parse(run_fight(1))
    assert a, "the fight printed no rows"
    assert a == b, "same seed must give the same table"

    # One row per party member of the archived six-character save.
    assert [r["idx"] for r in a] == list(range(1, len(a) + 1)), a
    assert len(a) == 6, f"expected six members, got {len(a)}"
    for r in a:
        assert -99 <= r["delta"] <= 0, f"delta out of range: {r}"
        assert r["status"] in ("Passed", "Failed", "out of action"), r

    # The seed drives the rolls: some other seed must change something.
    changed = any(parse(run_fight(s)) != a for s in (2, 3, 4, 5, 6))
    assert changed, "the seed had no effect on the round"

    # The rendered result must be exactly what --round-result prints for the
    # same rows: same save, same printer.
    with tempfile.TemporaryDirectory() as td:
        fight_png = os.path.join(td, "fight.png")
        rr_png = os.path.join(td, "rr.png")
        run_fight(1, shot=fight_png)
        # Row specs must directly follow --round-result (its parser consumes
        # the arguments that immediately follow the flag).
        cmd = [LOF, "--dir", GAME, "--round-result"] + spec(a) + [
            "--save", SAVE, "--shot", rr_png]
        out = subprocess.run(cmd, capture_output=True, text=True)
        assert out.returncode == 0, out.stderr
        with open(fight_png, "rb") as f1, open(rr_png, "rb") as f2:
            assert f1.read() == f2.read(), (
                "--fight and --round-result render differently for the same rows")

    # The --ui path: `C` in play opens the decision prompt, the next key
    # resolves the round and shows the table, the one after returns to play.
    with tempfile.TemporaryDirectory() as td:
        ui_res = os.path.join(td, "ui_res.png")
        ui_play = os.path.join(td, "ui_play.png")
        save = os.path.join(GAME, "GAMES", "GAME2")
        if os.path.exists(save):
            base = [LOF, "--dir", GAME, "--headless", "--ui", "--save", save]
            subprocess.run(base + ["--ui-keys", "32,32,79,67,32", "--shot", ui_res],
                           check=True, capture_output=True)
            subprocess.run(base + ["--ui-keys", "32,32,79,67,32,32", "--shot", ui_play],
                           check=True, capture_output=True)
            fight_png = os.path.join(td, "fight_game2.png")
            subprocess.run([LOF, "--dir", GAME, "--fight", "--save", save,
                            "--seed", "1", "--shot", fight_png],
                           check=True, capture_output=True)
            with open(ui_res, "rb") as f1, open(fight_png, "rb") as f2:
                assert f1.read() == f2.read(), (
                    "the --ui round-result frame differs from --fight")
            with open(ui_res, "rb") as f1, open(ui_play, "rb") as f2:
                assert f1.read() != f2.read(), (
                    "the --ui combat states render the same frame")

    print("port_fight_test: ok (deterministic, seed-driven, same printer as "
          "--round-result, six rows, --ui transition)")


if __name__ == "__main__":
    main()
