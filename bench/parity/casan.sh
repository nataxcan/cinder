#!/usr/bin/env bash
# Build and run a Cinder-side probe under AddressSanitizer.
#
#   casan.sh bench/parity/feature_debug.c [args...]
set -euo pipefail
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
W="${CINDER_BUILD_DIR:-$HOME/.cache/cinder-core}"
SRC="$1"
shift || true
NAME="$(basename "${SRC%.c}")-asan"
cp "$HERE/$SRC" "$W/$NAME.c"
EXTRA=()
if ! grep -q '^FeatureFn feature_vegetation_lookup' "$HERE/src/vanilla_feature_vegetation.c" 2>/dev/null; then
  EXTRA+=("-DCINDER_INC_VANILLA_FEATURE_VEGETATION_C=\"/nonexistent/vanilla_feature_vegetation.c\"")
fi
clang -O0 -g -fsanitize=address -std=gnu11 -Wno-unused-function -I "$W" "${EXTRA[@]}" \
  -o "$W/$NAME" "$W/$NAME.c" -lm -lpthread
exec "$W/$NAME" "$@"
