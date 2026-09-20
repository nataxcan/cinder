#!/usr/bin/env python3
"""Head-to-head Cinder vs Pumpkin on one machine.

Measures what both servers actually share today:
  - binary size
  - cold start until the first successful status ping
  - idle RSS after a ping
  - sequential status-ping latency
  - concurrent status-ping throughput

Plus Cinder-only parallel heightmap worldgen (Pumpkin has no isolated
worldgen binary). All times are time.perf_counter() on the client side.
"""
from __future__ import annotations

import argparse
import json
import os
import signal
import socket
import statistics
import struct
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BENCH_DIR = ROOT / "bench"
RESULTS = BENCH_DIR / "results.json"


def varint(n: int) -> bytes:
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def pack(pkt_id: int, payload: bytes) -> bytes:
    body = varint(pkt_id) + payload
    return varint(len(body)) + body


def read_varint(sock: socket.socket) -> int:
    n = 0
    shift = 0
    while True:
        b = sock.recv(1)
        if not b:
            raise EOFError("eof in varint")
        n |= (b[0] & 0x7F) << shift
        if b[0] < 0x80:
            return n
        shift += 7
        if shift > 35:
            raise ValueError("varint too long")


def status_ping(host: str, port: int, proto: int, timeout: float = 2.0) -> tuple[str, float]:
    t0 = time.perf_counter()
    s = socket.create_connection((host, port), timeout=timeout)
    try:
        addr = host.encode()
        handshake = (
            varint(proto)
            + varint(len(addr))
            + addr
            + struct.pack(">H", port)
            + varint(1)
        )
        s.sendall(pack(0, handshake))
        s.sendall(pack(0, b""))
        length = read_varint(s)
        if length <= 0 or length > 1_000_000:
            raise ValueError(f"bad length {length}")
        data = b""
        while len(data) < length:
            chunk = s.recv(length - len(data))
            if not chunk:
                raise EOFError("eof in status")
            data += chunk
        elapsed = time.perf_counter() - t0
        i = 0
        n = shift = 0
        while True:
            b = data[i]
            i += 1
            n |= (b & 0x7F) << shift
            if b < 0x80:
                break
            shift += 7
        slen = shift = 0
        while True:
            b = data[i]
            i += 1
            slen |= (b & 0x7F) << shift
            if b < 0x80:
                break
            shift += 7
        text = data[i : i + slen].decode("utf-8", errors="replace")
        return text, elapsed
    finally:
        s.close()


def rss_kib(pid: int) -> int | None:
    try:
        for line in Path(f"/proc/{pid}/status").read_text().splitlines():
            if line.startswith("VmRSS:"):
                return int(line.split()[1])
    except FileNotFoundError:
        return None
    return None


def wait_port(host: str, port: int, timeout: float) -> bool:
    deadline = time.perf_counter() + timeout
    while time.perf_counter() < deadline:
        try:
            s = socket.create_connection((host, port), timeout=0.05)
            s.close()
            return True
        except OSError:
            time.sleep(0.001)
    return False


def kill_tree(proc: subprocess.Popen) -> None:
    if proc.poll() is not None:
        return
    try:
        os.killpg(proc.pid, signal.SIGTERM)
    except (ProcessLookupError, PermissionError):
        proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            proc.kill()
        proc.wait(timeout=3)


def start_server(cmd: list[str], cwd: Path, host: str, port: int, log_path: Path) -> tuple[subprocess.Popen, float]:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log = open(log_path, "wb")
    t0 = time.perf_counter()
    proc = subprocess.Popen(
        cmd,
        cwd=str(cwd),
        stdout=log,
        stderr=subprocess.STDOUT,
        start_new_session=True,
    )
    if not wait_port(host, port, timeout=30):
        log.close()
        kill_tree(proc)
        raise RuntimeError(f"server never bound {host}:{port}: {cmd}\n{log_path.read_text(errors='replace')[-2000:]}")
    # First successful Minecraft ping is "ready", not just TCP accept.
    last_err = None
    for proto in (776, 767, 0):
        deadline = time.perf_counter() + 20
        while time.perf_counter() < deadline:
            try:
                status_ping(host, port, proto if proto else 776, timeout=0.5)
                ready = time.perf_counter() - t0
                log.close()
                return proc, ready
            except Exception as e:
                last_err = e
                if proc.poll() is not None:
                    log.close()
                    raise RuntimeError(
                        f"server exited {proc.returncode}: {log_path.read_text(errors='replace')[-2000:]}"
                    ) from e
                time.sleep(0.002)
    log.close()
    kill_tree(proc)
    raise RuntimeError(f"server bound but never answered status ping: {last_err}")


def pct(xs: list[float], p: float) -> float:
    if not xs:
        return float("nan")
    ys = sorted(xs)
    i = min(len(ys) - 1, max(0, int(round((p / 100) * (len(ys) - 1)))))
    return ys[i]


def summarize_ms(xs: list[float]) -> dict:
    ms = [x * 1000 for x in xs]
    return {
        "n": len(ms),
        "min_ms": min(ms),
        "p50_ms": statistics.median(ms),
        "p95_ms": pct(ms, 95),
        "p99_ms": pct(ms, 99),
        "max_ms": max(ms),
        "mean_ms": statistics.fmean(ms),
    }


def sequential_pings(host: str, port: int, proto: int, n: int, warmup: int) -> dict:
    for _ in range(warmup):
        status_ping(host, port, proto)
    times = []
    body = None
    for _ in range(n):
        body, dt = status_ping(host, port, proto)
        times.append(dt)
    return {"body": body, **summarize_ms(times)}


def concurrent_pings(host: str, port: int, proto: int, n: int, workers: int) -> dict:
    t0 = time.perf_counter()
    ok = 0
    err = 0
    times = []
    with ThreadPoolExecutor(max_workers=workers) as pool:
        futs = [pool.submit(status_ping, host, port, proto, 5.0) for _ in range(n)]
        for f in as_completed(futs):
            try:
                _, dt = f.result()
                times.append(dt)
                ok += 1
            except Exception:
                err += 1
    wall = time.perf_counter() - t0
    out = {
        "ok": ok,
        "err": err,
        "wall_s": wall,
        "qps": ok / wall if wall else 0.0,
        "workers": workers,
    }
    if times:
        out.update(summarize_ms(times))
    return out


def measure_server(name: str, cmd: list[str], cwd: Path, host: str, port: int, proto: int) -> dict:
    log_path = BENCH_DIR / f"{name}.log"
    print(f"== {name}: starting {cmd}", flush=True)
    readies = []
    proc = None
    for i in range(5):
        if proc is not None:
            kill_tree(proc)
            time.sleep(0.15)
        proc, ready_s = start_server(cmd, cwd, host, port, log_path)
        readies.append(ready_s)
        print(f"   start {i+1}/5  {ready_s*1000:.2f} ms", flush=True)
    try:
        time.sleep(0.25)
        rss = rss_kib(proc.pid)
        seq = sequential_pings(host, port, proto, n=200, warmup=50)
        conc = concurrent_pings(host, port, proto, n=400, workers=32)
        rss_after = rss_kib(proc.pid)
        bin_path = Path(cmd[0])
        size = bin_path.stat().st_size if bin_path.exists() else None
        ready_ms = [x * 1000 for x in readies]
        print(
            f"   ready median {statistics.median(ready_ms):.2f} ms  rss {rss} KiB  "
            f"ping p50 {seq['p50_ms']:.3f} ms  conc qps {conc['qps']:.0f}",
            flush=True,
        )
        return {
            "name": name,
            "cmd": cmd,
            "binary_bytes": size,
            "ready_ms": statistics.median(ready_ms),
            "ready_ms_samples": ready_ms,
            "ready_ms_best": min(ready_ms),
            "rss_kib": rss,
            "rss_after_kib": rss_after,
            "pid": proc.pid,
            "sequential": {k: v for k, v in seq.items() if k != "body"},
            "motd": seq.get("body"),
            "concurrent": conc,
        }
    finally:
        kill_tree(proc)
        time.sleep(0.2)


def worldgen_runs(bin_path: Path, threads: list[int], repeats: int) -> dict:
    rows = []
    for t in threads:
        samples = []
        checksum = None
        for i in range(repeats):
            t0 = time.perf_counter()
            p = subprocess.run(
                [str(bin_path), "--threads", str(t)],
                cwd=str(ROOT),
                capture_output=True,
                text=True,
                timeout=120,
            )
            wall = time.perf_counter() - t0
            if p.returncode != 0:
                raise RuntimeError(f"worldgen failed threads={t}: {p.stderr}\n{p.stdout}")
            line = p.stdout.strip().splitlines()[-1] if p.stdout.strip() else ""
            if "checksum=" in line:
                checksum = line.split("checksum=")[1].split()[0]
            samples.append(wall)
            print(f"   worldgen threads={t} run={i+1}/{repeats} {wall*1000:.2f} ms  {line}", flush=True)
        rows.append(
            {
                "threads": t,
                "repeats": repeats,
                "checksum": checksum,
                "wall_ms": [x * 1000 for x in samples],
                "best_ms": min(samples) * 1000,
                "median_ms": statistics.median(samples) * 1000,
            }
        )
    return {"chunks": 4096, "grid": "64x64", "runs": rows}


def host_info() -> dict:
    cpu = "?"
    try:
        for line in Path("/proc/cpuinfo").read_text().splitlines():
            if line.startswith("model name"):
                cpu = line.split(":", 1)[1].strip()
                break
    except FileNotFoundError:
        pass
    mem = None
    try:
        for line in Path("/proc/meminfo").read_text().splitlines():
            if line.startswith("MemTotal:"):
                mem = int(line.split()[1])
                break
    except FileNotFoundError:
        pass
    gpu = subprocess.run(["nvidia-smi", "-L"], capture_output=True, text=True)
    return {
        "cpu": cpu,
        "nproc": os.cpu_count(),
        "mem_kib": mem,
        "gpu": gpu.stdout.strip() if gpu.returncode == 0 else None,
        "os": Path("/etc/os-release").read_text() if Path("/etc/os-release").exists() else sys.platform,
    }


def fetch_pumpkin(dest: Path) -> Path:
    dest.parent.mkdir(parents=True, exist_ok=True)
    url = "https://github.com/Pumpkin-MC/Pumpkin/releases/download/nightly/pumpkin-X64-Linux-musl"
    if dest.exists() and dest.stat().st_size > 1_000_000:
        print(f"== pumpkin binary already at {dest} ({dest.stat().st_size} bytes)", flush=True)
        return dest
    print(f"== downloading {url}", flush=True)
    subprocess.check_call(["curl", "-fL", "--retry", "3", "-o", str(dest), url])
    dest.chmod(0o755)
    return dest


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=25565)
    ap.add_argument("--skip-pumpkin", action="store_true")
    ap.add_argument("--skip-cinder", action="store_true")
    ap.add_argument("--skip-worldgen", action="store_true")
    args = ap.parse_args()

    BENCH_DIR.mkdir(parents=True, exist_ok=True)
    info = host_info()
    print(f"== host {info['cpu']}  nproc={info['nproc']}  gpu={info['gpu']}", flush=True)

    out = {"host": info, "servers": []}

    cinder_bin = ROOT / "cinder"
    bench_bin = ROOT / "cinder-bench"
    pumpkin_bin = BENCH_DIR / "pumpkin"
    pumpkin_cwd = BENCH_DIR / "pumpkin-run"
    pumpkin_cwd.mkdir(parents=True, exist_ok=True)

    if not args.skip_cinder:
        if not cinder_bin.exists():
            raise SystemExit(f"missing {cinder_bin}; build with scripts/build.sh")
        out["servers"].append(
            measure_server(
                "cinder",
                [str(cinder_bin), "--threads", str(os.cpu_count() or 8)],
                ROOT,
                args.host,
                args.port,
                776,
            )
        )

    if not args.skip_pumpkin:
        fetch_pumpkin(pumpkin_bin)
        # First launch generates config; do it once so the timed run is a warm fileset.
        print("== pumpkin: generating config", flush=True)
        proc, _ = start_server([str(pumpkin_bin)], pumpkin_cwd, args.host, args.port, BENCH_DIR / "pumpkin-init.log")
        kill_tree(proc)
        time.sleep(0.3)
        out["servers"].append(
            measure_server(
                "pumpkin",
                [str(pumpkin_bin)],
                pumpkin_cwd,
                args.host,
                args.port,
                776,
            )
        )

    if not args.skip_worldgen:
        if not bench_bin.exists():
            raise SystemExit(f"missing {bench_bin}")
        print("== cinder worldgen", flush=True)
        nproc = os.cpu_count() or 8
        threads = []
        t = 1
        while t <= nproc:
            threads.append(t)
            t *= 2
        if nproc not in threads:
            threads.append(nproc)
        out["cinder_worldgen"] = worldgen_runs(bench_bin, threads, repeats=5)

    RESULTS.write_text(json.dumps(out, indent=2))
    print(f"== wrote {RESULTS}", flush=True)

    # Markdown table for humans
    md = []
    md.append("# Cinder vs Pumpkin — same-machine bench")
    md.append("")
    md.append(f"- CPU: {info['cpu']}")
    md.append(f"- Cores: {info['nproc']}")
    md.append(f"- RAM: {info['mem_kib'] / 1024 / 1024:.1f} GiB" if info["mem_kib"] else "- RAM: ?")
    md.append(f"- GPU: {info['gpu'] or 'none'}")
    md.append("- Status ping: handshake + status request, wall time on the client")
    md.append("- Ready: spawn → first successful status ping")
    md.append("")
    md.append("| Server | Binary | Ready | Idle RSS | Ping p50 | Ping p99 | 200 conc. QPS |")
    md.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: |")
    for s in out["servers"]:
        size = f"{s['binary_bytes'] / 1024 / 1024:.2f} MiB" if s.get("binary_bytes") else "?"
        rss = f"{s['rss_kib'] / 1024:.1f} MiB" if s.get("rss_kib") else "?"
        seq = s["sequential"]
        md.append(
            f"| {s['name']} | {size} | {s['ready_ms']:.2f} ms (best {s.get('ready_ms_best', s['ready_ms']):.2f}) | {rss} | "
            f"{seq['p50_ms']:.3f} ms | {seq['p99_ms']:.3f} ms | {s['concurrent']['qps']:.0f} |"
        )
    md.append("")
    if "cinder_worldgen" in out:
        md.append("## Cinder heightmap worldgen (4096 chunks, 64×64 grid)")
        md.append("")
        md.append("| Threads | Best | Median | Checksum |")
        md.append("| ---: | ---: | ---: | --- |")
        for r in out["cinder_worldgen"]["runs"]:
            md.append(
                f"| {r['threads']} | {r['best_ms']:.2f} ms | {r['median_ms']:.2f} ms | {r['checksum']} |"
            )
        md.append("")
        md.append("Pumpkin has no matching isolated worldgen binary; this is Cinder-only.")
        md.append("")
    md.append("Notes:")
    md.append("")
    md.append("- Cinder is a status/login/config server plus parallel heightmaps; it is not a full vanilla sim.")
    md.append("- Pumpkin is a full (early) vanilla-compatible server, so idle RSS includes world/runtime machinery Cinder does not have yet.")
    md.append("- Startup is measured from process spawn to the first successful Minecraft status ping, not bind().")
    (BENCH_DIR / "RESULTS.md").write_text("\n".join(md) + "\n")
    print("\n".join(md), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
