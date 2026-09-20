#!/usr/bin/env bash
# Build a scratch copy of the worldgen translation unit for fast iteration.
#
# src/vanilla_surface.c may still be a work in progress; when it does not
# compile, pass --stub-surface to build with a neutral surface stage so the
# noise/biome/carver work can still be exercised.
#
#   scripts/build_core_test.sh                 # use every real stage file
#   scripts/build_core_test.sh --stub-surface  # stub the surface stage
set -euo pipefail
cd "$(dirname "$0")/.."

STUB_SURFACE=0
[ "${1:-}" = "--stub-surface" ] && STUB_SURFACE=1

WORK=${CINDER_BUILD_DIR:-$HOME/.cache/cinder-core}
rm -rf "$WORK"
mkdir -p "$WORK/vanilla"
cp src/*.c src/*.h "$WORK/" 2>/dev/null || true
[ -f src/vanilla_feature_vegetation.c ] || rm -f "$WORK/vanilla_feature_vegetation.c"
[ -f src/vanilla_feature_vegetation.c ] && cp src/vanilla_feature_vegetation.c "$WORK/" || true
cp src/vanilla/embed.h "$WORK/vanilla/embed.h"

if [ "$STUB_SURFACE" = 1 ]; then
  cat > "$WORK/vanilla_surface.c" <<'EOF'
#include "vanilla_common.h"
void gen_surface(Gen *g, Chunk *c, ChunkEval *ce) { (void)g; (void)c; (void)ce; }
uint16_t surface_top_material(Gen *g, Chunk *c, int lx, int ly, int lz, int under_fluid) {
  (void)g; (void)c; (void)lx; (void)ly; (void)lz; (void)under_fluid;
  return TOP_MATERIAL_NONE;
}
EOF
fi

# Sibling includes are absolute (Bend inlines the file), so rewrite every one of
# them to the scratch copy: compiling the same header twice under two different
# paths would redefine its contents.
python3 - "$WORK" <<'PY'
import pathlib, re, sys
work = pathlib.Path(sys.argv[1])
root = "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/"
for p in sorted(work.glob("*.c")) + sorted(work.glob("*.h")):
    s = p.read_text()
    t = s.replace('"' + root + 'vanilla/embed.h"', '"vanilla/embed.h"')
    t = re.sub(r'"' + re.escape(root) + r'([A-Za-z0-9_./]+)"', r'"\1"', t)
    if t != s:
        p.write_text(t)
PY

# A family file that is still being written may not define its lookup yet; with
# CINDER_STUB_VEG=1 the include is pointed at a non-existent path so the weak
# fallback in vanilla_features.c links instead.
EXTRA=()
if [ "${CINDER_STUB_VEG:-0}" = 1 ]; then
  EXTRA+=("-DCINDER_INC_VANILLA_FEATURE_VEGETATION_C=\"/nonexistent/vanilla_feature_vegetation.c\"")
fi

clang -O2 -std=gnu11 -Wno-unused-function -I "$WORK" "${EXTRA[@]}" -DCINDER_STANDALONE_MAIN \
  -o "$WORK/cinder-gen" "$WORK/vanilla_gen.c" -lm -lpthread
echo "built $WORK/cinder-gen (surface $( [ "$STUB_SURFACE" = 1 ] && echo stubbed || echo real ))"

cp bench/parity/cinder_probe.c "$WORK/"
clang -O2 -std=gnu11 -Wno-unused-function -I "$WORK" "${EXTRA[@]}" \
  -o "$WORK/cinder-probe" "$WORK/cinder_probe.c" -lm -lpthread
echo "built $WORK/cinder-probe"
