import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[2]
gen = (root / "src/vanilla_gen.c").read_text()
hdr = (root / "src/vanilla_common.h").read_text()

body = gen[gen.index("static const char *names[B_COUNT] = {") :]
body = body[: body.index("};")]
names = re.findall(r'"(minecraft:[a-z_0-9]+)"', body)

block = hdr[hdr.index("B_AIR = 0") : hdr.index("B_COUNT")]
ids = re.findall(r"^\s*(B_[A-Z0-9_]+),?\s*$", block, re.M)
ids = [i for i in ids if i != "B_AIR"]
ids = ["B_AIR"] + ids

print(f"enum ids: {len(ids)}  names: {len(names)}")
for i, (ident, name) in enumerate(zip(ids, names)):
    if not name.startswith("minecraft:"):
        print("mismatch", ident, name)
if len(ids) != len(names):
    print("missing names for:", ids[len(names) :])
    raise SystemExit(1)
if len(names) != len(ids):
    print("extra names:", names[len(ids) :])
    raise SystemExit(1)

# order check: id N must be the block name of enum entry N (with the known
# irregular spellings allowed)
IRREGULAR = {
    "B_GRASS": "minecraft:grass_block",
    "B_COAL": "minecraft:coal_ore",
    "B_IRON": "minecraft:iron_ore",
    "B_COPPER": "minecraft:copper_ore",
    "B_GOLD": "minecraft:gold_ore",
    "B_REDSTONE": "minecraft:redstone_ore",
    "B_LAPIS": "minecraft:lapis_ore",
    "B_DIAMOND": "minecraft:diamond_ore",
    "B_EMERALD": "minecraft:emerald_ore",
    "B_DRIPSTONE": "minecraft:dripstone_block",
    "B_MOSS": "minecraft:moss_block",
    "B_DS_COAL": "minecraft:deepslate_coal_ore",
    "B_DS_IRON": "minecraft:deepslate_iron_ore",
    "B_DS_COPPER": "minecraft:deepslate_copper_ore",
    "B_DS_GOLD": "minecraft:deepslate_gold_ore",
    "B_DS_REDSTONE": "minecraft:deepslate_redstone_ore",
    "B_DS_LAPIS": "minecraft:deepslate_lapis_ore",
    "B_DS_DIAMOND": "minecraft:deepslate_diamond_ore",
    "B_MAGMA": "minecraft:magma_block",
    "B_AMETHYST": "minecraft:amethyst_block",
    "B_INFESTED": "minecraft:infested_stone",
    "B_LILY": "minecraft:lily_pad",
    "B_DARK_LOG": "minecraft:dark_oak_log",
    "B_DARK_LEAVES": "minecraft:dark_oak_leaves",
    "B_SNOW_BLOCK": "minecraft:snow_block",
    "B_SHORT_GRASS": "minecraft:short_grass",
    "B_RAW_COPPER": "minecraft:raw_copper_block",
    "B_RAW_IRON": "minecraft:raw_iron_block",
    "B_WINDSWEPT_GRAVEL": "minecraft:windswept_gravelly_hills",
    "B_SNOWY_SLOPES": "minecraft:snowy_slopes",
}
bad = []
for i, (ident, name) in enumerate(zip(ids, names)):
    want = IRREGULAR.get(ident, "minecraft:" + ident[2:].lower())
    if want != name:
        bad.append((i, ident, want, name))
if bad:
    print("ORDER MISMATCH (id -> name table):")
    for i, ident, want, got in bad[:10]:
        print(f"  {i}: {ident} expects {want}, table has {got}")
    raise SystemExit(1)
print("order ok")
