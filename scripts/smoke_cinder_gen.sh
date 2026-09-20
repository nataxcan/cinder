#!/usr/bin/env bash
set -euo pipefail
cd /mnt/c/Users/nataxcan/Documents/dev/cinder
export CINDER_GRID="${CINDER_GRID:-1}"
echo "CINDER_GRID=$CINDER_GRID"
./cinder --threads "${1:-8}" > /tmp/cinder_out.txt 2>&1 &
pid=$!
echo "pid=$pid"
end=$((SECONDS + 180))
while kill -0 "$pid" 2>/dev/null; do
  if grep -q "world ready" /tmp/cinder_out.txt 2>/dev/null; then
    echo "READY"
    cat /tmp/cinder_out.txt
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    exit 0
  fi
  if grep -qiE "error|fault|abort" /tmp/cinder_out.txt 2>/dev/null; then
    echo "ERROR"
    cat /tmp/cinder_out.txt
    kill -9 "$pid" 2>/dev/null || true
    exit 1
  fi
  echo "t=${SECONDS}s bytes=$(wc -c < /tmp/cinder_out.txt)"
  tail -3 /tmp/cinder_out.txt || true
  if [ "$SECONDS" -gt 180 ]; then
    echo "TIMEOUT"
    cat /tmp/cinder_out.txt
    kill -9 "$pid" 2>/dev/null || true
    exit 1
  fi
  sleep 2
done
echo "exited"
cat /tmp/cinder_out.txt
wait "$pid" || true
