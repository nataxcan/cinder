#!/usr/bin/env python3
"""Untrack development scratch that the ignore rules did not catch in time.

Rules added to .gitignore after a file is tracked do nothing, so this both adds
the missing rules and removes the paths from the index (files stay on disk).
"""
from __future__ import annotations

import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]

RULES = [
    ("/logs/", "server logs"),
    ("bench/parity/_fix_*.py", "one-shot edit scripts"),
    ("bench/parity/_patch*.py", "one-shot edit scripts"),
    ("bench/parity/_insert_names.py", "one-shot edit scripts"),
    ("bench/parity/_repair_names.py", "one-shot edit scripts"),
    ("bench/parity/_dbg*.py", "one-shot debug scripts"),
    ("/.*.sh", "local scratch helpers"),
    ("/.tags/", "local tag dump"),
    ("/.trees.txt", "local scratch dump"),
]


def main() -> int:
    gi = ROOT / ".gitignore"
    text = gi.read_text()
    added = []
    for rule, why in RULES:
        if rule not in text:
            added.append(rule)
    if added:
        text = text.rstrip() + "\n\n# Development scratch (kept out of the published tree)\n"
        text += "\n".join(added) + "\n"
        gi.write_text(text)
        print(f"gitignore: added {added}")

    tracked = subprocess.run(
        ["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout.split()
    drop = []
    for path in tracked:
        if path.startswith("logs/"):
            drop.append(path)
            continue
        name = pathlib.Path(path).name
        if path.startswith("bench/parity/") and (
            name.startswith("_fix_")
            or name.startswith("_patch")
            or name.startswith("_dbg")
            or name in ("_insert_names.py", "_repair_names.py")
        ):
            drop.append(path)
        elif path.startswith(".") and path.endswith(".sh"):
            drop.append(path)
    if drop:
        subprocess.run(["git", "rm", "-r", "--cached", "-q", *drop], cwd=ROOT, check=True)
        print(f"untracked {len(drop)} path(s), e.g. {drop[:3]}")
    else:
        print("nothing to untrack")

    # what is left, by top-level area
    left = subprocess.run(
        ["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout.split()
    areas: dict[str, int] = {}
    for path in left:
        areas[path.split("/")[0]] = areas.get(path.split("/")[0], 0) + 1
    print("tracked by area:", dict(sorted(areas.items(), key=lambda kv: -kv[1])))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
