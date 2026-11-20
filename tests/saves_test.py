#!/usr/bin/env python3
"""saves_test — the saves/ archive and its no-overwrite ingest scheme.

Guards the contract that lets owner uploads be filed without ever losing an
existing save:

  * every file in saves/ is a valid Legend of Faerghail save;
  * saves/manifest.tsv matches the files on disk (md5 + location + party);
  * filenames are content-addressed (`<map>-<party-lead>-<md5[0:8]>.sav`) and
    unique;
  * the well-known test fixtures exist with their expected content hash;
  * tools/ingest-save.sh, run against a temp copy of the tree, (a) skips a
    byte-identical upload, (b) files a *different* save for the same slot under
    a new name without touching the old file, and (c) rejects a non-save.

Run via `make test` (tests/local.mk) or directly.  No game data or oracle PNGs
needed: it works on saves/ alone and a temp directory.
"""
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAVES = os.path.join(ROOT, "saves")
sys.path.insert(0, os.path.join(ROOT, "tools"))
from saveinfo import parse, slug  # noqa: E402

# Fixtures the tests and bootstrap rely on: filename -> md5.
FIXTURES = {
    "dng05-Decker-3cf7cace.sav": "3cf7cacea0966c635a04f46ae6d12a68",
    "dng00-Conan-d8b1cad7.sav": "d8b1cad7d654b091968a12a71dd40a29",
    "dng05-Gonk-b2b765be.sav": "b2b765be6b5ff4e9243f4a729e507190",
    "dng05-Conan-e6c8c8bb.sav": "e6c8c8bb73d02e9b995010491b42367a",
}

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)


def md5(path):
    return hashlib.md5(open(path, "rb").read()).hexdigest()


def test_archive():
    files = sorted(f for f in os.listdir(SAVES) if f.endswith(".sav"))
    check(bool(files), "saves/ has no .sav files")
    seen_hashes = {}
    for f in files:
        path = os.path.join(SAVES, f)
        info = parse(path)
        check(info is not None, "%s is not a valid save" % f)
        if info is None:
            continue
        # Filename is <map>-<lead>-<md5[0:8]>.sav and matches the content.
        check(f == "%s-%s.sav" % (slug(info), info["md5"][:8]),
              "%s is not content-addressed (expected %s-%s.sav)"
              % (f, slug(info), info["md5"][:8]))
        check(info["md5"] not in seen_hashes,
              "duplicate content: %s == %s" % (f, seen_hashes.get(info["md5"])))
        seen_hashes[info["md5"]] = f


def test_fixtures():
    for name, want in FIXTURES.items():
        path = os.path.join(SAVES, name)
        check(os.path.exists(path), "missing fixture %s" % name)
        if os.path.exists(path):
            check(md5(path) == want, "fixture %s md5 changed" % name)


def test_manifest():
    mpath = os.path.join(SAVES, "manifest.tsv")
    check(os.path.exists(mpath), "missing saves/manifest.tsv")
    if not os.path.exists(mpath):
        return
    rows = {}
    for line in open(mpath):
        if line.startswith("#") or not line.strip():
            continue
        cols = line.rstrip("\n").split("\t")
        rows[cols[0]] = cols
    for f in sorted(x for x in os.listdir(SAVES) if x.endswith(".sav")):
        check(f in rows, "manifest missing row for %s" % f)
        if f not in rows:
            continue
        info = parse(os.path.join(SAVES, f))
        check(rows[f][1] == info["md5"], "manifest md5 stale for %s" % f)
        check(rows[f][3] == info["map"], "manifest location stale for %s" % f)


def test_ingest_no_overwrite():
    """Run the ingest script against a temp tree; prove nothing is clobbered."""
    seed = next((os.path.join(SAVES, f) for f in sorted(os.listdir(SAVES))
                 if f.endswith(".sav")), None)
    check(seed is not None, "no .sav file to seed the ingest test")
    if seed is None:
        return
    tmp = tempfile.mkdtemp(prefix="saves_test_")
    try:
        # A minimal tree: the script only needs tools/ and saves/.
        os.makedirs(os.path.join(tmp, "tools"))
        os.makedirs(os.path.join(tmp, "saves", "inbox"))
        for n in ("saveinfo.py", "ingest-save.sh"):
            shutil.copy(os.path.join(ROOT, "tools", n), os.path.join(tmp, "tools", n))
        shutil.copy(seed, os.path.join(tmp, "saves", "seed.sav"))
        script = os.path.join(tmp, "tools", "ingest-save.sh")
        os.chmod(script, 0o755)
        original = os.path.join(tmp, "saves", "seed.sav")
        orig_md5 = md5(original)

        def run(*args):
            return subprocess.run([script] + list(args), cwd=tmp,
                                  capture_output=True, text=True)

        # (a) byte-identical upload -> skipped, no new file.
        shutil.copy(original, os.path.join(tmp, "saves", "inbox", "GAME4"))
        r = run()
        check("DUP" in r.stderr, "identical upload was not skipped: %s" % r.stderr)
        check(len([f for f in os.listdir(os.path.join(tmp, "saves"))
                   if f.endswith(".sav")]) == 1,
              "identical upload created a duplicate file")

        # (b) different content for the same slot -> new name, old untouched.
        variant = os.path.join(tmp, "saves", "inbox", "GAME4")
        data = bytearray(open(original, "rb").read())
        data[0x9A6] = (data[0x9A6] + 1) & 0xFF  # change the facing byte
        open(variant, "wb").write(data)
        r = run()
        check("ADDED" in r.stderr, "new variant was not filed: %s" % r.stderr)
        check(md5(original) == orig_md5, "existing save was modified by ingest")
        savs = [f for f in os.listdir(os.path.join(tmp, "saves")) if f.endswith(".sav")]
        check(len(savs) == 2, "expected 2 archived saves, got %d" % len(savs))

        # (c) a non-save is rejected and not written.
        junk = os.path.join(tmp, "saves", "inbox", "GAME7")
        open(junk, "wb").write(b"not a save at all")
        r = run()
        check("SKIP" in r.stderr or "not a Legend" in r.stderr,
              "junk file was not rejected: %s" % r.stderr)
        check(len([f for f in os.listdir(os.path.join(tmp, "saves"))
                   if f.endswith(".sav")]) == 2,
              "junk file was written into saves/")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    test_archive()
    test_fixtures()
    test_manifest()
    test_ingest_no_overwrite()
    if failures:
        for f in failures:
            print("saves_test: FAIL (%s)" % f)
        return 1
    print("saves_test: ok (archive content-addressed, fixtures intact, "
          "ingest never overwrites)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
