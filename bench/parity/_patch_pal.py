import pathlib

p = pathlib.Path('bench/parity/extend_palette.py')
s = p.read_text()
marker = '    "cave_air",\n'
extra = (
    '    # terracotta colours (badlands bands), coarse dirt, red sandstone, cinnabar\n'
    '    "white_terracotta", "orange_terracotta", "yellow_terracotta",\n'
    '    "brown_terracotta", "red_terracotta", "light_gray_terracotta",\n'
    '    "coarse_dirt", "red_sandstone", "cinnabar", "soul_soil", "muddy_mangrove_roots",\n'
)
assert marker in s
if 'white_terracotta' not in s:
    s = s.replace(marker, marker + extra, 1)
    p.write_text(s)
    print('palette list extended')
else:
    print('already extended')
