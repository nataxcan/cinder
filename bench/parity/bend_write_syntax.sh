#!/usr/bin/env bash
# Find the accepted shape for a tree walk that writes into an array.
#
# The parser rejects `case ANode{xs, ys}:` when the arm contains a typed binder,
# so this tries the variants: untyped binder, sugar write, and a helper that
# avoids the binder entirely.
set -uo pipefail
export PATH="$HOME/.bend/bin:$PATH"
WORK="${NAT_WORK:-/tmp/nat-limit}"
mkdir -p "$WORK"

try() {
  local name="$1"
  if timeout 600 bend "$WORK/$name.bend" -o "$WORK/$name" >/dev/null 2>"$WORK/$name.err"; then
    printf '  %-14s BUILD OK   run: %s\n' "$name" \
      "$(timeout 300 "$WORK/$name" 2>&1 | tail -1 | cut -c1-56)"
  else
    printf '  %-14s FAILED: %s\n' "$name" \
      "$(grep -m1 -E 'observed|message' "$WORK/$name.err" | cut -c1-56)"
  fi
}

# A: untyped binder
cat > "$WORK/wa.bend" <<'EOF'
import Base
def Walk(node: Array<U32>, root: Array<U32>, +i: U32) -> Array<U32>:
  match node:
    case ALeaf{x}:
      Array.set(U32, root, (U32.and(i, 131071) : U32), (x + 1 : U32))
    case ANode{xs, ys}:
      after = Walk(xs, root, i)
      Walk(ys, after, (i + 1 : U32))
def main() -> IO(Unit):
  do IO<Unit>:
    a : Array<U32> = Array.new(U32, 17n, 0)
    b : Array<U32> = Walk(a, a, 0)
    IO.print("wa ok")
EOF

# B: write sugar
cat > "$WORK/wb.bend" <<'EOF'
import Base
def Walk(node: Array<U32>, root: Array<U32>, +i: U32) -> Array<U32>:
  match node:
    case ALeaf{x}:
      root[(U32.and(i, 131071) : U32)] <- (x + 1 : U32)
    case ANode{xs, ys}:
      after = Walk(xs, root, i)
      Walk(ys, after, (i + 1 : U32))
def main() -> IO(Unit):
  do IO<Unit>:
    a : Array<U32> = Array.new(U32, 17n, 0)
    b : Array<U32> = Walk(a, a, 0)
    IO.print("wb ok")
EOF

# C: no intermediate binder - recurse through a helper that takes the written array
cat > "$WORK/wc.bend" <<'EOF'
import Base
def Walk(node: Array<U32>, root: Array<U32>, +i: U32) -> Array<U32>:
  match node:
    case ALeaf{x}:
      Array.set(U32, root, (U32.and(i, 131071) : U32), (x + 1 : U32))
    case ANode{xs, ys}:
      Walk(ys, Walk(xs, root, i), (i + 1 : U32))
def main() -> IO(Unit):
  do IO<Unit>:
    a : Array<U32> = Array.new(U32, 17n, 0)
    b : Array<U32> = Walk(a, a, 0)
    IO.print("wc ok")
EOF

try wa
try wb
try wc
