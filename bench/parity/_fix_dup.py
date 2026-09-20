import pathlib

p = pathlib.Path('bench/WORLD.md')
s = p.read_text()
lines = s.splitlines(keepends=True)

# The file ended up with the Reproduce and GPU sections twice: the GPU rewrite
# inserted its version ahead of the old tail. Keep the first Reproduce (233) and
# the first GPU (290), drop everything from the second Reproduce (340) onward,
# then re-append the tail that belonged to the first Reproduce block.
starts = [i for i, l in enumerate(lines) if l.startswith('## ')]
second_reproduce = starts[4]      # 0-based index of the duplicate '## Reproduce'
first_gpu = starts[3]

head = "".join(lines[:second_reproduce])
tail = "".join(lines[second_reproduce:first_gpu])   # the original tail after Reproduce

assert lines[starts[3]].startswith('## GPU'), lines[starts[3]]
assert lines[starts[4]].startswith('## Reproduce'), lines[starts[4]]

# the duplicated GPU block sits before the second Reproduce in the current file;
# drop the old GPU paragraph that followed the first Reproduce
out = head + tail
p.write_text(out)
print('sections after rewrite:')
for i, l in enumerate(out.splitlines()):
    if l.startswith('## '):
        print(f'  {i+1}: {l}')
