#!/usr/bin/env bash
# CPU vs GPU timings for the heightmap microbench, with the GPU program already
# built so the one-time compile is not counted.
set -uo pipefail
cd "${GPU_BIN_DIR:-$HOME/.cache}"
BIN="${GPU_BIN:-gpu_bench_patched}"
[ -x "$BIN" ] || { echo "missing $BIN (run bench/parity/gpu_run.sh src/gpu_bench.bend first)"; exit 1; }

echo "== first run builds <binary>.gpu if it is missing"
ls -la "$BIN.gpu" 2>/dev/null | sed 's/^/   /'

for i in 1 2 3; do
  printf 'CPU  run %d: ' "$i"
  timeout 300 ./"$BIN" --threads 8 2>&1 | tail -1
done
for i in 1 2 3; do
  printf 'GPU  run %d: ' "$i"
  timeout 300 ./"$BIN" --gpu 4GB 2>&1 | tail -1
done
