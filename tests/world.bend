# Height is always in [48, 79]. A single column is cheap to normalize.
import Base
import ../src/world.bend as W

def bit(b: Bool) -> U32:
  match b:
    case True{}:
      1
    case False{}:
      0

def main() -> U32:
  +a = bit(W.World.height_ok(W.World.height(0, 0, 1)))
  +b = bit(W.World.height_ok(W.World.height(99, 7, 1)))
  +c = bit(U32.is_eq(W.World.block(0, 64), 1))
  +d = bit(U32.is_eq(W.World.block(128, 64), 4))
  +e = bit(U32.is_eq(W.World.block(200, 64), 0))
  ((((a * 10 + b : U32) * 10 + c : U32) * 10 + d : U32) * 10 + e : U32)
