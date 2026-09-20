#!/usr/bin/env python3
import json
import statistics
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from world_bench import cinder_world, PUMPKIN_GEN, pumpkin_world, OUT, BENCH

def main() -> int:
    print("== Cinder vanilla Full 1024 chunks", flush=True)
    cinder = []
    for th, reps in ((4, 1), (8, 3), (16, 1)):
        cinder.append(cinder_world(th, reps))
    pumpkin = None
    if PUMPKIN_GEN.exists():
        print("== Pumpkin Full 1024 @ 8 threads (1 sample, prior medians kept)", flush=True)
        pumpkin = pumpkin_world(32, 8, 1)
    out = {
        "cinder": cinder,
        "pumpkin_8t_this_run": pumpkin,
        "prior": {
            "pumpkin_best_8t_ms": 4807.0,
            "pumpkin_median_8t_ms": 5027.9,
            "c2me_median_pregen_ms": 8618.6,
            "folia_median_pregen_ms": 11889.3,
        },
    }
    OUT.write_text(json.dumps(out, indent=2))
    print(json.dumps(out, indent=2), flush=True)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
