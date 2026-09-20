# Status packet is a framed JSON string starting with a VarInt length.
import Base
import ../src/codec.bend as C
import ../src/proto.bend as P

def bit(b: Bool) -> U32:
  match b:
    case True{}:
      1
    case False{}:
      0

def nonempty(bs: List<&2, U32>) -> Bool:
  match bs:
    case Nil{}:
      False{}
    case h <> t:
      True{}

def main() -> U32:
  +a = bit(U32.is_eq(P.Proto.version(), 776))
  +b = bit(nonempty(P.Proto.status_response()))
  +c = bit(nonempty(P.Proto.pong([1, 2, 3, 4, 5, 6, 7, 8])))
  +d = bit(nonempty(P.Proto.login_success("Steve")))
  (((a * 10 + b : U32) * 10 + c : U32) * 10 + d : U32)
