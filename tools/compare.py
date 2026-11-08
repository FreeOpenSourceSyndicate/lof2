#!/usr/bin/env python3
"""Compare two 320x200 PNG frames (oracle vs drop-in).

Usage:
    python3 tools/compare.py A.png B.png [--tolerance N]

Prints the fraction of pixels that match exactly and the fraction within
`tolerance` (default 32) per channel, plus the mean absolute error.  Exit code
is 0 if the exact-match fraction is >= 0.98, else 1, so it can gate a test.
"""
import sys


def load(path):
    try:
        from PIL import Image
    except ImportError:
        sys.exit("compare.py needs Pillow (pip install pillow)")
    import numpy as np
    return np.asarray(Image.open(path).convert("RGB")).astype(int)


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    tol = 32
    for a in argv:
        if a.startswith("--tolerance"):
            tol = int(a.split("=", 1)[1]) if "=" in a else tol
    if len(args) < 2:
        sys.exit(__doc__)
    import numpy as np
    a, b = load(args[0]), load(args[1])
    if a.shape != b.shape:
        print("size mismatch: %s vs %s" % (a.shape, b.shape))
        return 1
    exact = (a == b).all(2).mean()
    near = (np.abs(a - b).max(2) <= tol).mean()
    mae = np.abs(a - b).mean()
    print("exact match : %.4f" % exact)
    print("within %3d  : %.4f" % (tol, near))
    print("mean abs err: %.3f" % mae)
    return 0 if exact >= 0.98 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
