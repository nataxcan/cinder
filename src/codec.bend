# Binary Minecraft protocol codec.
#
# A buffer is a list of bytes (U32 values 0..255). VarInts are the
# Minecraft encoding: 7 data bits per byte, high bit = continue.
import Base

# ---- VarInt --------------------------------------------------------------

def VarInt.encode.go(fuel: Nat, done: Bool, +n: U32) -> List<&2, U32>:
  match fuel done:
    case 0n True{}:
      [n]
    case 0n False{}:
      [U32.and(n, 127)]
    case 1n+p True{}:
      [n]
    case 1n+p False{}:
      +n2 = U32.shrn(n, 7n)
      ((n .&. 127) .|. 128 : U32) <> VarInt.encode.go(p, U32.is_lt(n2, 128), n2)

# at most 5 bytes, the VarInt ceiling for a U32
def VarInt.encode(+n: U32) -> List<&2, U32>:
  VarInt.encode.go(5n, U32.is_lt(n, 128), n)

# `done` is True when the previous byte ended the VarInt (high bit clear).
def VarInt.take.go(
  fuel: Nat, done: Bool, bs: List<&2, U32>, acc: U32, +shift: Nat
) -> Maybe<&1, U32 & List<&2, U32>>:
  match fuel done bs:
    case 0n True{} rest:
      Some{(acc, rest)}
    case 0n False{} Nil{}:
      None{}
    case 0n False{} h <> t:
      None{}
    case 1n+p True{} rest:
      Some{(acc, rest)}
    case 1n+p False{} Nil{}:
      None{}
    case 1n+p False{} +h <> t:
      VarInt.take.go(p, U32.is_lt(h, 128), t,
        (acc + U32.shln(U32.and(h, 127), shift) : U32), Nat.add(shift, 7n))

def VarInt.take(bs: List<&2, U32>) -> Maybe<&1, U32 & List<&2, U32>>:
  VarInt.take.go(5n, False{}, bs, 0, 0n)

# ---- Bytes ---------------------------------------------------------------

def Bytes.len(bs: List<&2, U32>) -> U32:
  U32.from_nat(List.length(&2, U32, bs))

def Bytes.u8(+n: U32) -> List<&2, U32>:
  [U32.and(n, 255)]

def Bytes.u16(+n: U32) -> List<&2, U32>:
  U32.and(U32.shrn(n, 8n), 255) <> [U32.and(n, 255)]

def Bytes.u32(+n: U32) -> List<&2, U32>:
  U32.and(U32.shrn(n, 24n), 255)
    <> (U32.and(U32.shrn(n, 16n), 255)
    <> (U32.and(U32.shrn(n, 8n), 255)
    <> [U32.and(n, 255)]))

# 8 zero bytes: a Long 0, used as a nil UUID half and as f64 0.0
def Bytes.zeros8() -> List<&2, U32>:
  [0, 0, 0, 0, 0, 0, 0, 0]

def Bytes.zeros16() -> List<&2, U32>:
  List.append(&2, U32, Bytes.zeros8(), Bytes.zeros8())

# f64 64.0 is 0x4050000000000000 — spawn Y
def Bytes.f64_64() -> List<&2, U32>:
  [64, 80, 0, 0, 0, 0, 0, 0]

def Bytes.take.go(
  n: Nat, bs: List<&2, U32>, acc: List<&2, U32>
) -> Maybe<&1, List<&2, U32> & List<&2, U32>>:
  match n:
    case 0n:
      Some{(List.reverse(&2, U32, acc), bs)}
    case 1n+p:
      match bs:
        case Nil{}:
          None{}
        case h <> t:
          Bytes.take.go(p, t, h <> acc)

def Bytes.take(n: U32, bs: List<&2, U32>) ->
  Maybe<&1, List<&2, U32> & List<&2, U32>>:
  Bytes.take.go(U32.to_nat(n), bs, Nil{})

def Bytes.append(a: List<&2, U32>, b: List<&2, U32>) -> List<&2, U32>:
  List.append(&2, U32, a, b)

def Bytes.concat3(
  a: List<&2, U32>, b: List<&2, U32>, c: List<&2, U32>
) -> List<&2, U32>:
  Bytes.append(a, Bytes.append(b, c))

# ---- Strings (ASCII / Latin-1 code units; MC uses UTF-8 on the wire) ----

def Str.bytes(s: String) -> List<&2, U32>:
  match s:
    case SNil{}:
      Nil{}
    case SCon{Chr{x}, t}:
      x <> Str.bytes(t)

def Str.from_bytes(bs: List<&2, U32>) -> String:
  match bs:
    case Nil{}:
      SNil{}
    case h <> t:
      SCon{Chr{h}, Str.from_bytes(t)}

# Minecraft string: VarInt byte-length, then the bytes
def Mc.string(s: String) -> List<&2, U32>:
  +bs = Str.bytes(s)
  Bytes.append(VarInt.encode(Bytes.len(bs)), bs)

def Mc.take_string.got(
  m: Maybe<&1, List<&2, U32> & List<&2, U32>>
) -> Maybe<&1, String & List<&2, U32>>:
  match m:
    case None{}:
      None{}
    case Some{(got, leftover)}:
      Some{(Str.from_bytes(got), leftover)}

def Mc.take_string.go(
  n: U32, rest: List<&2, U32>
) -> Maybe<&1, String & List<&2, U32>>:
  Mc.take_string.got(Bytes.take(n, rest))

def Mc.take_string.len(
  m: Maybe<&1, U32 & List<&2, U32>>
) -> Maybe<&1, String & List<&2, U32>>:
  match m:
    case None{}:
      None{}
    case Some{(n, rest)}:
      Mc.take_string.go(n, rest)

def Mc.take_string(bs: List<&2, U32>) -> Maybe<&1, String & List<&2, U32>>:
  Mc.take_string.len(VarInt.take(bs))

# ---- Packet framing ------------------------------------------------------

# length-prefixed packet: VarInt(len(id+payload)) ++ id ++ payload
def Packet.frame(+id: U32, payload: List<&2, U32>) -> List<&2, U32>:
  +body = Bytes.append(VarInt.encode(id), payload)
  Bytes.append(VarInt.encode(Bytes.len(body)), body)

# peel one packet off a stream: (packet-id-and-payload, leftover)
def Packet.peel.go(
  n: U32, rest: List<&2, U32>
) -> Maybe<&1, List<&2, U32> & List<&2, U32>>:
  Bytes.take(n, rest)

def Packet.peel.if(
  n: U32, rest: List<&2, U32>, z: Bool
) -> Maybe<&1, List<&2, U32> & List<&2, U32>>:
  match z:
    case True{}:
      None{}
    case False{}:
      Packet.peel.go(n, rest)

def Packet.peel.got(
  m: Maybe<&1, U32 & List<&2, U32>>
) -> Maybe<&1, List<&2, U32> & List<&2, U32>>:
  match m:
    case None{}:
      None{}
    case Some{(+n, rest)}:
      Packet.peel.if(n, rest, U32.is_eq(n, 0))

def Packet.peel(bs: List<&2, U32>) -> Maybe<&1, List<&2, U32> & List<&2, U32>>:
  Packet.peel.got(VarInt.take(bs))

def Packet.id(payload: List<&2, U32>) -> Maybe<&1, U32 & List<&2, U32>>:
  VarInt.take(payload)

# reject a framed length above 2 MiB (untrusted input)
def Packet.len_ok(n: U32) -> Bool:
  U32.is_le(n, 2097152)

# ---- Checksum (for tests) ------------------------------------------------

def Bytes.sum(bs: List<&2, U32>, acc: U32) -> U32:
  match bs:
    case Nil{}:
      acc
    case h <> t:
      Bytes.sum(t, (acc + h : U32))
