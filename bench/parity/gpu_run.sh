#!/usr/bin/env bash
# Build a Bend program for the GPU on WSL2 and run it both ways.
#
# Bend's gpu_probe requires CU_DEVICE_ATTRIBUTE_CONCURRENT_MANAGED_ACCESS, which
# WSL2 reports as 0 even though cuMemAllocManaged works there (verified with a
# standalone CUDA program). This emits the C, relaxes that one condition, builds
# it with clang 19 + the CUDA stub path, and runs CPU and GPU side by side.
#
#   bench/parity/gpu_run.sh <file.bend> [gpu-size] [args...]
#
# The patch is a workaround for WSL2, not a fix: on a bare-metal Linux or macOS
# host the unpatched binary runs on the device directly (bench/parity/gpu_probe.sh).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
export PATH="$HOME/.bend/bin:$PATH"

SRC="$1"; shift || true
SIZE="${1:-4GB}"; shift || true
LLVM="${LLVM_PREFIX:-$HOME/.cache/llvm19}"
CLANG="$LLVM/usr/lib/llvm-19/bin/clang"
OUT="${GPU_OUT:-/tmp/$(basename "${SRC%.bend}")-wsl2}"

if [ ! -x "$CLANG" ]; then
  echo "installing clang 19 (needed to build a GPU program)" >&2
  bash "$HERE/install_clang19.sh" "$LLVM" >/dev/null
fi

echo "== emit C"
( cd "$ROOT" && bend "$SRC" -o "$OUT.c" >/dev/null )

echo "== relax the WSL2 managed-memory gate"
python3 - "$OUT.c" <<'PY'
import pathlib, sys
p = pathlib.Path(sys.argv[1])
s = p.read_text()
old = """  return managed != 0
    && cuDevicePrimaryCtxRetain(&ctx, gpu_dev) == CUDA_SUCCESS
    && cuCtxSetCurrent(ctx) == CUDA_SUCCESS;"""
new = """  /* WSL2 reports CONCURRENT_MANAGED_ACCESS = 0 while cuMemAllocManaged works,
   * so the conservative gate is relaxed to the context checks. */
  (void)managed;
  return cuDevicePrimaryCtxRetain(&ctx, gpu_dev) == CUDA_SUCCESS
    && cuCtxSetCurrent(ctx) == CUDA_SUCCESS;"""
if old not in s:
    if new.split("\n")[2] in s:
        print("   already relaxed")
        sys.exit(0)
    raise SystemExit("gpu_probe not found - bend changed its runtime?")
p.write_text(s.replace(old, new, 1))
print("   patched")
PY

echo "== compile"
export LD_LIBRARY_PATH="$LLVM/usr/lib/x86_64-linux-gnu:$LLVM/usr/lib/llvm-19/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$CLANG" -DBEND_CUDA=1 -I/usr/local/cuda/include \
  -L/usr/local/cuda/lib64/stubs -L/usr/local/cuda/lib64 \
  -std=c11 -O3 "$OUT.c" -lpthread -lm -o "$OUT" -lcuda -lnvrtc

echo "== CPU (8 threads)"
( cd "$(dirname "$OUT")" && ./"$(basename "$OUT")" --threads 8 "$@" )
echo "== GPU ($SIZE)"
( cd "$(dirname "$OUT")" && ./"$(basename "$OUT")" --gpu "$SIZE" "$@" )
echo
echo "artifacts: $OUT and $(basename "$OUT").gpu"
