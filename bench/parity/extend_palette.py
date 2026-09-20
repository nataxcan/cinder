#!/usr/bin/env python3
"""Append blocks to the B_* enum and its name table.

    python3 bench/parity/extend_palette.py            # report only
    python3 bench/parity/extend_palette.py --apply    # rewrite the two files

Idempotent: only names whose B_* identifier is missing from the enum are added.
Ids are appended immediately before B_COUNT, and the name table is extended at
the end of its initializer (located structurally, not by matching a literal).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]

NEW_BLOCKS = [
    # vegetation / flowers / plants
    "allium", "amethyst_cluster", "azure_bluet", "blue_ice", "blue_orchid",
    "brown_mushroom", "budding_amethyst", "bush", "cactus_flower", "cave_vines",
    "cave_vines_plant", "closed_eyeblossom", "cornflower", "dandelion",
    "deepslate_emerald_ore", "firefly_bush", "hanging_roots", "infested_deepslate",
    "large_amethyst_bud", "large_fern", "leaf_litter", "lilac",
    "lily_of_the_valley", "medium_amethyst_bud", "melon", "mossy_cobblestone",
    "orange_tulip", "oxeye_daisy", "pale_moss_block", "peony", "pink_petals",
    "pink_tulip", "pointed_dripstone", "poppy", "potent_sulfur", "red_mushroom",
    "red_tulip", "rose_bush", "short_dry_grass", "small_amethyst_bud",
    "spore_blossom", "sulfur", "sulfur_spike", "sunflower", "sweet_berry_bush",
    "tall_dry_grass", "tall_grass", "cobweb", "spawner", "cobblestone",
    "moss_carpet", "hanging_moss", "glow_berries", "cave_air",
    # surface rules: terracotta colours, coarse dirt, red sandstone, cinnabar
    "white_terracotta", "orange_terracotta", "yellow_terracotta",
    "brown_terracotta", "red_terracotta", "light_gray_terracotta",
    "coarse_dirt", "red_sandstone", "cinnabar", "soul_soil", "muddy_mangrove_roots",
    # sculk / fossils / monster rooms
    "sculk", "sculk_vein", "sculk_catalyst", "sculk_shrieker", "sculk_sensor",
    "suspicious_sand", "sandstone_slab", "bone_block", "chest",
    # support blocks referenced by canSurvive rules
    "farmland",
    # vegetation / structures referenced by the tree and patch features
 # saplings: every tree's would_survive predicate names one, and an unknown
 # name makes block_id_for_name return -1, which rejects the whole placement
 "oak_sapling",
 "spruce_sapling",
 "birch_sapling",
 "jungle_sapling",
 "acacia_sapling",
 "dark_oak_sapling",
 "cherry_sapling",
 "pale_oak_sapling",
    "tall_seagrass", "kelp_plant", "sea_pickle", "bamboo", "vine", "cocoa",
    "bee_nest", "mangrove_roots", "mangrove_propagule", "azalea", "azalea_leaves",
    "flowering_azalea", "flowering_azalea_leaves", "big_dripleaf", "big_dripleaf_stem",
    "small_dripleaf", "pale_oak_log", "pale_oak_leaves", "pale_moss_carpet",
    "pale_hanging_moss", "creaking_heart", "crimson_roots", "fire", "soul_fire",
    "white_tulip", "wildflowers", "void_air",
]


def ident(name: str) -> str:
    return "B_" + re.sub(r"[^A-Za-z0-9]", "_", name).upper()


def enum_ids(header: str) -> list[str]:
    body = header[header.index("B_AIR = 0") : header.index("B_COUNT")]
    ids = re.findall(r"^\s*(B_[A-Z0-9_]+),?\s*$", body, re.M)
    return ["B_AIR"] + [i for i in ids if i != "B_AIR"]


def name_table_span(gen: str) -> tuple[int, int]:
    start = gen.index("static const char *names[B_COUNT] = {")
    end = gen.index("\n};", start)
    return start, end


def main() -> int:
    apply = "--apply" in sys.argv
    hdr_path = ROOT / "src" / "vanilla_common.h"
    gen_path = ROOT / "src" / "vanilla_gen.c"
    header = hdr_path.read_text()
    gen = gen_path.read_text()

    existing = set(enum_ids(header))
    add = [b for b in NEW_BLOCKS if ident(b) not in existing]
    print(f"adding {len(add)} block ids: {', '.join(add) if add else '(none)'}")
    if not apply or not add:
        return 0

    enum_block = "".join(f"  {ident(b)},\n" for b in add)
    header = header.replace("  B_COUNT\n};", enum_block + "  B_COUNT\n};", 1)
    hdr_path.write_text(header)

    start, end = name_table_span(gen)
    table = gen[start:end]
    names_block = "".join(f',\n      "minecraft:{b}"' for b in add)
    gen = gen[:start] + table + names_block + gen[end:]
    gen_path.write_text(gen)

    # sanity: ids and names must stay aligned
    header = hdr_path.read_text()
    gen = gen_path.read_text()
    n_ids = len(enum_ids(header))
    start, end = name_table_span(gen)
    n_names = len(re.findall(r'"(minecraft:[a-z_0-9]+)"', gen[start:end]))
    print(f"enum ids={n_ids} names={n_names}")
    if n_ids != n_names:
        print("MISALIGNED - fix before building", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
