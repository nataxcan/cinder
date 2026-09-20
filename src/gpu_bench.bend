# The heightmap microbench on the GPU.
#
# This is the only part of Cinder that is Bend code rather than embedded C, so it
# is the only part bend can hand to a device: `World.grid_gpu` calls
# `World.grid!`, and building this file produces the companion gpu_bench.gpu
# program that the binary loads with --gpu.
#
#   CC=$PWD/bench/parity/cc_cuda.sh bend src/gpu_bench.bend -o gpu_bench
#   ./gpu_bench --threads 8
#   ./gpu_bench --gpu 4GB
#
# The device must report CU_DEVICE_ATTRIBUTE_CONCURRENT_MANAGED_ACCESS (Bend's
# heap is cuMemAllocManaged); WSL2 reports 0, so on this box the run stops with
# "found no GPU device". bench/parity/gpu_probe.sh checks all of it.
import Base
import ./world.bend as W

def main() -> IO(Unit):
  do IO<Unit>:
    t0 : Nat <- IO.now()
    s  : U32 = W.World.grid_gpu(5n, 1, 0, 0)
    t1 : Nat <- IO.now()
    IO.print("cinder heightmap microbench 32x32 checksum=" ++ U32.show(s)
      ++ " elapsed_units=" ++ Nat.show(Nat.sub(t1, t0)))
