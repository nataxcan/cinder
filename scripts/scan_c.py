import pathlib
import sys

p = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "cinder.c")
t = p.read_text(errors="replace")
keys = ["unbox", "u32_", "term_u32", "NUM(", "f32_unbox", "io_node", "ctr_take"]
n = 0
for i, line in enumerate(t.splitlines(), 1):
    if any(k in line for k in keys):
        print(f"{i}:{line[:180]}")
        n += 1
        if n >= 80:
            break
print("BYTES", len(t), "HITS", n)
