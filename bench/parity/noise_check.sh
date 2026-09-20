#!/usr/bin/env bash
# Cross-language bit-exactness check for src/vanilla_rand_noise.c.
#
# Runs bench/parity/noise_ref.java against the real, deobfuscated vanilla 26.2
# server jar and bench/parity/noise_check.c (the C port) and diffs their output
# per section. Every value is printed as raw bits, so any diff is a real
# divergence, not a formatting artefact.
#
# Usage: bench/parity/noise_check.sh
set -uo pipefail

JAR="${JAR:-/home/nataxcan/dev/c2me-run/versions/26.2/server-26.2.jar}"
LIBS="${LIBS:-/home/nataxcan/dev/c2me-run/libraries}"
JAVA="${JAVA:-$HOME/jdk-25/bin/java}"
JAVAC="${JAVAC:-$HOME/jdk-25/bin/javac}"
CLANG="${CLANG:-clang}"

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

CP="$(find "$LIBS" -name '*.jar' -print | paste -sd: -):$JAR"

echo "== building reference (java) =="
"$JAVAC" -nowarn -cp "$CP" -d "$tmp/classes" "$here/noise_ref.java" || exit 2
"$JAVA" -cp "$tmp/classes:$CP" noise_ref > "$tmp/java.txt" || exit 2

echo "== building port (c) =="
"$CLANG" -O1 -std=gnu11 -I "$root/src" -o "$tmp/noise_check" \
    "$here/noise_check.c" "$root/src/vanilla_rand_noise.c" -lm || exit 2
"$tmp/noise_check" > "$tmp/c.txt" || exit 2

# Split each dump into per-section files keyed by the "== ... ==" header.
split_sections() {
  mkdir -p "$2"
  awk -v out="$2" 'BEGIN { file=out "/_preamble.txt" }
                   /^== / { name=$0; gsub(/[^A-Za-z0-9]+/, "_", name); file=out "/" name ".txt" }
                   { print >> file }' "$1"
}

split_sections "$tmp/java.txt" "$tmp/java_sec"
split_sections "$tmp/c.txt" "$tmp/c_sec"

echo
echo "== section results =="
fail=0
sections="$( { ls "$tmp/java_sec"; ls "$tmp/c_sec"; } | sort -u )"
for s in $sections; do
  name="$(basename "$s" .txt)"
  if [ ! -f "$tmp/java_sec/$s" ]; then
    echo "FAIL $name (missing from java output)"
    fail=1
  elif [ ! -f "$tmp/c_sec/$s" ]; then
    echo "FAIL $name (missing from c output)"
    fail=1
  elif diff -u "$tmp/java_sec/$s" "$tmp/c_sec/$s" > "$tmp/d.txt"; then
    echo "PASS $name ($(wc -l < "$tmp/c_sec/$s") lines)"
  else
    echo "FAIL $name"
    cat "$tmp/d.txt"
    fail=1
  fi
done

echo
if [ "$fail" -eq 0 ]; then
  echo "RESULT: PASS (java $(wc -l < "$tmp/java.txt") lines, c $(wc -l < "$tmp/c.txt") lines)"
else
  echo "RESULT: FAIL"
  echo "== full diff (java vs c), verbatim =="
  diff -u "$tmp/java.txt" "$tmp/c.txt"
fi
exit "$fail"
