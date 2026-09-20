import pathlib

p = pathlib.Path('bench/parity/feature_blocks.py')
s = p.read_text()
old = '''    embed = load_embed()
    biomes = [k for k in embed if k.startswith("biome:") and ":" in k]'''
new = '''    embed = load_embed()
    # only the biomes the overworld can actually produce (BIOME_NAMES in vanilla_biomes.h)
    bio_h = (ROOT / "src" / "vanilla_biomes.h").read_text()
    overworld = set(re.findall(r'\\[ (BIO_[A-Z_]+) \\] = "minecraft:([a-z_]+)"', bio_h))
    paths = {name for _, name in overworld}
    biomes = [k for k in embed if k.startswith("biome:") and k.split(":", 1)[1] in paths]'''
assert old in s
s = s.replace(old, new)
p.write_text(s)
print('filtered to overworld biomes')
