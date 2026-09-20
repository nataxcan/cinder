# Minecraft Java 26.2 (protocol 776) packet builders and parsers.
import Base
import ./codec.bend as C

def Proto.version() -> U32:
  776

def Proto.port() -> U32:
  25565

# MOTD JSON. ASCII only, no raw quotes inside the description text.
def Proto.status_json() -> String:
  "{\"version\":{\"name\":\"Cinder 26.2\",\"protocol\":776},\"players\":{\"max\":2048,\"online\":0},\"description\":{\"text\":\"Cinder - 1024 vanilla Full chunks\"}}"

def Proto.status_response() -> List<&2, U32>:
  C.Packet.frame(0, C.Mc.string(Proto.status_json()))

# Echo a 8-byte ping payload as a pong (packet id 1).
def Proto.pong(payload: List<&2, U32>) -> List<&2, U32>:
  C.Packet.frame(1, payload)

def Proto.known_packs() -> List<&2, U32>:
  C.Packet.frame(14, C.Bytes.append([1],
    C.Bytes.concat3(C.Mc.string("minecraft"), C.Mc.string("core"), C.Mc.string("26.2"))))

def Proto.finish_config() -> List<&2, U32>:
  C.Packet.frame(3, Nil{})

def Uuid.mix(s: String, acc: U32) -> U32:
  match s:
    case SNil{}:
      acc
    case SCon{Chr{x}, t}:
      Uuid.mix(t, ((acc * 16777619) .^. x : U32))

def Uuid.go(n: Nat, +h: U32, acc: List<&2, U32>) -> List<&2, U32>:
  match n:
    case 0n:
      acc
    case 1n+p:
      +h2 = (h * 1664525 + 1013904223 : U32)
      Uuid.go(p, h2, U32.and(h2, 255) <> acc)

def Uuid.from_name(+s: String) -> List<&2, U32>:
  Uuid.go(16n, Uuid.mix(s, 2166136261), Nil{})

# Login Success (id 2): Game Profile + session UUID.
def Proto.login_success(+name: String) -> List<&2, U32>:
  +uuid = Uuid.from_name(name)
  C.Packet.frame(2, C.Bytes.concat3(uuid, C.Mc.string(name),
    C.Bytes.append([0], uuid)))

def Proto.login_disconnect(reason: String) -> List<&2, U32>:
  C.Packet.frame(0, C.Mc.string(reason))

# ---- parsers -------------------------------------------------------------

def Mc.take_u16.lo(
  hi: U32, rest: List<&2, U32>
) -> Maybe<&1, U32 & List<&2, U32>>:
  match rest:
    case Nil{}:
      None{}
    case lo <> t:
      Some{((U32.shln(hi, 8n) + lo : U32), t)}

def Mc.take_u16(bs: List<&2, U32>) -> Maybe<&1, U32 & List<&2, U32>>:
  match bs:
    case Nil{}:
      None{}
    case hi <> rest:
      Mc.take_u16.lo(hi, rest)

# Handshake fields after the packet id has been stripped.
type Hello is Data:
  Hello{proto: U32, intent: U32}

def Proto.hello.intent(
  proto: U32, m: Maybe<&1, U32 & List<&2, U32>>
) -> Maybe<&1, Hello>:
  match m:
    case None{}:
      None{}
    case Some{(intent, rest3)}:
      Some{Hello{proto, intent}}

def Proto.hello.port.got(
  proto: U32, m: Maybe<&1, U32 & List<&2, U32>>
) -> Maybe<&1, Hello>:
  match m:
    case None{}:
      None{}
    case Some{(port, rest2)}:
      Proto.hello.intent(proto, C.VarInt.take(rest2))

def Proto.hello.port(
  proto: U32, rest: List<&2, U32>
) -> Maybe<&1, Hello>:
  Proto.hello.port.got(proto, Mc.take_u16(rest))

def Proto.hello.addr.got(
  proto: U32, m: Maybe<&1, String & List<&2, U32>>
) -> Maybe<&1, Hello>:
  match m:
    case None{}:
      None{}
    case Some{(addr, rest2)}:
      Proto.hello.port(proto, rest2)

def Proto.hello.addr(
  proto: U32, rest: List<&2, U32>
) -> Maybe<&1, Hello>:
  Proto.hello.addr.got(proto, C.Mc.take_string(rest))

def Proto.hello.got(
  m: Maybe<&1, U32 & List<&2, U32>>
) -> Maybe<&1, Hello>:
  match m:
    case None{}:
      None{}
    case Some{(proto, rest)}:
      Proto.hello.addr(proto, rest)

def Proto.hello(body: List<&2, U32>) -> Maybe<&1, Hello>:
  Proto.hello.got(C.VarInt.take(body))

# Login Start: name, then 16-byte UUID we ignore.
def Proto.login_name.got(
  m: Maybe<&1, String & List<&2, U32>>
) -> Maybe<&1, String>:
  match m:
    case None{}:
      None{}
    case Some{(name, rest)}:
      Some{name}

def Proto.login_name(body: List<&2, U32>) -> Maybe<&1, String>:
  Proto.login_name.got(C.Mc.take_string(body))
