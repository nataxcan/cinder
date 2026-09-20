#!/usr/bin/env bash
# Compare Cinder's density-function / noise / biome values against the vanilla
# in-process probe. Every value must match bit-for-bit (hex-float comparison).
#
#   bench/parity/df_check.sh            # all targets
#   bench/parity/df_check.sh biome      # only the biome target
set -uo pipefail
cd "$(dirname "$0")/../.."

HERE=bench/parity
WORK=/tmp/dfcheck
mkdir -p "$WORK"
python3 "$HERE/points.py" df > "$WORK/df_points.txt"
python3 "$HERE/points.py" biome > "$WORK/biome_points.txt"

if [ ! -x ${CINDER_BUILD_DIR:-$HOME/.cache/cinder-core}/cinder-probe ]; then
  echo "building the C probe first (scripts/build_core_test.sh)"
  bash scripts/build_core_test.sh >/dev/null
fi

fail=0
compare() {
  local label="$1"; shift
  local points="$1"; shift
  local mode="$1"; shift
  local name="$1"; shift
  echo "== $label"
  local extra=()
  if [ -n "$name" ]; then extra=("$mode" "$name"); else extra=("$mode"); fi
  bash "$HERE/probe.sh" --seed 1 --points "$points" --hex "${extra[@]}" 2>/dev/null | sort > "$WORK/vanilla.out"
  ${CINDER_BUILD_DIR:-$HOME/.cache/cinder-core}/cinder-probe --seed 1 --points "$points" "${extra[@]}" | sort > "$WORK/cinder.out"
  if python3 "$HERE/compare_probe.py" "$WORK/vanilla.out" "$WORK/cinder.out" "$label"; then
    :
  else
    fail=1
  fi
}

targets="${1:-all}"
if [ "$targets" = "all" ] || [ "$targets" = "df" ]; then
  compare "sloped_cheese"           "$WORK/df_points.txt" --df   minecraft:overworld/sloped_cheese
  compare "overworld/offset"        "$WORK/df_points.txt" --df   minecraft:overworld/offset
  compare "overworld/factor"        "$WORK/df_points.txt" --df   minecraft:overworld/factor
  compare "overworld/jaggedness"    "$WORK/df_points.txt" --df   minecraft:overworld/jaggedness
  compare "overworld/depth"         "$WORK/df_points.txt" --df   minecraft:overworld/depth
  compare "router final_density"    "$WORK/df_points.txt" --router final_density
  compare "router prelim_surface"   "$WORK/df_points.txt" --router preliminary_surface_level
  compare "router vein_toggle"      "$WORK/df_points.txt" --router vein_toggle
  compare "router vein_ridged"      "$WORK/df_points.txt" --router vein_ridged
  compare "router vein_gap"         "$WORK/df_points.txt" --router vein_gap
  compare "router temperature"      "$WORK/df_points.txt" --router temperature
  compare "router vegetation"       "$WORK/df_points.txt" --router vegetation
  compare "router continents"       "$WORK/df_points.txt" --router continents
  compare "router erosion"          "$WORK/df_points.txt" --router erosion
  compare "router ridges"           "$WORK/df_points.txt" --router ridges
  compare "router barrier"          "$WORK/df_points.txt" --router barrier
  compare "router floodedness"      "$WORK/df_points.txt" --router fluid_level_floodedness
  compare "router spread"           "$WORK/df_points.txt" --router fluid_level_spread
  compare "router lava"             "$WORK/df_points.txt" --router lava
  compare "noise surface"           "$WORK/df_points.txt" --noise minecraft:surface
  compare "noise cave_cheese"       "$WORK/df_points.txt" --noise minecraft:cave_cheese
fi
if [ "$targets" = "all" ] || [ "$targets" = "biome" ]; then
  compare "biomes"                  "$WORK/biome_points.txt" --biome ""
fi

exit $fail
