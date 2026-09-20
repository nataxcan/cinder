#!/usr/bin/env bash
# Regression check for worldgen output: seed 1, 32x32 chunks, two stage sets.
# Any change to the generator that alters blocks must deliberately update these
# pins (see bench/WORLD.md).
set -uo pipefail
cd "$(dirname "$0")/.."

if [ ! -x ./cinder-bench ]; then
  echo "build first: bash scripts/build.sh" >&2
  exit 1
fi

expect_all=759576596
expect_parity=644477432

all=$(./cinder-bench --threads 16 2>/dev/null | tail -1)
parity=$(CINDER_SKIP_STAGES=features ./cinder-bench --threads 16 2>/dev/null | tail -1)

echo "all stages:      $all   (expected checksum=$expect_all)"
echo "features skipped:$parity   (expected checksum=$expect_parity)"

status=0
case "$all" in *"checksum=$expect_all"*) ;; *) status=1 ;; esac
case "$parity" in *"checksum=$expect_parity"*) ;; *) status=1 ;; esac

if [ "$status" -eq 0 ]; then
  echo "OK: worldgen checksums match the pins"
else
  echo "MISMATCH: worldgen output changed - if intentional, update the pins here and in bench/WORLD.md"
fi
exit $status
