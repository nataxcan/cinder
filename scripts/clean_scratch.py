#!/usr/bin/env python3
"""Drop development scratch from the index and add it to .gitignore.

The first `git add -A` ran before the ignore rules existed, so scratch files were
committed and published. This removes them from the index (keeping them on disk)
and makes sure the rules that prevent a repeat are in place.
"""
from __future__ import annotations

import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRATCH = [
    ".blk.sh", ".blk2.sh", ".cs.sh", ".hier.sh", ".map.sh", ".surv.sh",
    ".tags.sh", ".tags2.sh", ".trees.txt", ".tags",
]
IGNORES = """
# Local scratch: helpers pointing at a local decompiled source tree
/.*.sh
/.tags/
/.trees.txt

# Server logs
/logs/

# One-shot edit scripts from development sessions
bench/parity/_fix_*.py
bench/parity/_patch*.py
bench/parity/_insert_names.py
bench/parity/_repair_names.py
"""


def main() -> int:
    gi = ROOT / ".gitignore"
    text = gi.read_text()
    if "Local scratch" not in text:
        gi.write_text(text.rstrip() + "\n" + IGNORES)
        print("gitignore: scratch rules added")

    # tracked scratch (and logs) out of the index, files stay on disk
    tracked = subprocess.run(
        ["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout.split()
    drop = [p for p in tracked if p in SCRATCH or p.startswith(".tags/") or p.startswith("logs/")]
    if drop:
        subprocess.run(["git", "rm", "-r", "--cached", "-q", *drop], cwd=ROOT, check=True)
        print(f"untracked {len(drop)} scratch path(s): {drop[:4]}{'...' if len(drop) > 4 else ''}")
    else:
        print("no tracked scratch paths")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
