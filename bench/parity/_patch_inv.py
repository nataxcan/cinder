import pathlib

p = pathlib.Path('bench/parity/feature_inventory.py')
s = p.read_text()
s = s.replace('def key(prefix: str, ref: str) -> str:', 'def embed_key(prefix: str, ref: str) -> str:')
s = s.replace('key("pf:", ref)', 'embed_key("pf:", ref)')
s = s.replace('key("cf:", cf_ref)', 'embed_key("cf:", cf_ref)')
s = s.replace('key("cf:", r)', 'embed_key("cf:", r)')
p.write_text(s)
print('renamed')
