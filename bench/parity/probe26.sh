#!/usr/bin/env bash
# Compile and run a small Java probe against the 26.2 server jar.
#
#   probe26.sh RngProbe [args...]
set -euo pipefail
JAR="${JAR:-/home/nataxcan/dev/c2me-run/versions/26.2/server-26.2.jar}"
LIBS="${LIBS:-/home/nataxcan/dev/c2me-run/libraries}"
JAVA="${JAVA:-$HOME/jdk-25/bin/java}"
JAVAC="${JAVAC:-$HOME/jdk-25/bin/javac}"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${PROBE_OUT:-$HOME/.cache/probe26}"

mkdir -p "$OUT"
CP="$JAR"
while IFS= read -r jar; do CP="$CP:$jar"; done < <(find "$LIBS" -name '*.jar')

CLASS="$1"
shift || true
if [ ! -f "$OUT/$CLASS.class" ]; then
  "$JAVAC" -cp "$CP" -d "$OUT" "$HERE/$CLASS.java"
fi
exec "$JAVA" -cp "$OUT:$CP" "$CLASS" "$@"
