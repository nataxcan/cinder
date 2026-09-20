# Pure codec tests. `bend tests/codec.bend` prints a bit-packed report:
# each passing check contributes a decimal digit of 1.
import Base
import ../src/codec.bend as C

def bit(b: Bool) -> U32:
  match b:
    case True{}:
      1
    case False{}:
      0

def eq_bytes(a: List<&2, U32>, b: List<&2, U32>) -> Bool:
  match a b:
    case Nil{} Nil{}:
      True{}
    case h1 <> t1 h2 <> t2:
      Bool.and(U32.is_eq(h1, h2), eq_bytes(t1, t2))
    case Nil{} h2 <> t2:
      False{}
    case h1 <> t1 Nil{}:
      False{}

def main() -> U32:
  +z  = bit(eq_bytes(C.VarInt.encode(0), [0]))
  +a  = bit(eq_bytes(C.VarInt.encode(127), [127]))
  +b  = bit(eq_bytes(C.VarInt.encode(128), [128, 1]))
  +c  = bit(eq_bytes(C.VarInt.encode(255), [255, 1]))
  +d  = bit(eq_bytes(C.VarInt.encode(767), [255, 5]))
  +e  = bit(eq_bytes(C.VarInt.encode(776), [136, 6]))
  +f  = bit(eq_bytes(C.Bytes.u16(25565), [99, 221]))
  +g  = bit(eq_bytes(C.Packet.frame(0, Nil{}), [1, 0]))
  (((((((z * 10 + a : U32) * 10 + b : U32) * 10 + c : U32) * 10 + d : U32)
    * 10 + e : U32) * 10 + f : U32) * 10 + g : U32)
