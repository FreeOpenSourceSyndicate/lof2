# VOR.EXE port — status (2026-11-09)

`VOR.EXE` is the pre-loader `START.BAT` runs before `LOF.EXE`.  It is not only a
copy-protection stub: it also displays the animated dragon title
(`EGA/IN/DRAG`) and the credits still (`EGA/IN/CRED`).  The drop-in `port/vor.cpp`
reproduces this without running `VOR.EXE`.

## What the port gets right

Both port frames are **byte-identical to the decoded game data**:

| port frame | source file | match |
|------------|-------------|-------|
| frame 0    | `EGA/IN/DRAG` | 100.0% |
| frame 1    | `EGA/IN/CRED` | 100.0% |

Verified with `tools/ega.py`'s `indices()` against
`./vor --dir game --headless --shot … --shot-frame 0|1`.

## Open item: credits palette

A DOSBox capture of the credits (`tools/capture_dosbox.sh … --wait 24`) matches
the `CRED` image at **81.3%**.  The 18.7% difference is a *palette* remap, not a
layout error — the port and the decoded `CRED` agree 100%, and the mismatch is a
consistent index translation (e.g. `CRED` index 0 ↔ DOSBox index 6, index 4/6/12
↔ 0).  Either `VOR` programs a custom attribute palette for the stills, or DOSBox
applies a different default EGA palette for this program.  Resolving it needs an
INT 10h AH=10h (set palette) trace of `VOR.EXE`.

## The emulator's VOR frame is not the DOSBox one

`./lof-native --vor` draws only the left ~half of `DRAG` and fills the rest with
the menu scene — the same partial-render class of bug as the play screen (see
`docs/notes-maintainer-town.md`).  Do not use the emulator's VOR frame as the
oracle; use a DOSBox capture.

## Reproduce

```sh
make vor
./vor --dir game --headless --shot /tmp/vor0.png --shot-frame 0
./vor --dir game --headless --shot /tmp/vor1.png --shot-frame 1
tools/capture_dosbox.sh /tmp/vorc --wait 24 --post 2   # credits under DOSBox
```
