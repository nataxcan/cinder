# Cost of a block write in Bend, in the simplest shape the language accepts.
#
# A chunk is 16x16x384 = 98304 blocks; Array.new takes a depth, so depth 17
# (131072 elements) is chunk-sized. This does a modest number of writes at
# consecutive indices on such an array and reports the time, which gives ns/write.
import Base

def Fill(n: Nat, a: Array<U32>, +i: U32) -> Array<U32>:
  match n:
    case 0n:
      a
    case 1n++p:
      Fill(p, a[i] <- (1 : U32), (i + 1 : U32))

def main() -> IO(Unit):
  do IO<Unit>:
    t0 : Nat <- IO.now()
    a : Array<U32> = Array.new(U32, 17n, 0)
    t1 : Nat <- IO.now()
    b : Array<U32> = Fill(1024n, a, (0 : U32))
    t2 : Nat <- IO.now()
    sz = Array.size(U32, b)
    (_, +n) = sz
    IO.print("elements=" ++ U32.show(n)
      ++ " alloc_ms=" ++ Nat.show(Nat.sub(t1, t0))
      ++ " writes_ms=" ++ Nat.show(Nat.sub(t2, t1)))
