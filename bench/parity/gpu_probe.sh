#!/usr/bin/env bash
# Does Cinder's Bend code run on the GPU?
#
# Bend 2.0.5 does have a GPU target: `!` (parallel call) makes `bend -o <out>`
# emit a companion `<out>.gpu` program, and `<out> --gpu 4GB` runs the `!` calls
# on the GPU (CUDA 12 at /usr/local/cuda on Linux). It needs clang >= 19.
#
# This script answers the question end to end: it checks the toolchain, builds
# bench/parity/gpu_smoke.bend (a fork-join numeric kernel of the same shape as
# Cinder's pure-Bend heightmap microbench, World.grid) for the GPU, inspects the
# artifacts, and runs it both ways.
#
#   bash bench/parity/gpu_probe.sh
set -uo pipefail
cd "$(dirname "$0")/../.."
export PATH="$HOME/.bend/bin:$PATH"
WORK="${GPU_WORK:-$HOME/.cache/cinder-gpu}"
mkdir -p "$WORK"

echo "== bend"
bend --version 2>/dev/null | tail -1

echo
echo "== GPU target in the language (from \`bend guide\`)"
bend guide 2>/dev/null | grep -n -E 'gpu 4GB|\.gpu|CUDA|clang 19' | head -6

echo
echo "== compiler"
CC_SCRIPT="$PWD/bench/parity/cc_cuda.sh"
LLVM="${LLVM_PREFIX:-$HOME/.cache/llvm19}"
CLANG="$LLVM/usr/lib/llvm-19/bin/clang"
if [ ! -x "$CLANG" ]; then
  echo "   no clang 19 at $CLANG - installing it locally (no root needed)"
  bash bench/parity/install_clang19.sh "$LLVM" >/dev/null 2>&1 || true
fi
if [ -x "$CLANG" ]; then
  # ask the wrapper: the bare clang needs LD_LIBRARY_PATH for libLLVM
  echo "   $("$CC_SCRIPT" --version 2>/dev/null | head -1)"
  echo "   wrapper: $CC_SCRIPT (adds the CUDA stub path so -lcuda links on WSL)"
else
  echo "   clang 19 unavailable - cannot build the GPU program"
fi

echo
echo "== CUDA device"
if command -v nvidia-smi >/dev/null; then
  nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader | sed 's/^/   /'
fi
cat > "$WORK/attr.cu" <<'EOF'
#include <cstdio>
#include <cuda_runtime.h>
int main() {
  int m = -1, u = -1;
  cudaDeviceGetAttribute(&m, cudaDevAttrConcurrentManagedAccess, 0);
  cudaDeviceGetAttribute(&u, cudaDevAttrUnifiedAddressing, 0);
  printf("   concurrentManagedAccess = %d\n   unifiedAddressing = %d\n", m, u);
  return 0;
}
EOF
if [ -x /usr/local/cuda/bin/nvcc ]; then
  ( cd "$WORK" && /usr/local/cuda/bin/nvcc -o attr attr.cu >/dev/null 2>&1 && ./attr )
else
  echo "   no nvcc; skipping the attribute query"
fi
echo "   (bend's gpu_probe requires concurrentManagedAccess != 0: its heap is cuMemAllocManaged)"

echo
echo "== build the GPU program"
CC="$CC_SCRIPT" bend bench/parity/gpu_smoke.bend -o "$WORK/gpu_smoke" 2>&1 | tail -3
ls -la "$WORK/gpu_smoke" "$WORK/gpu_smoke.gpu" 2>/dev/null | sed 's/^/   /'
if [ -x "$WORK/gpu_smoke" ]; then
  readelf -d "$WORK/gpu_smoke" 2>/dev/null | grep -i needed | sed 's/^/   /'
fi

echo
echo "== run: CPU"
( cd "$WORK" && timeout 600 ./gpu_smoke --threads 8 2>&1 | tail -2 )

echo
echo "== run: GPU"
( cd "$WORK" && timeout 600 ./gpu_smoke --gpu 4GB 2>&1 | tail -2 )
