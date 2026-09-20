#!/usr/bin/env python3
"""Time Folia 26.2 generating 1024 chunks (32x32), then compare to Cinder/Pumpkin."""
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
FOLIA_DIR = Path("/home/nataxcan/dev/folia-run")
JAR = FOLIA_DIR / "folia.jar"
CINDER_WORLD = Path("/mnt/c/Users/nataxcan/Documents/dev/cinder/bench/WORLD.md")
OUT_JSON = Path("/mnt/c/Users/nataxcan/Documents/dev/cinder/bench/folia_results.json")
OUT_MD = Path("/mnt/c/Users/nataxcan/Documents/dev/cinder/bench/FOLIA.md")

# Vanilla forceload allows at most 256 chunks per command. Four 16x16
# quadrants make 32x32 = 1024 chunks. Coords are block columns.
FORCELOADS = [
    "forceload add -256 -256 -1 -1",
    "forceload add -256 0 -1 255",
    "forceload add 0 -256 255 -1",
    "forceload add 0 0 255 255",
]
TARGET_CHUNKS = 1024


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
        proc.wait(timeout=20)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError, OSError):
            proc.kill()
        proc.wait(timeout=10)


def wipe_world() -> None:
    world = FOLIA_DIR / "world"
    if world.exists():
        subprocess.check_call(["rm", "-rf", str(world)])


def tune_paper_threads() -> None:
    """More chunk workers and 1s autosave so Anvil counts aren't stuck on a 5 min flush."""
    mapping = {
        "config/paper-global.yml": [
            (r"worker-threads:\s*-?\d+", "worker-threads: 8"),
            (r"io-threads:\s*-?\d+", "io-threads: 4"),
        ],
        "config/paper-world-defaults.yml": [
            (r"auto-save-interval:\s*.*", "auto-save-interval: 20"),
            (r"flush-regions-on-save:\s*.*", "flush-regions-on-save: true"),
            (r"max-auto-save-chunks-per-tick:\s*\d+", "max-auto-save-chunks-per-tick: 512"),
        ],
        "bukkit.yml": [
            (r"autosave:\s*\d+", "autosave: 20"),
        ],
    }
    for rel, subs in mapping.items():
        p = FOLIA_DIR / rel
        if not p.exists():
            continue
        text = p.read_text()
        for pat, repl in subs:
            text = re.sub(pat, repl, text)
        p.write_text(text)


def write_eula_and_props() -> None:
    (FOLIA_DIR / "eula.txt").write_text("eula=true\n")
    props = FOLIA_DIR / "server.properties"
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
    setp("motd", "Folia bench")
    setp("server-port", "25566")
    setp("network-compression-threshold", "-1")
    setp("white-list", "false")
    props.write_text(text)


def boot_once_for_files() -> None:
    """First launch generates configs; not timed."""
    wipe_world()
    write_eula_and_props()
    proc = subprocess.Popen(
        [str(JAVA), "-Xms2G", "-Xmx4G", "-jar", str(JAR), "nogui"],
        cwd=str(FOLIA_DIR),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        start_new_session=True,
        bufsize=1,
    )
    assert proc.stdout is not None
    deadline = time.perf_counter() + 180
    done = False
    while time.perf_counter() < deadline:
        line = proc.stdout.readline()
        if not line:
            if proc.poll() is not None:
                break
            continue
        print("   init", line.rstrip(), flush=True)
        if "Done (" in line or "Done!" in line or re.search(r"Done \(", line):
            done = True
            break
    if proc.stdin:
        try:
            proc.stdin.write("stop\n")
            proc.stdin.flush()
        except BrokenPipeError:
            pass
    try:
        proc.wait(timeout=60)
    except subprocess.TimeoutExpired:
        kill_tree(proc)
    write_eula_and_props()
    tune_paper_threads()
    if not done:
        raise RuntimeError("Folia init did not reach Done")


def timed_pregen() -> dict:
    wipe_world()
    region = FOLIA_DIR / "world" / "dimensions" / "minecraft" / "overworld" / "region"
    region_legacy = FOLIA_DIR / "world" / "region"
    t_spawn = time.perf_counter()
    proc = subprocess.Popen(
        [str(JAVA), "-Xms2G", "-Xmx4G", "-jar", str(JAR), "nogui"],
        cwd=str(FOLIA_DIR),
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
            print("   folia", line.rstrip(), flush=True)
        lines.put("")

    threading.Thread(target=reader, daemon=True).start()

    def region_dir() -> Path:
        if region.is_dir():
            return region
        return region_legacy

    t_done = None
    t_cmd = None
    t_chunks = None
    chunks = 0
    last_report = 0.0
    try:
        deadline = time.perf_counter() + 600
        while time.perf_counter() < deadline:
            try:
                line = lines.get(timeout=0.25)
            except queue.Empty:
                line = None
            if line == "":
                raise RuntimeError("Folia exited before Done")
            if line and t_done is None and "Done (" in line:
                t_done = time.perf_counter()
                for cmd in FORCELOADS:
                    proc.stdin.write(cmd + "\n")
                    print(f"   sent {cmd}", flush=True)
                proc.stdin.flush()
                t_cmd = time.perf_counter()
            if t_cmd is not None:
                chunks = count_mca_chunks(region_dir())
                now = time.perf_counter()
                if now - last_report >= 2.0:
                    print(f"   chunks={chunks} t={now - t_cmd:.1f}s", flush=True)
                    last_report = now
                if chunks >= TARGET_CHUNKS:
                    t_chunks = now
                    break
        if t_done is None:
            raise RuntimeError("Folia never printed Done")
        if t_chunks is None:
            print(f"   timeout with chunks={chunks}", flush=True)
            t_chunks = time.perf_counter()
        try:
            proc.stdin.write("save-all\n")
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
        raise SystemExit(f"missing {JAR}")
    print("== Folia init (untimed)", flush=True)
    boot_once_for_files()
    print("== Folia timed 1024-chunk pregen", flush=True)
    samples = []
    for i in range(3):
        print(f"== run {i+1}/3", flush=True)
        samples.append(timed_pregen())
        print(f"   {samples[-1]}", flush=True)

    def med(key: str) -> float:
        return statistics.median(s[key] for s in samples if s.get(key) is not None)

    out = {
        "folia": "26.2-7",
        "java": "25.0.4.1",
        "target_chunks": TARGET_CHUNKS,
        "samples": samples,
        "median_done_ms": med("done_ms"),
        "median_pregen_ms": med("pregen_ms"),
        "median_total_ms": med("total_ms"),
        "cinder_best_ms": 21.4,
        "cinder_median_4t_ms": 21.6,
        "pumpkin_best_8t_ms": 4807.0,
        "pumpkin_median_8t_ms": 5027.9,
    }
    OUT_JSON.write_text(json.dumps(out, indent=2))

    md = []
    md.append("# World generation — Cinder vs Pumpkin vs Folia")
    md.append("")
    md.append("Same target: **1024 chunks** (32×32). AMD Ryzen 7 5700G (8c/16t).")
    md.append("")
    md.append("| Server | What ran | Best / median |")
    md.append("| --- | --- | ---: |")
    md.append("| **Cinder** | Heightmap world, 384-block columns, 4 threads | **21.4 / 21.6 ms** |")
    md.append("| **Pumpkin** | Vanilla-like `generate_single_chunk(Full)`, 8 threads | **4.81 / 5.03 s** |")
    md.append(
        f"| **Folia 26.2-7** | JVM start to `Done` (almost no spawn chunks) | "
        f"{out['median_done_ms']/1000:.2f} s |"
    )
    md.append(
        f"| **Folia 26.2-7** | `forceload` 32×32 after Done | "
        f"{out['median_pregen_ms']/1000:.2f} s |"
    )
    md.append(
        f"| **Folia 26.2-7** | Process spawn → 1024 chunks on disk | "
        f"{out['median_total_ms']/1000:.2f} s |"
    )
    md.append("")
    if out["median_pregen_ms"]:
        md.append(
            f"Folia pregen / Cinder = **{out['median_pregen_ms']/out['cinder_median_4t_ms']:.0f}×** slower. "
            f"Folia pregen / Pumpkin = **{out['median_pregen_ms']/out['pumpkin_median_8t_ms']:.2f}×**."
        )
    md.append("")
    md.append("Notes:")
    md.append("")
    md.append("- Folia is Paper’s region-threaded Java server (Java 25, vanilla terrain + caves + features).")
    md.append("- Cinder is a heightmap world, not vanilla noise. Pumpkin is vanilla-like and in-memory (no Anvil write). Folia writes Anvil region files.")
    md.append("- Folia `Done` is JVM + registries + datapacks; it is not 1024 chunks. Pregen is `forceload add -256 -256 255 255`.")
    md.append("")
    OUT_MD.write_text("\n".join(md) + "\n")
    print("\n".join(md), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
