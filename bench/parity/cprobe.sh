#!/usr/bin/env bash
# Build and run a Cinder-side probe against the scratch translation unit.
#
#   cprobe.sh bench/parity/rng_probe.c [args...]
set -euo pipefail
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
W="${CINDER_BUILD_DIR:-$HOME/.cache/cinder-core}"
SRC="$1"
shift || true
NAME="$(basename "${SRC%.c}")"
cp "$HERE/$SRC" "$W/$NAME.c"
# The vegetation family file is written by another worker and may be missing
# its lookup table while it is in progress; build probes with the include
# pointed at a non-existent path so the weak fallback in vanilla_features.c
# supplies feature_vegetation_lookup.
VEG_STUB=0
if ! grep -q '^FeatureFn feature_vegetation_lookup' "$HERE/src/vanilla_feature_vegetation.c" 2>/dev/null; then
  VEG_STUB=1
fi
EXTRA=()
[ "$VEG_STUB" = 1 ] && EXTRA+=("-DCINDER_INC_VANILLA_FEATURE_VEGETATION_C=\"/nonexistent/vanilla_feature_vegetation.c\"")
clang -O2 -std=gnu11 -Wno-unused-function -I "$W" "${EXTRA[@]}" -o "$W/$NAME" "$W/$NAME.c" -lm -lpthread
exec "$W/$NAME" "$@"
