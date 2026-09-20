#!/usr/bin/env python3
"""Build the small variant reference worlds for the Cinder worldgen parity effort.

Both worlds are 8x8 chunks (x = 16..23, z = 8..15, seed 1, overworld only) and are
produced by the *same* pipeline as ``gen_ref.py`` - identical datapack machinery,
``tick freeze``, ``forceload``, ``save-all`` polling and shutdown - with only the
datapack contents and the generated rectangle overridden:

  carvers-off  ``ref26.2``'s datapack (features emptied, structures off) with every
               biome's ``"carvers"`` list emptied as well (``"carvers": []``, the
               valid 26.2 no-carvers form).  No cave carving happens, so the air
               count falls back to the pre-carver (noise/surface) level.
               Differences against this world isolate *carver logic*.
  canonical    no datapack at all: vanilla defaults, structures **and** features on.
               The gap between this and ``ref26.2`` is the whole feature stage.

Every chunk of both rectangles must reach ``Status=minecraft:full``; the block and
biome histograms are printed by the inherited verify stage.

Usage::

    python3 bench/parity/gen_ref_variants.py                     # both variants, all stages
    python3 bench/parity/gen_ref_variants.py --variant canonical --stage verify
    python3 bench/parity/gen_ref_variants.py --compare            # re-read only: status scan + air diff

On Windows the script re-executes itself inside WSL, like ``gen_ref.py``.
"""

from __future__ import annotations

import argparse
import collections
import datetime
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_ref  # noqa: E402  (path set above so the driver works from any cwd)
import regions  # noqa: E402  (the same Anvil/NBT reader the --scan CLI uses)

HOME = Path(os.path.expanduser("~"))

# The requested rectangle: 8x8 chunks, one forceload command (vanilla caps a single
# command at 256 chunks; 8x8 = 64).
CHUNK_X0, CHUNK_X1 = 16, 23
CHUNK_Z0, CHUNK_Z1 = 8, 15
FORCELOAD_TILE = 8

REF26_DIR = Path(os.environ.get("CINDER_REF_DIR", str(HOME / "dev" / "ref26.2")))
CARVERS_OFF_DIR = Path(
    os.environ.get("CINDER_CARVERS_OFF_DIR", str(HOME / "dev" / "ref-carvers-off26.2"))
)
CANONICAL_DIR = Path(
    os.environ.get("CINDER_CANONICAL_DIR", str(HOME / "dev" / "ref-canonical26.2"))
)

# The unpatched gen_ref implementations, captured before ``configure`` rebinds them.
_ORIGINAL_SERVER_PROPERTIES = gen_ref.server_properties
_ORIGINAL_BUILD_DATAPACK = gen_ref.build_datapack


def log(msg: str) -> None:
    print(f"[{datetime.datetime.now().strftime('%H:%M:%S')}] {msg}", flush=True)


# --------------------------------------------------------------------------- #
# variant-specific setup (properties + datapack)
# --------------------------------------------------------------------------- #


def carvers_off_properties() -> dict:
    """Exactly ``gen_ref``'s properties: features emptied, structures off."""
    props = _ORIGINAL_SERVER_PROPERTIES()
    props["motd"] = "Cinder reference: seed 1, no features, no structures, no carvers"
    return props


def canonical_properties() -> dict:
    """Vanilla defaults: structures on (the server default) and no datapack preset."""
    props = _ORIGINAL_SERVER_PROPERTIES()
    props["generate-structures"] = "true"
    props["level-type"] = "minecraft:normal"
    props["motd"] = "Cinder reference: seed 1, vanilla default worldgen"
    return props


def carvers_off_datapack() -> dict:
    """``gen_ref``'s datapack, then ``"carvers": []`` in every overridden biome.

    26.2's biome codec takes ``carvers`` as a carver id / list of carver ids / step
    map, so the empty *map* the ticket named is not parseable ("No key type in
    MapLike[{}]" - the server refuses to load the pack).  The empty *list* is the
    valid no-carvers form and is what vanilla itself uses for its 6 carver-less
    biomes; 55 vanilla biomes carry the cave/canyon list and 5 a single nether id.
    """
    info = _ORIGINAL_BUILD_DATAPACK()
    biome_dir = gen_ref.DATAPACK_DIR / "data" / "minecraft" / "worldgen" / "biome"
    files = sorted(biome_dir.glob("*.json"))
    emptied = 0
    already = 0
    nether_ids = 0
    for path in files:
        biome = json.loads(path.read_text())
        carvers = biome.get("carvers")
        if carvers == []:
            already += 1
            continue
        if isinstance(carvers, str):
            nether_ids += 1
        biome["carvers"] = []
        path.write_text(json.dumps(biome, indent=2) + "\n")
        emptied += 1
    info["carvers_empty_list"] = emptied
    info["carvers_already_empty"] = already
    info["carvers_single_id"] = nether_ids
    info["carvers_all_empty"] = all(
        json.loads(path.read_text()).get("carvers") == [] for path in files
    )
    return info


def canonical_datapack() -> dict:
    """No datapack: vanilla ``minecraft:normal`` generation, features and structures."""
    return {
        "pack_format": None,
        "biomes_overridden": 0,
        "feature_steps": [],
        "preset": "minecraft:normal (vanilla built-in, no datapack)",
    }


VARIANTS = {
    "carvers-off": {
        "dir": CARVERS_OFF_DIR,
        "properties": carvers_off_properties,
        "datapack": carvers_off_datapack,
    },
    "canonical": {
        "dir": CANONICAL_DIR,
        "properties": canonical_properties,
        "datapack": canonical_datapack,
    },
}


def configure(name: str) -> Path:
    """Point the shared gen_ref pipeline at one variant's world + rectangle."""
    spec = VARIANTS[name]
    base = spec["dir"]
    gen_ref.REF_DIR = base
    gen_ref.WORLD_DIR = base / gen_ref.LEVEL_NAME
    gen_ref.DATAPACK_DIR = gen_ref.WORLD_DIR / "datapacks" / gen_ref.PACK_NAME
    gen_ref.LOG_PATH = base / "server.log"
    gen_ref.VERIFY_JSON = base / "ref_verify.json"
    gen_ref.CHUNK_MIN = CHUNK_X0
    gen_ref.CHUNK_MAX = CHUNK_X1
    gen_ref.CHUNK_MIN_Z = CHUNK_Z0
    gen_ref.CHUNK_MAX_Z = CHUNK_Z1
    gen_ref.FORCELOAD_TILE = FORCELOAD_TILE
    gen_ref.server_properties = spec["properties"]
    gen_ref.build_datapack = spec["datapack"]
    return base


def variant_region_dir(name: str) -> Path:
    return VARIANTS[name]["dir"] / gen_ref.LEVEL_NAME / "dimensions" / gen_ref.DIMENSION / "region"


def commands_for(name: str) -> list:
    """The exact commands that (re)build one variant."""
    script = "bench/parity/gen_ref_variants.py"
    return [
        f"python3 {script} --variant {name} --stage setup",
        f"python3 {script} --variant {name} --stage run",
        f"python3 {script} --variant {name} --stage verify",
        f"python3 bench/parity/regions.py --scan --region-dir {variant_region_dir(name)}",
    ]


def run_variant(name: str, stage: str, gen_timeout: float) -> dict:
    base = configure(name)
    log(f"variant {name}: dir {base}, chunks x={CHUNK_X0}..{CHUNK_X1} z={CHUNK_Z0}..{CHUNK_Z1}")
    result = {}
    if stage in ("all", "setup"):
        result["setup"] = gen_ref.stage_setup()
        if name == "carvers-off":
            log(
                f"variant {name}: emptied carvers in {result['setup']['carvers_empty_list']} "
                f"biome files ({result['setup']['biomes_overridden']} overrides, "
                f"{result['setup']['carvers_already_empty']} already empty, "
                f"{result['setup']['carvers_single_id']} single-id, "
                f"all empty now: {result['setup']['carvers_all_empty']})"
            )
    if stage in ("all", "run"):
        result["run"] = gen_ref.stage_run(gen_timeout)
    if stage in ("all", "verify"):
        result["verify"] = gen_ref.stage_verify()
    result["commands"] = commands_for(name)
    result["region_dir"] = str(variant_region_dir(name))
    (base / "variant_summary.json").write_text(
        json.dumps(
            {k: v for k, v in result.items() if k != "verify"},
            indent=2,
            sort_keys=True,
        )
        + "\n"
    )
    log(f"variant {name}: summary written to {base / 'variant_summary.json'}")
    return result


# --------------------------------------------------------------------------- #
# air comparison (the evidence that carvers-off really has no carved caves)
# --------------------------------------------------------------------------- #


def area_blocks(region_dir: Path) -> tuple:
    """``(chunks, Counter(name -> blocks), air below y=0)`` over the requested rectangle.

    Air below y=0 is the cave-carving signal: the noise terrain there is solid
    stone/deepslate, so any air in the four bottom sections is carved-out cave
    volume (the sky air above the surface stays out of the count).
    """
    blocks = collections.Counter()
    underground_air = 0
    chunks = 0
    for rx in range(CHUNK_X0 // 32, CHUNK_X1 // 32 + 1):
        for rz in range(CHUNK_Z0 // 32, CHUNK_Z1 // 32 + 1):
            path = region_dir / f"r.{rx}.{rz}.mca"
            if not path.exists():
                continue
            for cx, cz, root in gen_ref.region_chunks(path):
                if not (CHUNK_X0 <= cx <= CHUNK_X1 and CHUNK_Z0 <= cz <= CHUNK_Z1):
                    continue
                chunks += 1
                for section in root.get("sections") or []:
                    counts, _ = gen_ref.section_counts(section)
                    blocks.update(counts)
                    if section.get("Y", 0) < 0:
                        underground_air += sum(
                            n for name, n in counts.items() if name in gen_ref.AIR_BLOCKS
                        )
    return chunks, blocks, underground_air


def air_only(region_dir: Path) -> dict:
    chunks, blocks, underground_air = area_blocks(region_dir)
    return {
        "chunks": chunks,
        "air": sum(n for name, n in blocks.items() if name in gen_ref.AIR_BLOCKS),
        "air_below_y0": underground_air,
        "by_name": {
            name: blocks.get(name, 0) for name in sorted(gen_ref.AIR_BLOCKS)
        },
        "blocks": blocks,
    }


def stage_status_scan() -> dict:
    """``regions.py --scan`` over each world, plus the requested rectangle alone.

    The whole-directory histogram also contains the chunks the vanilla server
    always prepares around the global world spawn (identical in both variants, and
    present in ``ref26.2`` too); only the requested rectangle is the deliverable.
    """
    worlds = [*VARIANTS, "ref26.2"]
    dirs = {name: variant_region_dir(name) for name in VARIANTS}
    dirs["ref26.2"] = REF26_DIR / gen_ref.LEVEL_NAME / "dimensions" / gen_ref.DIMENSION / "region"
    expected = (CHUNK_X1 - CHUNK_X0 + 1) * (CHUNK_Z1 - CHUNK_Z0 + 1)
    result = {}
    print()
    print("=" * 72)
    print("STATUS SCAN (regions.py reader)")
    print("=" * 72)
    for name in worlds:
        whole = collections.Counter()
        rect = collections.Counter()
        for cx, cz, status in regions.scan_dir(str(dirs[name])):
            whole[status] += 1
            if CHUNK_X0 <= cx <= CHUNK_X1 and CHUNK_Z0 <= cz <= CHUNK_Z1:
                rect[status] += 1
        full = rect.get("minecraft:full", 0)
        result[name] = {
            "region_dir": str(dirs[name]),
            "whole_dir": dict(whole),
            "rectangle": dict(rect),
            "rectangle_full": full,
            "rectangle_expected": expected,
        }
        print(f"{name}")
        print(f"  region dir        : {dirs[name]}")
        print(f"  whole dir         : {dict(whole)} (total {sum(whole.values())})")
        print(f"  rectangle x={CHUNK_X0}..{CHUNK_X1} z={CHUNK_Z0}..{CHUNK_Z1}: {dict(rect)}"
              f"  -> {full}/{expected} full")
    print("=" * 72)
    return result


def stage_air_compare() -> dict:
    ref_dir = REF26_DIR / gen_ref.LEVEL_NAME / "dimensions" / gen_ref.DIMENSION / "region"
    off_dir = variant_region_dir("carvers-off")
    ref = air_only(ref_dir)
    off = air_only(off_dir)
    delta = {name: off["blocks"].get(name, 0) - ref["blocks"].get(name, 0) for name in
             set(ref["blocks"]) | set(off["blocks"])}
    top = sorted(delta.items(), key=lambda kv: -abs(kv[1]))[:8]

    print()
    print("=" * 72)
    print("AIR COMPARISON - carvers-off vs ref26.2 (same 8x8 area)")
    print("=" * 72)
    print(f"area                : x={CHUNK_X0}..{CHUNK_X1} z={CHUNK_Z0}..{CHUNK_Z1} "
          f"({(CHUNK_X1 - CHUNK_X0 + 1) * (CHUNK_Z1 - CHUNK_Z0 + 1)} requested chunks)")
    print(f"ref26.2             : {ref_dir}")
    print(f"  chunks={ref['chunks']}  air={ref['air']:,}  air below y=0={ref['air_below_y0']:,}"
          f"  {ref['by_name']}")
    print(f"carvers-off         : {off_dir}")
    print(f"  chunks={off['chunks']}  air={off['air']:,}  air below y=0={off['air_below_y0']:,}"
          f"  {off['by_name']}")
    print(f"air delta           : {off['air'] - ref['air']:,} "
          f"({(off['air'] - ref['air']) / ref['air'] * 100:+.2f}%)")
    print(f"air below y=0 delta : {off['air_below_y0'] - ref['air_below_y0']:,} "
          f"(carvers-off {off['air_below_y0']:,} vs ref26.2 {ref['air_below_y0']:,})")
    print("largest block deltas (carvers-off - ref26.2):")
    for name, value in top:
        print(f"  {value:>15,}  {name}")
    print("=" * 72)
    return {"ref": {k: v for k, v in ref.items() if k != "blocks"},
            "carvers_off": {k: v for k, v in off.items() if k != "blocks"},
            "top_deltas": top}


# --------------------------------------------------------------------------- #
# entry point
# --------------------------------------------------------------------------- #


def reexec_in_wsl() -> None:
    """On Windows, re-run this script inside WSL (where the server lives)."""
    script = os.path.abspath(__file__)
    converted = subprocess.run(
        ["wsl.exe", "wslpath", "-a", script], capture_output=True, text=True, check=True
    ).stdout.strip()
    inner = "exec python3 " + " ".join(shlex.quote(a) for a in [converted, *sys.argv[1:]])
    sys.exit(subprocess.call(["wsl.exe", "bash", "-lc", inner]))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--variant",
        choices=[*VARIANTS, "both"],
        default="both",
        help="which variant world to build (default: both)",
    )
    parser.add_argument(
        "--stage",
        choices=["all", "setup", "run", "verify"],
        default="all",
        help="which stage to execute (default: all)",
    )
    parser.add_argument(
        "--compare",
        action="store_true",
        help="skip generation: only re-read the region files (status scan + air comparison)",
    )
    parser.add_argument("--gen-timeout", type=float, default=1800.0,
                        help="seconds to wait for all 64 chunks to reach full status")
    args = parser.parse_args()

    if os.name == "nt":
        reexec_in_wsl()

    for path in (gen_ref.SERVER_JAR, gen_ref.JAVA, gen_ref.LIBS_DIR):
        if not path.exists():
            raise SystemExit(f"missing required path: {path}")

    names = list(VARIANTS) if args.variant == "both" else [args.variant]
    if args.compare:
        log(json.dumps(stage_status_scan(), sort_keys=True, default=str))
        log(json.dumps(stage_air_compare(), sort_keys=True, default=str))
        return 0

    for name in names:
        run_variant(name, args.stage, args.gen_timeout)

    if args.stage in ("all", "verify"):
        log(json.dumps(stage_status_scan(), sort_keys=True, default=str))
        log(json.dumps(stage_air_compare(), sort_keys=True, default=str))

    print()
    print("=" * 72)
    print("COMMANDS")
    print("=" * 72)
    for name in names:
        configure(name)
        print(f"# {name} -> {VARIANTS[name]['dir']}")
        for command in commands_for(name):
            print(f"  {command}")
    print("=" * 72)
    return 0


if __name__ == "__main__":
    sys.exit(main())
