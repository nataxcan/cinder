# Parallel worldgen bench. Native:
#   bend src/bench.bend -o cinder-bench && ./cinder-bench --threads 8
# GPU: this binary has no "!" call (the pipeline is embedded C), so --gpu is a
# no-op here; the GPU target is exercised by src/gpu_bench.bend instead.
import Base
import ./world.bend as W

def main() -> IO(Unit):
  do IO<Unit>:
    t0 : Nat <- IO.now()
    s  : U32 <- W.World.generate(1)
    t1 : Nat <- IO.now()
    IO.print("cinder worldgen 32x32 full checksum=" ++ U32.show(s)
      ++ " elapsed_units=" ++ Nat.show(Nat.sub(t1, t0)))
