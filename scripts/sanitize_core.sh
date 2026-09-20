#!/usr/bin/env bash
# Build the scratch generator with UBSan or TSan at -O2 and run a failing case.
#   bench/parity/_san.sh ubsan|tsan [grid] [threads] [runs]
set -uo pipefail
cd /mnt/c/Users/nataxcan/Documents/dev/cinder
KIND="${1:-ubsan}"
GRID="${2:-16}"
THREADS="${3:-16}"
RUNS="${4:-1}"

case "$KIND" in
  ubsan) FLAGS="-fsanitize=undefined -fno-sanitize-recover=all" ;;
  tsan)  FLAGS="-fsanitize=thread" ;;
  *) echo "unknown kind $KIND" >&2; exit 2 ;;
esac

WORK="/home/nataxcan/.cache/cinder-"$KIND
rm -rf "$WORK"
mkdir -p "$WORK/vanilla"
cp src/vanilla_gen.c src/vanilla_common.h src/vanilla_df.c src/vanilla_biomes.c \
   src/vanilla_biomes.h src/vanilla_fill.c src/vanilla_rand_noise.c \
   src/vanilla_features.c src/vanilla_carvers.c src/vanilla_surface.c "$WORK/"
cp src/vanilla/embed.h "$WORK/vanilla/embed.h"
python3 - "$WORK" <<'PY'
import pathlib, sys
p = pathlib.Path(sys.argv[1]) / "vanilla_gen.c"
s = p.read_text().replace('"/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla/embed.h"', '"vanilla/embed.h"')
p.write_text(s)
PY

clang -O2 -g $FLAGS -std=gnu11 -Wno-unused-function -I "$WORK" -DCINDER_STANDALONE_MAIN \
  -o "${CINDER_BUILD_DIR:-$HOME/.cache/cinder-core}/cinder-gen-$KIND" "$WORK/vanilla_gen.c" -lm -lpthread
echo "built ${CINDER_BUILD_DIR:-$HOME/.cache/cinder-core}/cinder-gen-$KIND"

cd /tmp
for i in $(seq 1 "$RUNS"); do
  rm -rf "d$KIND"; mkdir -p "d$KIND"
  CINDER_GRID="$GRID" CINDER_THREADS="$THREADS" CINDER_DUMP="/tmp/d$KIND" \
    "${CINDER_BUILD_DIR:-$HOME/.cache/cinder-core}/cinder-gen-$KIND" > "/tmp/$KIND.out" 2>&1
  rc=$?
  echo "run=$i rc=$rc"
  grep -E "runtime error|WARNING: ThreadSanitizer|SUMMARY|dump written" "/tmp/$KIND.out" | head -20
done
