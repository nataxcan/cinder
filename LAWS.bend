# Laws Cinder must not break. The AI fills PROOF.bend; humans edit this.
import Base
import ./src/codec.bend as C
import ./src/world.bend as W

# LAW: VarInt 0 is a single zero byte.
law varint_zero:
  {C.VarInt.encode(0) == [0] : List<&2, U32>}

# LAW: values under 128 fit in one byte, unchanged.
law varint_small:
  {C.VarInt.encode(127) == [127] : List<&2, U32>}

# LAW: 128 is the first two-byte VarInt.
law varint_128:
  {C.VarInt.encode(128) == [128, 1] : List<&2, U32>}

# LAW: Minecraft 1.21 protocol 767 and Java 26.2 protocol 776 encode as expected.
law varint_protocol:
  {C.VarInt.encode(767) == [255, 5] : List<&2, U32>}
  & {C.VarInt.encode(776) == [136, 6] : List<&2, U32>}

# LAW: an empty packet with id 0 is the two bytes 0x01 0x00.
law empty_packet:
  {C.Packet.frame(0, Nil{}) == [1, 0] : List<&2, U32>}

# LAW: default port 25565 is big-endian 0x63DD.
law port_bytes:
  {C.Bytes.u16(25565) == [99, 221] : List<&2, U32>}

# LAW: sampled columns sit in the walkable band [48, 79].
law height_range:
  {W.World.height_ok(W.World.height(0, 0, 1)) == True{} : Bool}
  & {W.World.height_ok(W.World.height(99, 7, 1)) == True{} : Bool}
  & {W.World.height_ok(W.World.column(255, 3, 5, 9)) == True{} : Bool}
