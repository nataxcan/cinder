#!/usr/bin/env bash
# Fast C-level checks for the worldgen translation unit.
#   scripts/check_c.sh            # syntax check every module
#   scripts/check_c.sh --standalone   # build a standalone generator (needs all parts)
set -euo pipefail
cd "$(dirname "$0")/.."

MODULES=(
  src/vanilla_gen.c
  src/vanilla_df.c
  src/vanilla_biomes.c
  src/vanilla_biome_manager.c
  src/vanilla_fill.c
  src/vanilla_rand_noise.c
  src/vanilla_surface.c
  src/vanilla_carvers.c
  src/vanilla_features.c
  src/vanilla_feature_vegetation.c
  src/vanilla_feature_terrain.c
  src/vanilla_feature_driver.c
  src/vanilla_feature_helpers.c
  src/vanilla_placements.c
)

status=0
for m in "${MODULES[@]}"; do
  if [ ! -s "$m" ]; then
    echo "skip (empty): $m"
    continue
  fi
  if ! clang -fsyntax-only -std=gnu11 -Wall -Wno-unused-function -I src "$m"; then
    status=1
  fi
  echo "ok: $m"
done

if [ "${1:-}" = "--standalone" ]; then
  clang -O2 -std=gnu11 -Wno-unused-function -I src -DCINDER_STANDALONE_MAIN -o /tmp/cinder-gen src/vanilla_gen.c -lm -lpthread
  echo "built /tmp/cinder-gen"
fi

exit $status
