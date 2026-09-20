#!/usr/bin/env bash
# Time each worldgen stage set on the 32x32 grid.
#   bench/parity/timing.sh [threads]
set -uo pipefail
cd "$(dirname "$0")/../.."
T="${1:-8}"
run() {
  local label="$1" skip="$2"
  local out
  out=$(CINDER_SKIP_STAGES="$skip" ./cinder-bench --threads "$T" 2>/dev/null | tail -1)
  printf '%-34s %s\n' "$label" "$out"
}
run "terrain only"          "surface,carvers,features"
run "+ surface"             "carvers,features"
run "+ carvers"             "features"
run "+ features (full)"     ""
