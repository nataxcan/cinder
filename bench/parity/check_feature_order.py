#!/usr/bin/env python3
"""Compare Cinder's features-per-step order with the vanilla jar's.

    python3 bench/parity/check_feature_order.py

Runs bench/parity/FeatureOrderProbe.java (needs the extracted data pack, see
bench/parity/probe.sh) and bench/parity/sorter_probe.c, then diffs the two
listings line by line. The order matters: a placed feature's index within its
step is what setFeatureSeed hashes, so any reordering moves that feature.
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
PACK = Path(os.environ.get("VANILLA_PACK", "/home/nataxcan/dev/vanilla-probe/pack"))
NOISE = re.compile(r"^(WARNING|\[|Non-directory|\s*$)")


def java_side() -> list[str]:
    out = subprocess.run(
        ["bash", str(HERE / "probe26.sh"), "FeatureOrderProbe", str(PACK)],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    return [l for l in out.splitlines() if not NOISE.match(l) and not l.startswith("#")]


def c_side() -> list[str]:
    tmp = Path("/tmp/sorter_probe_out.txt")
    subprocess.run(
        ["bash", str(HERE / "cprobe.sh"), "bench/parity/sorter_probe.c"],
        capture_output=True,
        text=True,
        check=True,
        cwd=str(ROOT),
    )
    # cprobe prints to stdout; re-run capturing it directly instead of a file
    out = subprocess.run(
        ["bash", str(HERE / "cprobe.sh"), "bench/parity/sorter_probe.c"],
        capture_output=True,
        text=True,
        check=True,
        cwd=str(ROOT),
    ).stdout
    if tmp.exists():
        tmp.unlink()
    return [l for l in out.splitlines() if not NOISE.match(l) and not l.startswith("#")]


def main() -> int:
    a = java_side()
    b = c_side()
    print(f"vanilla lines: {len(a)}   cinder lines: {len(b)}")
    first = None
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            first = i
            break
    if first is None and len(a) == len(b):
        print("feature order matches the jar exactly")
        return 0
    if first is None:
        print(f"common prefix matches; lengths differ ({len(a)} vs {len(b)})")
        first = min(len(a), len(b))
    print(f"first difference at line {first}:")
    for j in range(max(0, first - 4), min(max(len(a), len(b)), first + 5)):
        ja = a[j] if j < len(a) else "-"
        ci = b[j] if j < len(b) else "-"
        mark = "  " if ja == ci else "->"
        print(f"{mark} vanilla: {ja}")
        print(f"   cinder : {ci}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
