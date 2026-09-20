#!/usr/bin/env bash
set -euo pipefail
cd /mnt/c/Users/nataxcan/Documents/dev/cinder
./cinder --threads 8 > /tmp/cinder_out.txt 2>&1 &
pid=$!
echo "pid=$pid"
for i in 1 2 3 4 5 6 7 8 9 10 12 15 20 25 30 40 50 60; do
  if ! kill -0 "$pid" 2>/dev/null; then
    echo "exited after ${i}s"
    cat /tmp/cinder_out.txt
    wait "$pid" || true
    exit 0
  fi
  echo "t=${i}s still running bytes=$(wc -c < /tmp/cinder_out.txt)"
  tail -5 /tmp/cinder_out.txt || true
  sleep 1
done
echo "still running after 60s; first lines:"
cat /tmp/cinder_out.txt
# leave it running
wait "$pid" || true
