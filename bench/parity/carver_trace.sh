#!/usr/bin/env bash
# Carver-stage parity check: vanilla oracle vs the C port.
#
# Builds bench/parity/carver_trace.java against the real 26.2 server jar (real registries, real
# ConfiguredWorldCarver instances, real CaveWorldCarver/CanyonWorldCarver.carve) and replays
# NoiseBasedChunkGenerator.applyCarvers' decision phase for one target chunk, logging every random
# draw. Builds a scratch copy of the C worldgen TU with -DCINDER_CARVER_TRACE, which logs the same
# draws for the same target chunk, and diffs the two logs.
#
# Usage: bench/parity/carver_trace.sh [seed] [cx] [cz]      (defaults: 1 20 11)
#
# A PASS means the carvers picked the same source chunks/carvers, derived the same
# setLargeFeatureSeed state and consumed the same RNG stream draw for draw (so the same cave
# counts, tunnel seeds and geometry) as vanilla.
set -uo pipefail

SEED="${1:-1}"
CX="${2:-20}"
CZ="${3:-11}"

JAR="${JAR:-/home/nataxcan/dev/c2me-run/versions/26.2/server-26.2.jar}"
LIBS="${LIBS:-/home/nataxcan/dev/c2me-run/libraries}"
JAVA="${JAVA:-$HOME/jdk-25/bin/java}"
JAVAC="${JAVAC:-$HOME/jdk-25/bin/javac}"
CLANG="${CLANG:-clang}"

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
pack="${SCRATCH:-/home/nataxcan/dev/vanilla-probe}/pack"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

CP="$(find "$LIBS" -name '*.jar' -print | paste -sd: -):$JAR"
logconf="${SCRATCH:-/home/nataxcan/dev/vanilla-probe}/log4j2-probe.xml"

echo "== building vanilla reference (carver_trace.java) =="
"$JAVAC" -nowarn -cp "$CP" -d "$tmp/classes" "$here/carver_trace.java" || exit 2
"$JAVA" -Dlog4j2.configurationFile="$logconf" -cp "$tmp/classes:$CP" carver_trace \
    --data "$pack" --seed "$SEED" --cx "$CX" --cz "$CZ" > "$tmp/java.txt" 2> "$tmp/java.err" || {
        echo "java reference failed:"; tail -20 "$tmp/java.err"; exit 2; }

echo "== building port (c) with -DCINDER_CARVER_TRACE =="
work="$tmp/core"
mkdir -p "$work/vanilla"
cp "$root"/src/vanilla_gen.c "$root"/src/vanilla_common.h "$root"/src/vanilla_df.c \
   "$root"/src/vanilla_biomes.c "$root"/src/vanilla_biomes.h "$root"/src/vanilla_fill.c \
   "$root"/src/vanilla_rand_noise.c "$root"/src/vanilla_features.c "$root"/src/vanilla_carvers.c \
   "$root"/src/vanilla_surface.c "$work/"
cp "$root"/src/vanilla/embed.h "$work/vanilla/embed.h"
python3 - "$work" <<'PY'
import pathlib, sys
p = pathlib.Path(sys.argv[1]) / "vanilla_gen.c"
s = p.read_text()
s = s.replace('"/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla/embed.h"', '"vanilla/embed.h"')
p.write_text(s)
PY
"$CLANG" -O1 -std=gnu11 -Wno-unused-function -I "$work" -DCINDER_STANDALONE_MAIN \
    -DCINDER_CARVER_TRACE -DCINDER_CARVER_TRACE_CX="$CX" -DCINDER_CARVER_TRACE_CZ="$CZ" \
    -o "$tmp/cinder-trace" "$work/vanilla_gen.c" -lm -lpthread || exit 2
# The trace hooks use one process-global "is this the target chunk" flag, so the trace run must be
# single-threaded (other threads generating concurrently would clobber it). CINDER_THREADS=1.
CINDER_GRID=32 CINDER_SEED="$SEED" CINDER_SKIP_STAGES=features CINDER_THREADS=1 \
    "$tmp/cinder-trace" > "$tmp/c.txt" 2> "$tmp/c.err" || {
        echo "c trace failed:"; tail -20 "$tmp/c.err"; exit 2; }

echo "== trace comparison (seed $SEED, chunk $CX,$CZ) =="
grep '^# selftest ' "$tmp/java.txt" || true
# Not part of the carver trace: the C standalone main's world checksum (C output only) and the
# Java harness' own RNG self-test header (Java output only).
grep -v '^checksum=' "$tmp/c.txt" > "$tmp/c.cmp"
grep -v '^# selftest ' "$tmp/java.txt" > "$tmp/java.cmp"
echo "java: $(wc -l < "$tmp/java.cmp") lines, $(grep -c '^SRC ' "$tmp/java.cmp") source-chunk/carver entries, $(grep -c '^X 1' "$tmp/java.cmp") starts"
echo "c   : $(wc -l < "$tmp/c.cmp") lines, $(grep -c '^SRC ' "$tmp/c.cmp") source-chunk/carver entries, $(grep -c '^X 1' "$tmp/c.cmp") starts"
echo

if diff -u "$tmp/java.cmp" "$tmp/c.cmp" > "$tmp/d.txt"; then
    echo "RESULT: PASS ($(wc -l < "$tmp/java.cmp") identical trace lines)"
    exit 0
fi

echo "RESULT: FAIL ($(grep -c '^[-+][^-+]' "$tmp/d.txt") differing lines)"
head -80 "$tmp/d.txt"
exit 1
