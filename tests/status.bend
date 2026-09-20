# Drive the pure connection machine: handshake (status) + status request
# must emit a non-empty status JSON packet.
import Base
import ../src/codec.bend as C
import ../src/server.bend as S

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

def handshake_body() -> List<&2, U32>:
  C.Bytes.append(C.Bytes.append(C.Bytes.append(
    C.VarInt.encode(776), C.Mc.string("localhost")), C.Bytes.u16(25565)), [1])

def shot_ok(s: S.Shot) -> U32:
  match s:
    case S.Shot{buf, phase, name, out, close}:
      bit(nonempty(out))

def main() -> U32:
  +hs = C.Packet.frame(0, handshake_body())
  +req = C.Packet.frame(0, Nil{})
  shot_ok(S.process(C.Bytes.append(hs, req), S.Hs{}, "Player"))
