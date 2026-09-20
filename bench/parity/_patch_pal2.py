import pathlib

p = pathlib.Path('bench/parity/extend_palette.py')
s = p.read_text()
old = '    marker = \'"minecraft:cave_air"};'
new = '    marker = \'"minecraft:muddy_mangrove_roots"};'
assert old in s
s = s.replace(old, new)
s = s.replace('"minecraft:cave_air",\\n\' + names_block', '"minecraft:muddy_mangrove_roots",\\n\' + names_block')
p.write_text(s)
print('applier anchor updated')
