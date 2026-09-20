"""Insert the two names that a previous run added to the enum but not the table."""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
gen_path = ROOT / "src" / "vanilla_gen.c"
gen = gen_path.read_text()

marker = '      "minecraft:bone_block",'
assert marker in gen
if '"minecraft:sculk_sensor"' not in gen:
    gen = gen.replace(
        marker,
        '      "minecraft:bone_block",\n      "minecraft:sculk_sensor",\n      "minecraft:chest",',
        1)
    gen_path.write_text(gen)
    print("inserted sculk_sensor, chest")
else:
    print("already present")
