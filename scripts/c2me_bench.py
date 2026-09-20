#!/usr/bin/env python3
"""Time Fabric 26.2 + C2ME generating 1024 chunks (32x32), then compare."""
from __future__ import annotations

import json
import os
import queue
import re
import signal
import statistics
import struct
import subprocess
import threading
import time
from pathlib import Path

JAVA = Path("/home/nataxcan/jdk-25/bin/java")
RUN_DIR = Path("/home/nataxcan/dev/c2me-run")
JAR = RUN_DIR / "fabric-server.jar"
OUT_JSON = Path("/mnt/c/Users/nataxcan/Documents/dev/cinder/bench/c2me_results.json")
OUT_MD = Path("/mnt/c/Users/nataxcan/Documents/dev/cinder/bench/C2ME.md")
PORT = "25567"

FORCELOADS = [
    "forceload add -256 -256 -1 -1",
    "forceload add -256 0 -1 255",
    "forceload add 0 -256 255 -1",
    "forceload add 0 0 255 255",
]
TARGET_CHUNKS = 1024
JAVA_CMD = [str(JAVA), "-Xms2G", "-Xmx4G", "-jar", str(JAR), "nogui"]


def count_mca_chunks(region_dir: Path) -> int:
    n = 0
    if not region_dir.is_dir():
        return 0
    for p in region_dir.glob("*.mca"):
        data = p.read_bytes()
        if len(data) < 4096:
            continue
        for i in range(0, 4096, 4):
            loc = struct.unpack_from(">I", data, i)[0]
            if loc != 0:
                n += 1
    return n


def kill_tree(proc: subprocess.Popen) -> None:
    if proc.poll() is not None:
        return
    try:
        os.killpg(proc.pid, signal.SIGTERM)
    except (ProcessLookupError, PermissionError, OSError):
        proc.terminate()
    try:
        proc.wait(timeout=30)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError, OSError):
            proc.kill()
        proc.wait(timeout=10)


def wipe_world() -> None:
    world = RUN_DIR / "world"
    if world.exists():
        subprocess.check_call(["rm", "-rf", str(world)])


def write_eula_and_props() -> None:
    (RUN_DIR / "eula.txt").write_text("eula=true\n")
    props = RUN_DIR / "server.properties"
    text = props.read_text() if props.exists() else ""

    def setp(key: str, val: str) -> None:
        nonlocal text
        line = f"{key}={val}"
        if re.search(rf"^{re.escape(key)}=", text, re.M):
            text = re.sub(rf"^{re.escape(key)}=.*$", line, text, flags=re.M)
        else:
            text += ("\n" if text and not text.endswith("\n") else "") + line + "\n"

    setp("online-mode", "false")
    setp("spawn-protection", "0")
    setp("view-distance", "2")
    setp("simulation-distance", "2")
    setp("sync-chunk-writes", "false")
    setp("max-players", "1")
    setp("motd", "C2ME bench")
    setp("server-port", PORT)
    setp("network-compression-threshold", "-1")
    setp("white-list", "false")
    setp("max-tick-time", "-1")
    setp("pause-when-empty-seconds", "0")
    props.write_text(text)


def tune_c2me() -> None:
    """Pin C2ME workers to 8 (physical cores). Generated file appears after first boot."""
    candidates = [
        RUN_DIR / "config" / "c2me.toml",
        RUN_DIR / "config" / "c2me" / "c2me.toml",
    ]
    for p in candidates:
        if not p.exists():
            continue
        text = p.read_text()
        text = re.sub(
            r"^globalExecutorParallelism\s*=\s*.*$",
            "globalExecutorParallelism = 8",
            text,
            flags=re.M,
        )
        text = re.sub(
            r"^enabled\s*=\s*.*$",
            "enabled = true",
            text,
            count=1,
            flags=re.M,
        )
        if re.search(r"^\[threadedWorldGen\]", text, re.M):
            text = re.sub(
                r"(^\[threadedWorldGen\][\s\S]*?^enabled\s*=\s*).*$",
                r"\1true",
                text,
                count=1,
                flags=re.M,
            )
        p.write_text(text)
        print(f"   tuned {p}", flush=True)
        return
    print("   no c2me.toml yet", flush=True)


def boot_once_for_files() -> None:
    """First launch generates configs + downloads vanilla jar; not timed."""
    wipe_world()
    write_eula_and_props()
    (RUN_DIR / "mods").mkdir(exist_ok=True)
    proc = subprocess.Popen(
        JAVA_CMD,
        cwd=str(RUN_DIR),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        start_new_session=True,
        bufsize=1,
    )
    assert proc.stdout is not None
    deadline = time.perf_counter() + 300
    done = False
    while time.perf_counter() < deadline:
        line = proc.stdout.readline()
        if not line:
            if proc.poll() is not None:
                break
            continue
        print("   init", line.rstrip(), flush=True)
        if "Done (" in line:
            done = True
            break
    if proc.stdin:
        try:
            proc.stdin.write("stop\n")
            proc.stdin.flush()
        except BrokenPipeError:
            pass
    try:
        proc.wait(timeout=90)
    except subprocess.TimeoutExpired:
        kill_tree(proc)
    write_eula_and_props()
    tune_c2me()
    if not done:
        raise RuntimeError("C2ME Fabric init did not reach Done")


def region_dir() -> Path:
    modern = RUN_DIR / "world" / "dimensions" / "minecraft" / "overworld" / "region"
    legacy = RUN_DIR / "world" / "region"
    if modern.is_dir():
        return modern
    return legacy


def timed_pregen() -> dict:
    wipe_world()
    t_spawn = time.perf_counter()
    proc = subprocess.Popen(
        JAVA_CMD,
        cwd=str(RUN_DIR),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        start_new_session=True,
        bufsize=1,
    )
    assert proc.stdout is not None and proc.stdin is not None
    lines: queue.Queue[str] = queue.Queue()

    def reader() -> None:
        for line in proc.stdout:
            lines.put(line)
            print("   c2me", line.rstrip(), flush=True)
        lines.put("")

    threading.Thread(target=reader, daemon=True).start()

    t_done = None
    t_cmd = None
    t_chunks = None
    chunks = 0
    last_report = 0.0
    last_save = 0.0
    try:
        deadline = time.perf_counter() + 600
        while time.perf_counter() < deadline:
            try:
                line = lines.get(timeout=0.25)
            except queue.Empty:
                line = None
            if line == "":
                raise RuntimeError("C2ME Fabric exited before Done")
            if line and t_done is None and "Done (" in line:
                t_done = time.perf_counter()
                for cmd in FORCELOADS:
                    proc.stdin.write(cmd + "\n")
                    print(f"   sent {cmd}", flush=True)
                proc.stdin.flush()
                t_cmd = time.perf_counter()
                last_save = t_cmd
            if t_cmd is not None:
                now = time.perf_counter()
                if now - last_save >= 2.0:
                    try:
                        proc.stdin.write("save-all flush\n")
                        proc.stdin.flush()
                    except BrokenPipeError:
                        pass
                    last_save = now
                chunks = count_mca_chunks(region_dir())
                if now - last_report >= 2.0:
                    print(f"   chunks={chunks} t={now - t_cmd:.1f}s", flush=True)
                    last_report = now
                if chunks >= TARGET_CHUNKS:
                    t_chunks = now
                    break
        if t_done is None:
            raise RuntimeError("C2ME Fabric never printed Done")
        if t_chunks is None:
            print(f"   timeout with chunks={chunks}", flush=True)
            t_chunks = time.perf_counter()
        try:
            proc.stdin.write("save-all flush\n")
            proc.stdin.flush()
            time.sleep(2)
            proc.stdin.write("stop\n")
            proc.stdin.flush()
        except BrokenPipeError:
            pass
        try:
            proc.wait(timeout=90)
        except subprocess.TimeoutExpired:
            kill_tree(proc)
        chunks = count_mca_chunks(region_dir())
        return {
            "done_ms": (t_done - t_spawn) * 1000,
            "pregen_ms": (t_chunks - t_cmd) * 1000 if t_cmd else None,
            "total_ms": (t_chunks - t_spawn) * 1000,
            "chunks": chunks,
            "target": TARGET_CHUNKS,
        }
    except Exception:
        kill_tree(proc)
        raise


def main() -> int:
    if not JAVA.exists():
        raise SystemExit(f"missing {JAVA}")
    if not JAR.exists():
        raise SystemExit(f"missing {JAR} — run scripts/fetch_c2me.sh")
    mods = list((RUN_DIR / "mods").glob("c2me-*.jar"))
    if not mods:
        raise SystemExit(f"missing C2ME jar in {RUN_DIR}/mods")
    cfg = RUN_DIR / "config" / "c2me.toml"
    if cfg.exists() and (RUN_DIR / "server.properties").exists():
        print("== skip init, configs present", flush=True)
        write_eula_and_props()
        tune_c2me()
    else:
        print("== C2ME Fabric init (untimed)", flush=True)
        boot_once_for_files()
    print("== C2ME timed 1024-chunk pregen", flush=True)
    samples = []
    for i in range(3):
        print(f"== run {i+1}/3", flush=True)
        samples.append(timed_pregen())
        print(f"   {samples[-1]}", flush=True)

    def med(key: str) -> float:
        return statistics.median(s[key] for s in samples if s.get(key) is not None)

    out = {
        "c2me": "0.4.2-alpha.0.52+26.2",
        "fabric_loader": "0.19.5",
        "fabric_api": "0.161.0+26.2",
        "lithium": "0.25.3+mc26.2",
        "java": "25",
        "target_chunks": TARGET_CHUNKS,
        "samples": samples,
        "median_done_ms": med("done_ms"),
        "median_pregen_ms": med("pregen_ms"),
        "median_total_ms": med("total_ms"),
        "cinder_best_ms": 21.4,
        "cinder_median_4t_ms": 21.6,
        "pumpkin_best_8t_ms": 4807.0,
        "pumpkin_median_8t_ms": 5027.9,
        "folia_median_pregen_ms": 11889.282471000002,
        "folia_median_done_ms": 10658.78494399999,
        "folia_median_total_ms": 23321.500000000015,
        "best_pregen_ms": min(s["pregen_ms"] for s in samples if s.get("pregen_ms") is not None),
    }
    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUT_JSON.write_text(json.dumps(out, indent=2))

    pre = out["median_pregen_ms"]
    done = out["median_done_ms"]
    tot = out["median_total_ms"]
    best = out["best_pregen_ms"]
    rows = []
    for i, s in enumerate(samples, 1):
        rows.append(
            f"| {i} | {s['done_ms']/1000:.1f} s | {s['pregen_ms']/1000:.2f} s | {s['total_ms']/1000:.1f} s |"
        )
    md = f"""# World generation — Cinder vs Pumpkin vs Folia vs C2ME

Same machine: AMD Ryzen 7 5700G (8 cores / 16 threads), 47 GiB RAM. Same target: **1024 chunks** (32×32).

| | Cinder | Pumpkin | Folia 26.2-7 | C2ME 0.4.2-α.0.52 |
| --- | --- | --- | --- | --- |
| Runtime | native C (Bend) | native Rust | JVM Java 25 | JVM Java 25 + Fabric |
| Terrain | hash heightmap, stone/dirt/grass | vanilla-like 3D noise + features | vanilla (caves, ores, biomes) | vanilla (parity by default) |
| Threads used | 4 (best) | 8 (best) | Moonrise 8 workers + Folia regions | C2ME 8 workers |
| Persistence | in-memory fingerprint | in-memory | Anvil region files | Anvil region files |
| 1024-chunk gen | **21.4 ms** | **4.81 s** | **11.9 s** | **{pre/1000:.2f} s** |
| vs Cinder | 1× | 225× slower | 550× slower | **{pre/out['cinder_median_4t_ms']:.0f}× slower** |
| JVM/`Done` only | — | 0.43 s listen, no spawn chunks | **10.7 s** | **{done/1000:.1f} s** |
| Spawn → 1024 chunks | **44 ms** (gen then ping) | ~5.2 s if you gen then boot | **23.3 s** | **{tot/1000:.1f} s** |

[C2ME](https://github.com/RelativityMC/C2ME-fabric) is RelativityMC’s Fabric concurrent chunk engine (gen, I/O, loading). On this pregen it is **{out['folia_median_pregen_ms']/pre:.2f}× faster** than Folia and **{pre/out['pumpkin_median_8t_ms']:.2f}× slower** than Pumpkin’s in-memory generator. It is not close to Cinder’s heightmap world.

## C2ME samples (3 cold worlds)

| Run | `Done` | forceload → 1024 on disk | spawn → 1024 |
| ---: | ---: | ---: | ---: |
{chr(10).join(rows)}
| median | {done/1000:.1f} s | {pre/1000:.2f} s | {tot/1000:.1f} s |

Stack: C2ME `0.4.2-alpha.0.52+26.2`, Fabric loader 0.19.5, Fabric API `0.161.0+26.2`, Lithium `0.25.3+mc26.2` (C2ME’s recommended pair). Java 25 Temurin, `-Xms2G -Xmx4G`, `globalExecutorParallelism = 8`. Port 25567. Best pregen {best/1000:.2f} s.

`forceload` is capped at 256 chunks per command, so the bench issues the same four 16×16 quadrants as Folia. 26.2 writes overworld Anvil at `world/dimensions/minecraft/overworld/region/`.

No c2me-ocl. That is a separate OpenCL accelerator; this run is CPU C2ME.

## Reproduce

```bash
bash scripts/fetch_c2me.sh
python3 scripts/c2me_bench.py
```
"""
    OUT_MD.write_text(md)
    print(md, flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
