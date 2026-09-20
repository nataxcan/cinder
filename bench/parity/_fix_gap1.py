import pathlib

p = pathlib.Path('bench/WORLD.md')
s = p.read_text()

old = """1. **Structures** (stage 7) are not generated. This is now the largest single
   source of disagreement with a default vanilla world (~23 000 positions in the
   8x8 area above, all trial-chamber blocks)."""

new = """1. **Structures** are not generated, and they are 64 % of the remaining
   disagreement (89 380 of 140 519 positions in the 8x8 area above). The scope is
   narrower than "structures" suggests: `bench/parity/structures_present.py` reads
   the reference chunks' `structures` tag and the whole area contains exactly one
   structure type, `minecraft:trial_chambers`, assembled from 229 `minecraft:jigsaw`
   pieces. Porting it means the jigsaw structure type (structure-set spacing and
   salt, pool aliases, piece selection, junction placement) plus NBT template
   loading with rotation and mirroring - a bounded subsystem, but not a small one."""

assert old in s, 'gap item 1 not found'
s = s.replace(old, new, 1)
p.write_text(s)
print('gap item 1 updated with the measured scope')
