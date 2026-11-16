# Reference oracle frames

Captures of the *original* game (DOSBox, EGA 320x200) used as ground truth for
the drop-in.  They are byte-captures; the drop-in must never load them at run
time — they exist so tests and analysis can score against real behaviour.

`tools/capture_dosbox.sh` regenerates frames from DOSBox (EGA, 320x200 under
Xvfb).  The emulator (`./lof-native`) reproduces many of them; the headless
commands below are the reproducible recipe for the dungeon frames.

## Dungeon frames (Temple save, DNG05)

`dungeon_slot1.png` is the Temple spawn view; `dungeon_slot1_moved.png` is the
same party after stepping forward.  The emulator reaches both deterministically
with the extended-key key feed (`LOF_KEYAUTO`); the arrows are delivered as the
DOS extended-key pair (AL=0 then the scan code), so the party turns and steps.

```sh
# Temple spawn (O, "1" -> load slot 1): the party's first-person viewport.
LOF_CMDLEN=3 LOF_NO_TIMER=1 LOF_KEYAUTO=0x18,0x02 LOF_KEYGAP=8 \
    ./lof-native --dir game --headless --frames 1500 --shot still.png

# Same, plus two "Up" (0x48) steps forward -> the *moved* frame.
LOF_CMDLEN=3 LOF_NO_TIMER=1 LOF_KEYAUTO=0x18,0x02,0x48,0x48 LOF_KEYGAP=8 \
    ./lof-native --dir game --headless --frames 1500 --shot moved.png
```

Both runs are deterministic (repeat md5-identical).  Scoring the viewport window
(the `DP001` sprite blitted at 10,11) against the oracles:

| emulator frame | vs `dungeon_slot1.png` | vs `dungeon_slot1_moved.png` |
|----------------|------------------------|------------------------------|
| spawn          | 62.7 %                 | 16.7 %                       |
| two steps fwd  | 16.7 %                 | **80.9 %**                   |

So the moved frame is reproduced by *live movement* — the `dungeon_slot1_moved`
state is reachable with keys, not only by patching the DNG tile layer
(`tools/dngs.py patch game/DNGS/DNG05 17 4 0`), which was the earlier workaround.
This is the reproducible command BLK-A004 asked for; BLK-A004 is resolved.
