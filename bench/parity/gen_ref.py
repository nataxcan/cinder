#!/usr/bin/env python3
"""Build the vanilla Minecraft 26.2 reference world used for Cinder worldgen parity.

The world is the ground-truth side of the parity effort: seed 1, overworld only,
chunks (x=0..47, z=0..47), every one of them at ``Status=minecraft:full`` on disk,
with **all biome features disabled** (no trees / ores / vegetation / geodes) and
**all structures disabled**.  What is left is exactly the terrain stages Cinder
implements: multi-noise biomes, noise + aquifers + ore veins, surface rules and
cave carvers.

``CHUNK_MIN``/``CHUNK_MAX`` (x) and ``CHUNK_MIN_Z``/``CHUNK_MAX_Z`` (z) bound the
generated and verified rectangle, and ``FORCELOAD_TILE`` the forceload tile edge.
``bench/parity/gen_ref_variants.py`` overrides them (plus the datapack contents) to
build the small carvers-off / canonical variant worlds from this same pipeline.

Stages (a bare run performs all three):

  setup   wipe the reference dir, write ``eula.txt`` + ``server.properties``,
          link the vanilla server jar, build the ``refpack`` datapack (a world
          preset used as ``level-type`` plus feature-less biome overrides;
          structures are switched off with ``generate-structures=false`` because
          26.2 noise generators no longer take ``structure_overrides``).
  run     boot the server, disable block-mutating mechanics (gamerules + ``/tick
          freeze``) so the saved blocks depend only on worldgen, forceload the
          48x48 chunk area, poll the region files until every chunk reports
          ``Status=minecraft:full``, then stop the server.
  verify  re-read the region files and print status / block / biome statistics.

The setup stage always deletes the previous world directory, so a re-run is a
clean rebuild.  Requires Python 3.8+ with the standard library only; the server
needs Java 25 (see ``bench/parity/ref_config.md``).
"""

from __future__ import annotations

import argparse
import collections
import datetime
import gzip
import json
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import time
import zipfile
import zlib
from pathlib import Path

# --------------------------------------------------------------------------- #
# configuration
# --------------------------------------------------------------------------- #

HOME = Path(os.path.expanduser("~"))

REF_DIR = Path(os.environ.get("CINDER_REF_DIR", str(HOME / "dev" / "ref26.2")))
SERVER_JAR = Path(
    os.environ.get("CINDER_REF_JAR", "/home/nataxcan/dev/c2me-run/versions/26.2/server-26.2.jar")
)
LIBS_DIR = Path(os.environ.get("CINDER_REF_LIBS", "/home/nataxcan/dev/c2me-run/libraries"))
JAVA = Path(os.environ.get("CINDER_REF_JAVA", str(HOME / "jdk-25" / "bin" / "java")))
# JVM args for the reference server.  A hook rather than a list of tuning flags:
# ``-Dmax.bg.threads=1`` serializes the chunk system if a run needs to be slowed
# down or made less parallel, but it does not make a features-on world reproducible
# (measured 49/64 vs 59/64 differing chunks - see ref_config.md).
JAVA_ARGS = ["-Xmx2G"]

SEED = 1
LEVEL_NAME = "world"
CHUNK_MIN = 0  # inclusive x bounds of the generated/verified rectangle
CHUNK_MAX = 47  # 0..47 x 0..47 = the 48x48 reference area
CHUNK_MIN_Z = CHUNK_MIN  # z bounds; kept separate so a driver can request a sub-rectangle
CHUNK_MAX_Z = CHUNK_MAX
FORCELOAD_TILE = 16  # chunk edge length per forceload command: 16*16 = 256 (vanilla cap)
SERVER_PORT = 25599

PACK_NAME = "refpack"
PRESET_NS = "cinder_ref"
PRESET_ID = "no_structures"

# Console responses that mean a command did not do what we asked.  Commands are
# echoed by the parser when they fail, so a successful-looking pattern match is not
# enough: any of these must abort the run instead of producing a silently wrong
# world (this is how a bad `gamerule` id was caught).
CONSOLE_ERRORS = re.compile(
    r"Incorrect argument for command|Unknown or incomplete command|"
    r"Unknown gamerule|Expected [a-z]+ but found|That position is not loaded"
)

WORLD_DIR = REF_DIR / LEVEL_NAME
DATAPACK_DIR = WORLD_DIR / "datapacks" / PACK_NAME
DIMENSION = os.environ.get("CINDER_REF_DIMENSION", "minecraft/overworld")
LOG_PATH = REF_DIR / "server.log"
VERIFY_JSON = REF_DIR / "ref_verify.json"

AIR_BLOCKS = {"minecraft:air", "minecraft:cave_air", "minecraft:void_air"}

# Blocks a terrain-only world may legitimately contain: noise terrain, the noise
# stage ore veins (OreVeinifier fills copper veins with granite and iron veins
# with tuff), the overworld surface_rule block list, cave carvers and aquifers.
EXPECTED_BLOCKS = {
    *AIR_BLOCKS,
    # noise terrain
    "minecraft:stone",
    "minecraft:deepslate",
    "minecraft:tuff",
    "minecraft:granite",
    "minecraft:diorite",
    "minecraft:andesite",
    # ore veins (noise stage, not features)
    "minecraft:copper_ore",
    "minecraft:deepslate_copper_ore",
    "minecraft:raw_copper_block",
    "minecraft:iron_ore",
    "minecraft:deepslate_iron_ore",
    "minecraft:raw_iron_block",
    # surface rules
    "minecraft:bedrock",
    "minecraft:dirt",
    "minecraft:coarse_dirt",
    "minecraft:rooted_dirt",
    "minecraft:grass_block",
    "minecraft:podzol",
    "minecraft:mycelium",
    "minecraft:mud",
    "minecraft:gravel",
    "minecraft:clay",
    "minecraft:sand",
    "minecraft:red_sand",
    "minecraft:sandstone",
    "minecraft:red_sandstone",
    "minecraft:water",
    "minecraft:lava",
    "minecraft:snow_block",
    "minecraft:ice",
    "minecraft:blue_ice",
    "minecraft:packed_ice",
    "minecraft:powder_snow",
    "minecraft:calcite",
    "minecraft:terracotta",
    "minecraft:orange_terracotta",
    "minecraft:white_terracotta",
    "minecraft:cinnabar",
    "minecraft:sulfur",
    # carver leftovers / benign extras
    "minecraft:dripstone_block",
    "minecraft:moss_block",
    "minecraft:smooth_basalt",
    "minecraft:amethyst_block",
}

# Substrings that must never show up: any hit means features or structures ran.
FEATURE_MARKERS = (
    "leaves",
    "planks",
    "_log",
    "_wood",
    "sapling",
    "spawner",
    "chest",
    "amethyst_cluster",
    "budding_amethyst",
    "pointed_dripstone",
    "moss_carpet",
    "glow_lichen",
    "sculk",
    "kelp",
    "seagrass",
    "coral",
    "cactus",
    "bamboo",
    "sugar_cane",
    "vine",
    "mushroom",
    "flower",
    "tulip",
    "daisy",
    "blossom",
    "petals",
    "geode",
    "chorus",
    "cobblestone",
    "bricks",
    "obsidian",
    "glazed_terracotta",
    "sea_lantern",
    "prismarine",
)
# ore blocks that are *not* part of the noise-stage copper/iron veins
VEIN_ORES = {
    "minecraft:copper_ore",
    "minecraft:deepslate_copper_ore",
    "minecraft:iron_ore",
    "minecraft:deepslate_iron_ore",
    "minecraft:raw_copper_block",
    "minecraft:raw_iron_block",
}

# Not worldgen and not a feature: water touching a lava source converts it to
# obsidian through the fluid interaction (`LiquidBlock` is the only class in the
# jar that can place it; no worldgen class references Blocks.OBSIDIAN).  It sits
# exactly on lava-aquifer/water contacts and the count is stable while chunks
# stay loaded, so it is applied once when a chunk is finalized.
FLUID_ARTIFACTS = {"minecraft:obsidian"}


def log(msg: str) -> None:
    print(f"[{datetime.datetime.now().strftime('%H:%M:%S')}] {msg}", flush=True)


def classpath() -> str:
    """server-26.2.jar is a plain (non-bundler) jar with no Class-Path manifest,
    so every library from the Fabric run dir has to be passed explicitly."""
    jars = sorted(str(path) for path in LIBS_DIR.rglob("*.jar"))
    if not jars:
        raise SystemExit(f"no library jars found under {LIBS_DIR}")
    return os.pathsep.join([*jars, str(SERVER_JAR)])


def port_in_use(port: int) -> bool:
    with socket.socket() as sock:
        sock.settimeout(0.5)
        return sock.connect_ex(("127.0.0.1", port)) == 0


def ensure_free_port() -> int:
    """The server binds ``server-port``; move it if some other process holds it."""
    path = REF_DIR / "server.properties"
    current = SERVER_PORT
    for line in path.read_text().splitlines():
        if line.startswith("server-port="):
            current = int(line.split("=", 1)[1])
    if not port_in_use(current):
        return current
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        replacement = sock.getsockname()[1]
    log(f"run: port {current} is in use, switching to {replacement}")
    path.write_text(
        "\n".join(
            f"server-port={replacement}" if line.startswith("server-port=") else line
            for line in path.read_text().splitlines()
        )
        + "\n"
    )
    return replacement


# --------------------------------------------------------------------------- #
# tiny NBT reader (chunk payloads are zlib-compressed NBT compounds)
# --------------------------------------------------------------------------- #


def _nbt_payload(data: bytes, pos: int, tag: int):
    if tag == 0:
        return None, pos
    if tag == 1:
        return struct.unpack_from(">b", data, pos)[0], pos + 1
    if tag == 2:
        return struct.unpack_from(">h", data, pos)[0], pos + 2
    if tag == 3:
        return struct.unpack_from(">i", data, pos)[0], pos + 4
    if tag == 4:
        return struct.unpack_from(">q", data, pos)[0], pos + 8
    if tag == 5:
        return struct.unpack_from(">f", data, pos)[0], pos + 4
    if tag == 6:
        return struct.unpack_from(">d", data, pos)[0], pos + 8
    if tag == 7:  # byte array
        n = struct.unpack_from(">i", data, pos)[0]
        pos += 4
        return data[pos : pos + n], pos + n
    if tag == 8:  # string
        n = struct.unpack_from(">H", data, pos)[0]
        pos += 2
        return data[pos : pos + n].decode("utf-8", "replace"), pos + n
    if tag == 9:  # list
        elem, n = struct.unpack_from(">bi", data, pos)
        pos += 5
        out = []
        for _ in range(n):
            val, pos = _nbt_payload(data, pos, elem)
            out.append(val)
        return out, pos
    if tag == 10:  # compound
        out = {}
        while True:
            sub = data[pos]
            pos += 1
            if sub == 0:
                return out, pos
            n = struct.unpack_from(">H", data, pos)[0]
            pos += 2
            name = data[pos : pos + n].decode("utf-8", "replace")
            pos += n
            out[name], pos = _nbt_payload(data, pos, sub)
    if tag == 11:  # int array
        n = struct.unpack_from(">i", data, pos)[0]
        pos += 4
        return list(struct.unpack_from(">%di" % n, data, pos)), pos + 4 * n
    if tag == 12:  # long array
        n = struct.unpack_from(">i", data, pos)[0]
        pos += 4
        return list(struct.unpack_from(">%dq" % n, data, pos)), pos + 8 * n
    raise ValueError(f"unknown NBT tag {tag} at {pos}")


def nbt_read(data: bytes) -> dict:
    """Parse a root compound; returns the compound's mapping."""
    if not data or data[0] != 10:
        raise ValueError("not a root compound")
    pos = 1
    n = struct.unpack_from(">H", data, pos)[0]
    pos += 2 + n  # root name, unused
    root, _ = _nbt_payload(data, pos, 10)
    return root


def region_chunks(path: Path):
    """Yield ``(cx, cz, root_nbt_dict)`` for every populated slot of a region file."""
    with open(path, "rb") as fh:
        header = fh.read(4096)
        if len(header) < 4096:
            return
        for i in range(1024):
            entry = header[i * 4 : i * 4 + 4]
            offset = int.from_bytes(entry[:3], "big")
            count = entry[3]
            if offset == 0 or count == 0:
                continue
            fh.seek(offset * 4096)
            length = struct.unpack(">i", fh.read(4))[0]
            compression = fh.read(1)[0]
            payload = fh.read(length - 1)
            if compression == 1:
                raw = gzip.decompress(payload)
            elif compression == 2:
                raw = zlib.decompress(payload)
            elif compression == 3:
                raw = payload
            else:
                raise ValueError(f"{path.name}: unsupported compression {compression}")
            cx = int(path.name.split(".")[1]) * 32 + i % 32
            cz = int(path.name.split(".")[2]) * 32 + i // 32
            yield cx, cz, nbt_read(raw)


def overworld_region_dir() -> Path:
    """Where overworld region files live.

    26.2 stores per-dimension data under ``world/dimensions/<dimension>/region``
    (``world/region`` was the pre-1.21.9 layout).  ``CINDER_REF_DIMENSION`` can
    point this at another dimension path.
    """
    return WORLD_DIR / "dimensions" / DIMENSION / "region"


def ensure_region_symlink() -> Path | None:
    """Expose the real region directory at the classic ``world/region`` path.

    Purely a convenience for tooling that still expects ``world/region``; it is
    created only while the server is stopped so world loading never sees it.
    """
    real = overworld_region_dir()
    link = WORLD_DIR / "region"
    if link.is_symlink():
        if Path(os.readlink(link)) == real:
            return link
        link.unlink()
    elif link.exists():
        return None  # never clobber a genuine directory
    if not real.is_dir():
        return None
    try:
        link.symlink_to(real)
    except OSError as exc:  # pragma: no cover - platform dependent
        log(f"warning: could not create {link}: {exc}")
        return None
    log(f"{link} -> {real}")
    return link


def region_paths() -> list:
    region_dir = overworld_region_dir()
    paths = []
    for rx in range(CHUNK_MIN // 32, CHUNK_MAX // 32 + 1):
        for rz in range(CHUNK_MIN_Z // 32, CHUNK_MAX_Z // 32 + 1):
            paths.append(region_dir / f"r.{rx}.{rz}.mca")
    return paths


def count_occupied_slots() -> int:
    """Cheap progress probe: number of non-empty slots in the region files."""
    total = 0
    for path in region_paths():
        if not path.exists():
            continue
        with open(path, "rb") as fh:
            header = fh.read(4096)
        for i in range(1024):
            entry = header[i * 4 : i * 4 + 4]
            if entry[:3] != b"\x00\x00\x00" and entry[3] != 0:
                total += 1
    return total


def chunk_statuses() -> dict:
    """Map (cx, cz) -> Status for every chunk in the requested area."""
    statuses = {}
    for path in region_paths():
        if not path.exists():
            continue
        for cx, cz, root in region_chunks(path):
            if CHUNK_MIN <= cx <= CHUNK_MAX and CHUNK_MIN_Z <= cz <= CHUNK_MAX_Z:
                statuses[(cx, cz)] = root.get("Status", "<none>")
    return statuses


# --------------------------------------------------------------------------- #
# setup
# --------------------------------------------------------------------------- #


def server_properties() -> dict:
    # ``generate-structures=false`` is the real switch: in 26.2 a noise generator
    # no longer carries ``structure_overrides``; ChunkStatusTasks.generateStructureStarts
    # consults WorldOptions.generateStructures before creating any structure start.
    return {
        "allow-nether": "false",
        "difficulty": "peaceful",
        "enable-jmx-monitoring": "false",
        "enforce-secure-profile": "false",
        "generate-structures": "false",
        "level-name": LEVEL_NAME,
        "level-seed": str(SEED),
        "level-type": f"{PRESET_NS}:{PRESET_ID}",
        "max-players": "1",
        "max-tick-time": "-1",
        "motd": "Cinder reference world: seed 1, no features, no structures",
        "online-mode": "false",
        "pause-when-empty-seconds": "0",
        "region-file-compression": "deflate",
        "server-port": str(SERVER_PORT),
        "simulation-distance": "4",
        "spawn-protection": "0",
        "sync-chunk-writes": "false",
        "view-distance": "4",
    }


def build_datapack() -> dict:
    """Write ``world/datapacks/refpack``: preset without structures + empty features."""
    with zipfile.ZipFile(SERVER_JAR) as jar:
        version = json.loads(jar.read("version.json"))
        major = version["pack_version"]["data_major"]
        minor = version["pack_version"]["data_minor"]
        preset = json.loads(jar.read("data/minecraft/worldgen/world_preset/normal.json"))
        biomes = {
            name: jar.read(name)
            for name in jar.namelist()
            if name.startswith("data/minecraft/worldgen/biome/") and name.endswith(".json")
        }

    # 26.2 dropped ``structure_overrides`` from noise generators (only flat
    # generators still have it); drop it if a future jar adds it back.
    for dim in preset.get("dimensions", {}).values():
        dim.get("generator", {}).pop("structure_overrides", None)

    preset_dir = DATAPACK_DIR / "data" / PRESET_NS / "worldgen" / "world_preset"
    biome_dir = DATAPACK_DIR / "data" / "minecraft" / "worldgen" / "biome"
    preset_dir.mkdir(parents=True, exist_ok=True)
    biome_dir.mkdir(parents=True, exist_ok=True)

    (preset_dir / f"{PRESET_ID}.json").write_text(json.dumps(preset, indent=2) + "\n")
    (DATAPACK_DIR / "pack.mcmeta").write_text(
        json.dumps(
            {
                "pack": {
                    "description": "Cinder reference: vanilla 26.2 terrain without features/structures",
                    "min_format": [major, 0],
                    "max_format": [major, minor],
                }
            },
            indent=2,
        )
        + "\n"
    )

    steps = set()
    for name, raw in sorted(biomes.items()):
        biome = json.loads(raw)
        features = biome.get("features")
        if features is not None:
            steps.add(len(features))
            biome["features"] = [[] for _ in features]  # same step count, every step empty
        (biome_dir / Path(name).name).write_text(json.dumps(biome, indent=2) + "\n")

    return {
        "pack_format": [major, minor],
        "biomes_overridden": len(biomes),
        "feature_steps": sorted(steps),
        "preset": f"{PRESET_NS}:{PRESET_ID}",
    }


def stage_setup() -> dict:
    if WORLD_DIR.exists():
        log(f"setup: removing previous world {WORLD_DIR}")
        shutil.rmtree(WORLD_DIR)
    for stale in (LOG_PATH, VERIFY_JSON):
        stale.unlink(missing_ok=True)
    REF_DIR.mkdir(parents=True, exist_ok=True)
    WORLD_DIR.mkdir(parents=True)

    (REF_DIR / "eula.txt").write_text(
        "# Accepted by bench/parity/gen_ref.py\neula=true\n"
    )
    props = server_properties()
    (REF_DIR / "server.properties").write_text(
        "".join(f"{k}={v}\n" for k, v in sorted(props.items()))
    )

    jar_link = REF_DIR / "server.jar"
    jar_link.unlink(missing_ok=True)
    try:
        jar_link.symlink_to(SERVER_JAR)
    except OSError:
        shutil.copyfile(SERVER_JAR, jar_link)

    info = build_datapack()
    log(
        f"setup: {info['biomes_overridden']} biome overrides "
        f"(feature step counts {info['feature_steps']}), preset {info['preset']}, "
        f"pack format {info['pack_format']}"
    )
    return info


# --------------------------------------------------------------------------- #
# run
# --------------------------------------------------------------------------- #


class Server:
    """The reference server plus its console/stdout pipe."""

    def __init__(self, cp: str) -> None:
        self.log_file = open(LOG_PATH, "wb")
        self.proc = subprocess.Popen(
            [str(JAVA), *JAVA_ARGS, "-cp", cp, "net.minecraft.server.Main", "nogui"],
            cwd=REF_DIR,
            stdin=subprocess.PIPE,
            stdout=self.log_file,
            stderr=subprocess.STDOUT,
        )
        self.pos = 0
        self.echoed = 0
        self.error_pos = 0

    # -- console ---------------------------------------------------------- #
    def send(self, command: str) -> None:
        log(f"console> {command}")
        self.proc.stdin.write((command + "\n").encode())
        self.proc.stdin.flush()

    def read_log(self) -> str:
        return LOG_PATH.read_text(errors="replace") if LOG_PATH.exists() else ""

    def echo_new(self) -> None:
        data = self.read_log()
        if len(data) > self.echoed:
            for line in data[self.echoed :].splitlines():
                if line.strip():
                    print(f"    server| {line}", flush=True)
            self.echoed = len(data)

    def wait_for(self, pattern: str, timeout: float = 600.0, what: str = "") -> "re.Match":
        regex = re.compile(pattern)
        deadline = time.time() + timeout
        while time.time() < deadline:
            data = self.read_log()
            failure = CONSOLE_ERRORS.search(data, self.error_pos)
            if failure:
                self.echo_new()
                line_start = data.rfind("\n", 0, failure.start()) + 1
                line_end = data.find("\n", failure.end())
                culprit = data[line_start : line_end if line_end > 0 else len(data)]
                raise RuntimeError(f"server rejected a command: {culprit.strip()}")
            match = regex.search(data, self.pos)
            if match:
                self.pos = match.end()
                self.error_pos = max(self.error_pos, match.end())
                self.echo_new()
                return match
            if self.proc.poll() is not None:
                self.echo_new()
                raise RuntimeError(
                    f"server exited with code {self.proc.returncode} while waiting for "
                    f"{what or pattern!r}"
                )
            time.sleep(0.5)
        self.echo_new()
        raise TimeoutError(f"timed out waiting for {what or pattern!r}")

    def stop(self, timeout: float = 240.0) -> int:
        if self.proc.poll() is None:
            self.send("stop")
            try:
                code = self.proc.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                log("run: server did not exit after 'stop', terminating")
                self.proc.terminate()
                try:
                    code = self.proc.wait(timeout=60)
                except subprocess.TimeoutExpired:
                    log("run: server ignored SIGTERM, killing")
                    self.proc.kill()
                    code = self.proc.wait(timeout=60)
        else:
            code = self.proc.returncode
        self.log_file.close()
        return code

    def save_flush(self) -> None:
        self.send("save-all flush")
        self.wait_for(r"Saved the game", timeout=300, what="'Saved the game'")


def stage_run(gen_timeout: float, settle: float = 0.0) -> dict:
    if not WORLD_DIR.exists():
        raise SystemExit(f"{WORLD_DIR} missing - run the setup stage first")
    started = time.time()
    port = ensure_free_port()
    log(f"run: booting server on port {port}")
    server = Server(classpath())
    try:
        server.wait_for(r"Done \(", timeout=600, what="server startup ('Done (')")

        # Freeze the gameplay tick phase (blocks, fluids, entities, random ticks,
        # weather, time) while the chunk system keeps loading and generating. Without
        # it, aquifers keep flowing for as long as the chunks stay loaded, so the
        # saved blocks become a function of wall-clock time rather than of worldgen
        # (observed: 161 vs 218 obsidian and ~9 k different water blocks between two
        # unfrozen runs).  `ServerLevel.tick` only runs its block/fluid/random-tick
        # phase when `tickRateManager().runsNormally()`.
        server.send("tick freeze")
        server.wait_for(r"The game is frozen", timeout=60, what="tick freeze ack")

        # Tiles of at most 256 chunks; 16x16 for the 48x48 reference area.
        tiles_x = range(CHUNK_MIN, CHUNK_MAX + 1, FORCELOAD_TILE)
        tiles_z = range(CHUNK_MIN_Z, CHUNK_MAX_Z + 1, FORCELOAD_TILE)
        for x0 in tiles_x:
            for z0 in tiles_z:
                server.send(
                    f"forceload add {x0 * 16} {z0 * 16} "
                    f"{(x0 + FORCELOAD_TILE) * 16 - 1} {(z0 + FORCELOAD_TILE) * 16 - 1}"
                )
            server.wait_for(r"to be force loaded", timeout=180, what="forceload ack")

        expected = (CHUNK_MAX - CHUNK_MIN + 1) * (CHUNK_MAX_Z - CHUNK_MIN_Z + 1)
        deadline = time.time() + gen_timeout
        full = 0
        last_report = 0.0
        while True:
            server.save_flush()
            # Occupied slot count is nearly free; it is only a progress hint, the
            # authoritative check is the Status field of every saved chunk.
            occupied = min(count_occupied_slots(), expected)
            statuses = {}
            if occupied >= expected:
                statuses = chunk_statuses()
                full = sum(1 for status in statuses.values() if status.endswith("full"))
            now = time.time()
            if full >= expected or now - last_report > 20 or now > deadline:
                log(
                    f"run: {occupied}/{expected} slots occupied, "
                    f"{full}/{expected} chunks full ({now - started:.0f}s elapsed)"
                )
                last_report = now
            if full >= expected:
                break
            if now > deadline:
                log(f"run: giving up with {expected - full} chunks not full after {gen_timeout:.0f}s")
                break
            time.sleep(10)

        if settle:
            log(f"run: holding the generated chunks for {settle:.0f}s to test drift")
            time.sleep(settle)

        server.save_flush()
        try:
            server.send("datapack list")
            server.wait_for(r"data pack\(s\)|data packs? enabled", timeout=120, what="datapack list output")
        except TimeoutError as exc:
            log(f"run: warning: {exc}")
        code = server.stop()
        ensure_region_symlink()
    finally:
        if server.proc.poll() is None:
            server.stop()
    elapsed = time.time() - started
    log(f"run: server exited with code {code} after {elapsed:.0f}s")
    return {
        "full_chunks": full,
        "seconds": round(elapsed, 1),
        "exit_code": code,
        "tick_freeze": True,
        "settle_seconds": settle,
        "region_dir": str(overworld_region_dir()),
        "world_size": sum(p.stat().st_size for p in overworld_region_dir().glob("*.mca")),
    }


# --------------------------------------------------------------------------- #
# verify
# --------------------------------------------------------------------------- #


def section_counts(section: dict):
    """Return ``(Counter(name -> blocks), set(y-layers with non-air))`` for a section."""
    counts = collections.Counter()
    non_air_layers = set()
    states = section.get("block_states") or {}
    palette = [entry["Name"] for entry in states.get("palette") or []]
    if not palette:
        return counts, non_air_layers
    data = states.get("data")
    if len(palette) == 1 or not data:
        name = palette[0]
        counts[name] = 4096
        if name not in AIR_BLOCKS:
            non_air_layers.update(range(16))
        return counts, non_air_layers

    bits = max(4, (len(palette) - 1).bit_length())
    total = 0
    if bits in (4, 8):
        # The bit stream is little-endian per long, so the packed longs read as
        # bytes are exactly the index stream: nibbles for 4 bits, bytes for 8.
        stream = struct.pack("<%dq" % len(data), *data)
        step = 32 * bits  # 256 indices per y-layer
        for layer in range(16):
            slice_ = stream[layer * step : (layer + 1) * step]
            histogram = collections.Counter(slice_)
            if bits == 4:
                rows = [0] * 16
                for byte, n in histogram.items():
                    rows[byte & 15] += n
                    rows[byte >> 4] += n
            else:
                rows = [0] * 256
                for byte, n in histogram.items():
                    rows[byte] = n
            for index, n in enumerate(rows):
                if n:
                    counts[palette[index]] += n
                    total += n
                    if palette[index] not in AIR_BLOCKS:
                        non_air_layers.add(layer)
    else:  # bits that do not divide 64 (palette 17..128 or >256)
        # Vanilla's SimpleBitStorage packs ``64 // bits`` values per long,
        # low-to-high, and never spans a long boundary; the leftover high bits of
        # each long are padding.  (The legacy cross-long stream layout would decode
        # garbage here and index past the palette - that is what a canonical,
        # feature-bearing world with >16-entry palettes exposed.)
        per_long = 64 // bits
        mask = (1 << bits) - 1
        index = 0
        for value in data:
            value &= 0xFFFFFFFFFFFFFFFF
            for _ in range(per_long):
                if index >= 4096:
                    break
                slot = value & mask
                value >>= bits
                if slot >= len(palette):
                    raise ValueError(
                        f"section Y={section.get('Y')}: index {slot} outside palette "
                        f"of {len(palette)} entries (bits={bits}, per_long={per_long})"
                    )
                counts[palette[slot]] += 1
                if palette[slot] not in AIR_BLOCKS:
                    non_air_layers.add(index // 256)
                index += 1
            if index >= 4096:
                break
        total = index

    if total != 4096:
        raise ValueError(f"section Y={section.get('Y')} decoded {total} of 4096 blocks")
    return counts, non_air_layers


def stage_verify() -> dict:
    started = time.time()
    ensure_region_symlink()
    statuses = chunk_statuses()
    expected = (CHUNK_MAX - CHUNK_MIN + 1) * (CHUNK_MAX_Z - CHUNK_MIN_Z + 1)
    status_histogram = collections.Counter(statuses.values())

    blocks = collections.Counter()
    biomes = set()
    non_air_y: list = []
    sections_seen = 0
    chunks_scanned = 0
    for path in region_paths():
        if not path.exists():
            continue
        for cx, cz, root in region_chunks(path):
            if not (CHUNK_MIN <= cx <= CHUNK_MAX and CHUNK_MIN_Z <= cz <= CHUNK_MAX_Z):
                continue
            chunk_min, chunk_max = None, None
            for section in root.get("sections") or []:
                sections_seen += 1
                for biome in (section.get("biomes") or {}).get("palette") or []:
                    biomes.add(biome)
                counts, layers = section_counts(section)
                blocks.update(counts)
                if layers:
                    base = section["Y"] * 16
                    low, high = base + min(layers), base + max(layers)
                    chunk_min = low if chunk_min is None else min(chunk_min, low)
                    chunk_max = high if chunk_max is None else max(chunk_max, high)
            if chunk_min is not None:
                non_air_y.append((chunk_min, chunk_max))
            chunks_scanned += 1

    missing = [(cx, cz) for cx in range(CHUNK_MIN, CHUNK_MAX + 1)
               for cz in range(CHUNK_MIN_Z, CHUNK_MAX_Z + 1) if (cx, cz) not in statuses]
    total_blocks = sum(blocks.values())
    artifacts = {
        name: n for name, n in blocks.items() if name in FLUID_ARTIFACTS and n
    }
    unexpected = {
        name: n
        for name, n in blocks.items()
        if name not in EXPECTED_BLOCKS and name not in FLUID_ARTIFACTS
    }
    flagged = {
        name: n
        for name, n in blocks.items()
        if name not in VEIN_ORES
        and name not in FLUID_ARTIFACTS
        and any(marker in name for marker in FEATURE_MARKERS)
    }

    summary = {
        "region_dir": str(overworld_region_dir()),
        "region_dir_alias": str(WORLD_DIR / "region")
        if (WORLD_DIR / "region").is_symlink()
        else None,
        "region_files": sorted(str(p) for p in region_paths() if p.exists()),
        "chunks_expected": expected,
        "chunks_scanned": chunks_scanned,
        "chunks_missing": [list(p) for p in missing],
        "status_histogram": dict(status_histogram),
        "sections": sections_seen,
        "blocks_total": total_blocks,
        "block_histogram": dict(blocks.most_common()),
        "min_y_non_air": min(low for low, _ in non_air_y) if non_air_y else None,
        "max_y_non_air": max(high for _, high in non_air_y) if non_air_y else None,
        "biomes": sorted(biomes),
        "unexpected_blocks": unexpected,
        "fluid_artifacts": artifacts,
        "feature_like_blocks": flagged,
        "verify_seconds": round(time.time() - started, 1),
    }
    VERIFY_JSON.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")

    # -- report ----------------------------------------------------------- #
    print()
    print("=" * 72)
    print("REFERENCE WORLD VERIFICATION")
    print("=" * 72)
    print(f"region dir          : {summary['region_dir']}")
    if summary["region_dir_alias"]:
        print(f"region dir alias    : {summary['region_dir_alias']} -> {summary['region_dir']}")
    print(f"region files        : {', '.join(Path(p).name for p in summary['region_files'])}")
    print(f"chunks              : {chunks_scanned} scanned / {expected} expected"
          f" ({len(missing)} missing)")
    print(f"status histogram    : {dict(status_histogram)}")
    print(f"sections            : {sections_seen}")
    print(f"blocks counted      : {total_blocks:,}")
    print(f"non-air Y range     : {summary['min_y_non_air']} .. {summary['max_y_non_air']}")
    print(f"distinct biomes     : {len(biomes)}")
    print(f"  {', '.join(sorted(biomes))}")
    print("-" * 72)
    print(f"block histogram ({len(blocks)} distinct names):")
    for name, count in blocks.most_common():
        print(f"  {count:>12,}  {name}")
    print("-" * 72)
    print(f"unexpected blocks   : {unexpected or 'none'}")
    print(f"fluid artifacts     : {artifacts or 'none'}  (water/lava contact, not worldgen)")
    print(f"feature-like blocks : {flagged or 'none'}")
    print(f"verify seconds      : {summary['verify_seconds']}")
    print(f"summary written to  : {VERIFY_JSON}")
    print("=" * 72)
    return summary


# --------------------------------------------------------------------------- #
# entry point
# --------------------------------------------------------------------------- #


def reexec_in_wsl() -> None:
    """On Windows, re-run this script inside WSL (where the server lives)."""
    script = os.path.abspath(__file__)
    converted = subprocess.run(
        ["wsl.exe", "wslpath", "-a", script], capture_output=True, text=True, check=True
    ).stdout.strip()
    import shlex

    inner = "exec python3 " + " ".join(shlex.quote(a) for a in [converted, *sys.argv[1:]])
    sys.exit(subprocess.call(["wsl.exe", "bash", "-lc", inner]))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--stage",
        choices=["all", "setup", "run", "verify"],
        default="all",
        help="which stage to execute (default: all)",
    )
    parser.add_argument(
        "--gen-timeout",
        type=float,
        default=3600.0,
        help="seconds to wait for every requested chunk to reach full status",
    )
    parser.add_argument(
        "--settle-seconds",
        type=float,
        default=0.0,
        help="extra seconds to hold the generated chunks before saving (drift test)",
    )
    args = parser.parse_args()

    if os.name == "nt":
        reexec_in_wsl()

    log(f"reference dir : {REF_DIR}")
    log(f"server jar    : {SERVER_JAR}")
    log(f"libraries     : {LIBS_DIR}")
    log(f"java          : {JAVA}")
    for path in (SERVER_JAR, JAVA, LIBS_DIR):
        if not path.exists():
            raise SystemExit(f"missing required path: {path}")

    result = {}
    if args.stage in ("all", "setup"):
        result["setup"] = stage_setup()
    if args.stage in ("all", "run"):
        result["run"] = stage_run(args.gen_timeout, args.settle_seconds)
    if args.stage in ("all", "verify"):
        result["verify"] = stage_verify()
    log(json.dumps({k: v for k, v in result.items() if k != "verify"}, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
