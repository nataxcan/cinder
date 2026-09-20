#!/usr/bin/env bash
# 1024-chunk (32x32) Full-chunk worldgen wall clock for Cinder.
#   scripts/bench_cinder_worldgen.sh [threads...]
# Prints one line per run and the median per thread count.
set -uo pipefail
cd "$(dirname "$0")/.."

if [ ! -x ./cinder-bench ]; then
  echo "build first: bash scripts/build.sh" >&2
  exit 1
fi

THREADS=("$@")
if [ ${#THREADS[@]} -eq 0 ]; then
  THREADS=(4 8 16)
fi

for t in "${THREADS[@]}"; do
  times=()
  for run in 1 2 3; do
    start=$(date +%s.%N)
    out=$(./cinder-bench --threads "$t" 2>/dev/null | tail -1)
    end=$(date +%s.%N)
    secs=$(python3 -c "print(f'{$end-$start:.2f}')")
    times+=("$secs")
    echo "threads=$t run=$run ${secs}s  $out"
  done
  python3 - "$t" "${times[@]}" <<'PY'
import statistics, sys
t = sys.argv[1]
vals = [float(v) for v in sys.argv[2:]]
print(f"threads={t} median={statistics.median(vals):.2f}s best={min(vals):.2f}s")
PY
done
