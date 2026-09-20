#!/usr/bin/env bash
# One-shot parity verification: builds the scratch generator, runs the primitive
# checks against the vanilla jar, then compares Cinder's output block-by-block
# with the vanilla reference worlds.
#
#   bash bench/parity/verify_all.sh            # everything
#   bash bench/parity/verify_all.sh quick      # skip the 1024-chunk comparison
set -uo pipefail
cd "$(dirname "$0")/../.."
QUICK="${1:-}"

REF=/home/nataxcan/dev/ref26.2/world/region
REF_CARVERS_OFF=/home/nataxcan/dev/ref-carvers-off26.2/world/region
WORK="${CINDER_VERIFY_DIR:-$HOME/cinder-verify}"
mkdir -p "$WORK"

echo "== syntax =="
bash scripts/check_c.sh | tail -1

echo "== noise primitives (vs the real jar) =="
bash bench/parity/noise_check.sh 2>&1 | tail -3

echo "== density functions / noise / biomes (vs the real jar) =="
bash scripts/build_core_test.sh >/dev/null
bash bench/parity/df_check.sh 2>&1 | grep -E '^==|PASS|FAIL'

if [ ! -d "$REF" ]; then
  echo "!! reference world missing ($REF) - run bench/parity/gen_ref.py"
  exit 1
fi

echo "== 8x8 chunks, carvers off (isolates biomes/noise/aquifer/veins/surface) =="
rm -rf "$WORK/nocarve"; mkdir -p "$WORK/nocarve"
CINDER_GRID=32 CINDER_THREADS=16 CINDER_SKIP_STAGES=features,carvers CINDER_DUMP="$WORK/nocarve" \
  ./cinder-bench --threads 16 >/dev/null 2>&1
python3 bench/parity/diff.py --dump "$WORK/nocarve/dump.bin" --region-dir "$REF_CARVERS_OFF" \
  --cx0 16 --cz0 8 --nx 8 --nz 8 --no-per-chunk 2>&1 | grep -E 'block agreement|biome agreement'

echo "== 8x8 chunks, carvers on =="
rm -rf "$WORK/carve"; mkdir -p "$WORK/carve"
CINDER_GRID=32 CINDER_THREADS=16 CINDER_SKIP_STAGES=features CINDER_DUMP="$WORK/carve" \
  ./cinder-bench --threads 16 >/dev/null 2>&1
python3 bench/parity/diff.py --dump "$WORK/carve/dump.bin" --region-dir "$REF" \
  --cx0 16 --cz0 8 --nx 8 --nz 8 --no-per-chunk 2>&1 | grep -E 'block agreement|biome agreement'

if [ "$QUICK" != "quick" ]; then
  echo "== 1024 chunks, all ported stages (features skipped on both sides) =="
  rm -rf "$WORK/full"; mkdir -p "$WORK/full"
  CINDER_GRID=32 CINDER_THREADS=16 CINDER_SKIP_STAGES=features CINDER_DUMP="$WORK/full" \
    ./cinder-bench --threads 16 2>&1 | tail -1
  python3 bench/parity/diff.py --dump "$WORK/full/dump.bin" --region-dir "$REF" \
    --cx0 0 --cz0 0 --nx 32 --nz 32 --no-per-chunk 2>&1 | grep -E 'block agreement|biome agreement'
fi
