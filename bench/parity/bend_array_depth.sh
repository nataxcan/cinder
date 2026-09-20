#!/usr/bin/env bash
# Confirm what Array.new's second argument means.
#
# Sizes 2 -> 4 and 8 -> 256 suggest it is a tree DEPTH (2^d elements), not a
# count, which would explain every earlier failure: asking for Array.new(U32, 98304n)
# is asking for 2^98304 elements. A chunk is 98304 blocks, so depth 17
# (131072 elements) is the chunk-sized array.
set -uo pipefail
export PATH="$HOME/.bend/bin:$PATH"
WORK="${NAT_WORK:-/tmp/nat-limit}"
mkdir -p "$WORK"

for d in 2 8 10 17 20; do
  cat > "$WORK/depth.bend" <<EOF
import Base
def Sized(pair: Array<U32> & U32) -> U32:
  (arr, +n) = pair
  n
def main() -> IO(Unit):
  do IO<Unit>:
    a : Array<U32> = Array.new(U32, ${d}n, 42)
    n : U32 = Sized(Array.size(U32, a))
    IO.print("depth=${d} size=" ++ U32.show(n))
EOF
  if timeout 600 bend "$WORK/depth.bend" -o "$WORK/depth" >/dev/null 2>"$WORK/depth.err"; then
    printf '  depth %-3s -> %s\n' "$d" "$(timeout 120 "$WORK/depth" 2>&1 | tail -1 | cut -c1-46)"
  else
    printf '  depth %-3s -> BUILD FAILED: %s\n' "$d" "$(grep -m1 message "$WORK/depth.err" | cut -c1-40)"
  fi
done
echo
echo "expected if the argument is a depth: 4, 256, 1024, 131072, 1048576"
