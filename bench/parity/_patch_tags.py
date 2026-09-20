import pathlib

p = pathlib.Path('bench/parity/gen_block_tags.py')
s = p.read_text()
old = "    # which tags does the worldgen data reference? Parse the embedded JSON"
new = ("    embed = (ROOT / \"src\" / \"vanilla\" / \"embed.h\").read_text(encoding=\"utf-8\", errors=\"replace\")\n"
       "    # which tags does the worldgen data reference? Parse the embedded JSON")
s = s.replace(old, new, 1)
p.write_text(s)
print('restored embed read')
