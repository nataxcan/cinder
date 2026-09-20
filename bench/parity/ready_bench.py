#!/usr/bin/env python3
"""Server-ready measurement: process spawn -> "world ready" line (1024 chunks),
plus a status ping to the freshly started server.

    python3 bench/parity/ready_bench.py [threads ...]
"""
from __future__ import annotations

import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))


def run_once(threads: int, ping: bool) -> float:
    t0 = time.perf_counter()
    proc = subprocess.Popen(
        [str(ROOT / "cinder"), "--threads", str(threads)],
        cwd=str(ROOT), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1, start_new_session=True,
    )
    ready = None
    assert proc.stdout is not None
    for raw in proc.stdout:
        if "world ready" in raw:
            ready = time.perf_counter() - t0
            print(f"   threads={threads} ready={ready*1000:.0f} ms  {raw.strip()}", flush=True)
            if ping:
                subprocess.run([sys.executable, str(ROOT / "scripts" / "ping.py"),
                                "127.0.0.1", "25565"], check=False)
            break
    try:
        proc.kill()
    except ProcessLookupError:
        pass
    proc.wait(timeout=10)
    if ready is None:
        raise RuntimeError("cinder never printed world ready")
    return ready


def main() -> int:
    threads = [int(a) for a in sys.argv[1:]] or [8, 16]
    for t in threads:
        run_once(t, ping=(t == threads[-1]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
