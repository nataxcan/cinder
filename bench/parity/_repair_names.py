"""Repair: add names-table entries for ids already present in the enum."""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[2]
hdr = (ROOT / "src" / "vanilla_common.h").read_text()
gen_path = ROOT / "src" / "vanilla_gen.c"
gen = gen_path.read_text()

body = hdr[hdr.index("B_AIR = 0") : hdr.index("B_COUNT")]
ids = re.findall(r"^\s*(B_[A-Z0-9_]+),?\s*$", body, re.M)
ids = ["B_AIR"] + [i for i in ids if i != "B_AIR"]

table = gen[gen.index("static const char *names[B_COUNT]") :]
table = table[: table.index("};")]
names = re.findall(r'"(minecraft:[a-z_0-9]+)"', table)

missing = ids[len(names) :]
if not missing:
    print("nothing to repair")
    raise SystemExit(0)

added = ["minecraft:" + ident[2:].lower() for ident in missing]
block = "".join(f',\n      "{n}"' for n in added)
marker = '"minecraft:muddy_mangrove_roots",\n};'
assert marker in gen, "tail marker missing"
gen = gen.replace(marker, '"minecraft:muddy_mangrove_roots"' + block + ",\n};", 1)
gen_path.write_text(gen)
print(f"added {len(added)} names: {added}")
