#!/usr/bin/env bash
# Print the optional-field defaults of a RecordCodecBuilder class.
#   codec_defaults.sh net/minecraft/world/level/levelgen/feature/configurations/SpringFeatureConfiguration.class
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
cls="$1"
out=$("$HERE/../parity/javap26.sh" "$cls" 2>/dev/null)
[ -z "$out" ] && out=$(bash "$HERE/javap26.sh" "$cls" 2>/dev/null)
printf '%s\n' "$out" | awk '
  /optionalFieldOf|fieldOf/ { hold = hold $0 "\n" }
  /\/\/ String / { hold = hold $0 "\n" ; seen = 1 }
  /iconst_|-?[0-9]+: (ldc|ldc2_w|bipush|sipush|dconst|iconst_)/ { hold = hold $0 "\n" }
' > /dev/null
printf '%s\n' "$out" | grep -nE 'String [a-z_0-9]+$|// (double|float|int|long) |iconst_[0-9]|optionalFieldOf|valueOf' | sed -n '1,80p'
