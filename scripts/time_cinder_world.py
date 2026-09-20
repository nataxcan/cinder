#!/usr/bin/env python3
import os
import signal
import subprocess
import time
from pathlib import Path

root = Path(__file__).resolve().parent.parent
t0 = time.perf_counter()
p = subprocess.Popen(
    [str(root / "cinder"), "--threads", "8"],
    cwd=str(root),
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True,
    start_new_session=True,
)
assert p.stdout is not None
for line in p.stdout:
    print(line.rstrip(), flush=True)
    if "world ready" in line:
        print(f"WALL_MS={(time.perf_counter() - t0) * 1000:.2f}", flush=True)
        break
try:
    os.killpg(p.pid, signal.SIGTERM)
except OSError:
    p.terminate()
p.wait(timeout=5)
