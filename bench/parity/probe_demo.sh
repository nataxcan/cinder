#!/usr/bin/env bash
# Demo of the vanilla worldgen probe: 20 density-function points plus 20 biome samples.
#
#   bench/parity/probe_demo.sh            # uses seed 12345
#   SEED=98765 bench/parity/probe_demo.sh
#
# Exits 0 and prints a short, reproducible reference table.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
SCRATCH="${SCRATCH:-/home/nataxcan/dev/vanilla-probe}"
SEED="${SEED:-12345}"

points="$SCRATCH/demo_points.txt"
mkdir -p "$SCRATCH"
cat > "$points" <<'EOF'
# x y z -- quart coordinates for --biome, block coordinates for --df/--router/--noise
0 0 0
0 -64 0
0 -16 0
0 64 0
0 128 0
16 64 -32
64 64 64
-64 64 -64
100 70 100
-100 63 250
500 65 500
-500 62 -500
1000 0 -1000
-1234 100 4321
4096 72 4096
-4096 8 -4096
12345 60 -54321
-54321 60 12345
999 320 -999
-999 -64 999
EOF

echo "== vanilla 26.2 probe demo (seed $SEED, $(grep -cv '^#' "$points") points)"
echo
echo "-- minecraft:overworld/sloped_cheese (registry density function)"
bash "$here/probe.sh" --seed "$SEED" --points "$points" --df minecraft:overworld/sloped_cheese
echo
echo "-- noise router: final_density"
bash "$here/probe.sh" --seed "$SEED" --points "$points" --router final_density
echo
echo "-- multi-noise biome source (overworld preset) at quart coordinates"
bash "$here/probe.sh" --seed "$SEED" --points "$points" --biome
