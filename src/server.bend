# Connection loop. Pure `process` decides; IO only recvs, sends, and closes.
import Base
import ./codec.bend as C
import ./proto.bend as P
import ./net.bend as N
import ./world.bend as W

type Phase is Data:
  Hs{}
  St{}
  Lg{}
  Cf{}
  Pl{}

# leftover buffer, phase, name, bytes to write, close after write
type Shot is Data:
  Shot{
    buf: List<&2, U32>,
    phase: Phase,
    name: String,
    out: List<&2, U32>,
    close: Bool
  }

def recv_step(r: Result<&1, &1, U32 & String, List<&2, U32>>) ->
  Maybe<&2, List<&2, U32>>:
  match r:
    case Done{Nil{}}:
      None{}
    case Done{h <> t}:
      Some{h <> t}
    case Fail{e}:
      None{}

def send_then(
  m: Socket & Result<&1, &1, U32 & String, Unit>,
  next: Socket -> IO(Unit)
) -> IO(Unit):
  (s, r) = m
  match r:
    case Done{u}:
      next(s)
    case Fail{e}:
      Socket.close(s)

# ---- pure handler --------------------------------------------------------

def handle_intent.login(
  buf: List<&2, U32>, name: String, proto: U32, login: Bool
) -> Shot:
  match login:
    case False{}:
      Shot{buf, Hs{}, name, Nil{}, True{}}
    case True{}:
      Shot{buf, Lg{}, name, Nil{}, False{}}

def handle_intent.if(
  buf: List<&2, U32>, name: String, proto: U32, +intent: U32, status: Bool
) -> Shot:
  match status:
    case True{}:
      Shot{buf, St{}, name, Nil{}, False{}}
    case False{}:
      handle_intent.login(buf, name, proto, U32.is_eq(intent, 2))

def handle_intent(buf: List<&2, U32>, name: String, proto: U32, +intent: U32) -> Shot:
  handle_intent.if(buf, name, proto, intent, U32.is_eq(intent, 1))

def handle_hello(buf: List<&2, U32>, name: String, m: Maybe<&1, P.Hello>) -> Shot:
  match m:
    case None{}:
      Shot{buf, Hs{}, name, Nil{}, True{}}
    case Some{P.Hello{proto, intent}}:
      handle_intent(buf, name, proto, intent)

def handle_status.ping(
  buf: List<&2, U32>, name: String, body: List<&2, U32>, ping: Bool
) -> Shot:
  match ping:
    case True{}:
      Shot{buf, St{}, name, P.Proto.pong(body), True{}}
    case False{}:
      Shot{buf, St{}, name, Nil{}, False{}}

def handle_status.if(
  buf: List<&2, U32>, name: String, +id: U32, body: List<&2, U32>, req: Bool
) -> Shot:
  match req:
    case True{}:
      Shot{buf, St{}, name, P.Proto.status_response(), False{}}
    case False{}:
      handle_status.ping(buf, name, body, U32.is_eq(id, 1))

def handle_status(buf: List<&2, U32>, name: String, +id: U32, body: List<&2, U32>) -> Shot:
  handle_status.if(buf, name, id, body, U32.is_eq(id, 0))

def handle_login_name(buf: List<&2, U32>, m: Maybe<&1, String>) -> Shot:
  match m:
    case None{}:
      Shot{buf, Lg{}, "Player", Nil{}, True{}}
    case Some{+name}:
      Shot{buf, Lg{}, name, P.Proto.login_success(name), False{}}

def handle_login.ack(buf: List<&2, U32>, name: String, ack: Bool) -> Shot:
  match ack:
    case False{}:
      Shot{buf, Lg{}, name, Nil{}, False{}}
    case True{}:
      Shot{buf, Cf{}, name, P.Proto.known_packs(), False{}}

def handle_login.if(
  buf: List<&2, U32>, name: String, +id: U32, body: List<&2, U32>, start: Bool
) -> Shot:
  match start:
    case True{}:
      handle_login_name(buf, P.Proto.login_name(body))
    case False{}:
      handle_login.ack(buf, name, U32.is_eq(id, 3))

def handle_login(buf: List<&2, U32>, name: String, +id: U32, body: List<&2, U32>) -> Shot:
  handle_login.if(buf, name, id, body, U32.is_eq(id, 0))

def handle_config.done(buf: List<&2, U32>, name: String, done: Bool) -> Shot:
  match done:
    case False{}:
      Shot{buf, Cf{}, name, Nil{}, False{}}
    case True{}:
      Shot{buf, Pl{}, name, Nil{}, False{}}

def handle_config.if(buf: List<&2, U32>, name: String, +id: U32, packs: Bool) -> Shot:
  match packs:
    case True{}:
      Shot{buf, Cf{}, name, P.Proto.finish_config(), False{}}
    case False{}:
      handle_config.done(buf, name, U32.is_eq(id, 3))

def handle_config(buf: List<&2, U32>, name: String, +id: U32, body: List<&2, U32>) -> Shot:
  handle_config.if(buf, name, id, U32.is_eq(id, 7))

def handle_body(
  buf: List<&2, U32>, phase: Phase, name: String, id: U32, body: List<&2, U32>
) -> Shot:
  match phase:
    case Hs{}:
      handle_hello(buf, name, P.Proto.hello(body))
    case St{}:
      handle_status(buf, name, id, body)
    case Lg{}:
      handle_login(buf, name, id, body)
    case Cf{}:
      handle_config(buf, name, id, body)
    case Pl{}:
      Shot{buf, Pl{}, name, Nil{}, False{}}

def handle_id(
  buf: List<&2, U32>, phase: Phase, name: String,
  m: Maybe<&1, U32 & List<&2, U32>>
) -> Shot:
  match m:
    case None{}:
      Shot{buf, phase, name, Nil{}, True{}}
    case Some{(id, body)}:
      handle_body(buf, phase, name, id, body)

def handle(buf: List<&2, U32>, phase: Phase, name: String, pkt: List<&2, U32>) -> Shot:
  handle_id(buf, phase, name, C.Packet.id(pkt))

# Continuation-passing peel: helpers never name process.go.
def process.more(
  leftover: List<&2, U32>, phase: Phase, name: String, acc: List<&2, U32>,
  close: Bool,
  cont: Nat -> List<&2, U32> -> Phase -> String -> List<&2, U32> -> Shot,
  p: Nat
) -> Shot:
  match close:
    case True{}:
      Shot{leftover, phase, name, acc, True{}}
    case False{}:
      cont(p, leftover, phase, name, acc)

def process.hit(
  leftover: List<&2, U32>, acc: List<&2, U32>, s: Shot,
  cont: Nat -> List<&2, U32> -> Phase -> String -> List<&2, U32> -> Shot,
  p: Nat
) -> Shot:
  match s:
    case Shot{buf, phase, name, out, close}:
      process.more(leftover, phase, name, C.Bytes.append(acc, out), close, cont, p)

def process.step(
  buf: List<&2, U32>, phase: Phase, name: String, acc: List<&2, U32>,
  m: Maybe<&1, List<&2, U32> & List<&2, U32>>,
  cont: Nat -> List<&2, U32> -> Phase -> String -> List<&2, U32> -> Shot,
  p: Nat
) -> Shot:
  match m:
    case None{}:
      Shot{buf, phase, name, acc, False{}}
    case Some{(pkt, +leftover)}:
      process.hit(leftover, acc, handle(leftover, phase, name, pkt), cont, p)

@unsafe
def process.go(
  fuel: Nat, +buf: List<&2, U32>, +phase: Phase, +name: String, +acc: List<&2, U32>
) -> Shot:
  match fuel:
    case 0n:
      Shot{buf, phase, name, acc, False{}}
    case 1n+p:
      process.step(buf, phase, name, acc, C.Packet.peel(buf),
        f => b => ph => nm => a => process.go(f, b, ph, nm, a), p)

def process(buf: List<&2, U32>, phase: Phase, name: String) -> Shot:
  process.go(64n, buf, phase, name, Nil{})

# ---- IO ------------------------------------------------------------------

def fire.close(
  sock: Socket, out: List<&2, U32>
) -> IO(Unit):
  match out:
    case Nil{}:
      Socket.close(sock)
    case h <> t:
      do IO<Unit>:
        sm : Socket & Result<&1, &1, U32 & String, Unit> <-
          N.Bytes.send(sock, h <> t)
        send_then(sm, s2 => Socket.close(s2))

def fire.cont(
  sock: Socket, buf: List<&2, U32>, phase: Phase, name: String, out: List<&2, U32>,
  next: Socket -> List<&2, U32> -> Phase -> String -> IO(Unit)
) -> IO(Unit):
  match out:
    case Nil{}:
      next(sock, buf, phase, name)
    case h <> t:
      do IO<Unit>:
        sm : Socket & Result<&1, &1, U32 & String, Unit> <-
          N.Bytes.send(sock, h <> t)
        send_then(sm, s2 => next(s2, buf, phase, name))

def fire.if(
  sock: Socket, buf: List<&2, U32>, phase: Phase, name: String,
  out: List<&2, U32>, close: Bool,
  next: Socket -> List<&2, U32> -> Phase -> String -> IO(Unit)
) -> IO(Unit):
  match close:
    case True{}:
      fire.close(sock, out)
    case False{}:
      fire.cont(sock, buf, phase, name, out, next)

def fire(
  sock: Socket, s: Shot,
  next: Socket -> List<&2, U32> -> Phase -> String -> IO(Unit)
) -> IO(Unit):
  match s:
    case Shot{buf, phase, name, out, close}:
      fire.if(sock, buf, phase, name, out, close, next)

def after_recv.got(
  sock: Socket, buf: List<&2, U32>, phase: Phase, name: String,
  m: Maybe<&2, List<&2, U32>>,
  next: Socket -> List<&2, U32> -> Phase -> String -> IO(Unit)
) -> IO(Unit):
  match m:
    case None{}:
      Socket.close(sock)
    case Some{fresh}:
      fire(sock, process(C.Bytes.append(buf, fresh), phase, name), next)

def after_recv(
  m: Socket & Result<&1, &1, U32 & String, List<&2, U32>>,
  buf: List<&2, U32>, phase: Phase, name: String,
  next: Socket -> List<&2, U32> -> Phase -> String -> IO(Unit)
) -> IO(Unit):
  (s, r) = m
  after_recv.got(s, buf, phase, name, recv_step(r), next)

@unsafe
def conn_loop(
  sock: Socket, buf: List<&2, U32>, phase: Phase, name: String
) -> IO(Unit):
  do IO<Unit>:
    rm : Socket & Result<&1, &1, U32 & String, List<&2, U32>> <-
      N.Bytes.recv(sock, 4096)
    after_recv(rm, buf, phase, name, s2 => b2 => p2 => n2 => conn_loop(s2, b2, p2, n2))

def serve_next(
  m: Listener & Result<&1, &1, U32 & String, Socket>
) -> IO(Listener):
  (l, r) = m
  do IO<Listener>:
    s : Socket <- IO.pass(Socket, r)
    IO.spawn(Unit, conn_loop(s, Nil{}, Hs{}, "Player"))
    return l

@unsafe
def serve(l: Listener) -> IO(Unit):
  do IO<Unit>:
    m  : Listener & Result<&1, &1, U32 & String, Socket> <- TCP.accept(l)
    l2 : Listener <- serve_next(m)
    serve(l2)

def run() -> IO(Unit):
  do IO<Unit>:
    t0 : Nat <- IO.now()
    fp : U32 <- W.World.generate(1)
    t1 : Nat <- IO.now()
    IO.print("Cinder world ready: chunks=1024 checksum=" ++ U32.show(fp)
      ++ " elapsed_units=" ++ Nat.show(Nat.sub(t1, t0)))
    l : Listener <- IO.try(Listener, TCP.listen(25565))
    IO.print("Cinder listening on 0.0.0.0:25565 (Minecraft 26.2 / protocol 776)")
    serve(l)
