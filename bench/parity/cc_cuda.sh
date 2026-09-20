#!/usr/bin/env bash
# Compiler wrapper so bend can link the GPU program on this box.
#
# Bend invokes $CC itself, and its GPU link needs -lcuda. On WSL the driver only
# ships /usr/lib/wsl/lib/libcuda.so.1 (no linkable libcuda.so), so point the
# linker at the CUDA toolkit's stub instead - the stub satisfies the link and the
# real driver is loaded at run time.
#
#   CC=$PWD/bench/parity/cc_cuda.sh bend <file.bend> -o <out>
set -euo pipefail
LLVM="${LLVM_PREFIX:-$HOME/.cache/llvm19}"
CLANG="$LLVM/usr/lib/llvm-19/bin/clang"
export LD_LIBRARY_PATH="$LLVM/usr/lib/x86_64-linux-gnu:$LLVM/usr/lib/llvm-19/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$CLANG" -L/usr/local/cuda/lib64/stubs -L/usr/local/cuda/lib64 "$@"
