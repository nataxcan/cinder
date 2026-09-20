# GPU smoke test for the Bend target.
#
# `!` marks a parallel call: bend builds a companion `<out>.gpu` artifact for it
# (CUDA 12 at /usr/local/cuda on Linux, clang 19+) and the binary runs it with
# `--gpu <size>`. This program is the same shape as Cinder's pure-Bend heightmap
# microbench (World.grid): a fork-join recursion over a big uniform numeric
# workload, which is the case the guide says the GPU is for.
import Base

# A deliberately uniform, branch-light kernel: 2^k rounds of multiply-add.
def Kernel.work(+seed: U32, fuel: Nat, acc: U32) -> U32:
  match fuel:
    case 0n:
      acc
    case 1n++p:
      +x = (acc * 1664525 + seed + 1013904223 : U32)
      +y = (x .^. U32.shrn(x, 13n) : U32)
      Kernel.work(seed, p, (y * 1274126177 : U32))

# 2^d independent jobs, each doing 2^rounds of work: a wide, uniform fan-out.
def Kernel.fan(+d: Nat, +seed: U32, +rounds: Nat) -> U32:
  match d:
    case 0n:
      Kernel.work(seed, rounds, seed)
    case 1n++p:
      a b = Kernel.fan(p, seed, rounds) Kernel.fan(p, (seed + 7919 : U32), rounds)
      (a + b : U32)

def Kernel.total(+d: Nat, +seed: U32, +rounds: Nat) -> U32:
  Kernel.fan!(d, seed, rounds)

def main() -> IO(Unit):
  do IO<Unit>:
    IO.print(U32.show(Kernel.total(12n, 1, 200n)))
