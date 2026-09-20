#!/usr/bin/env bash
# Disassemble a vanilla class from the 26.2 server jar.
# usage: javap26.sh <class/path/Name.class> [javap args...]
set -euo pipefail
JAR=/home/nataxcan/dev/c2me-run/versions/26.2/server-26.2.jar
JDK=/home/nataxcan/jdk-25/bin
WORK=/tmp/jx
mkdir -p "$WORK"
if [ ! -f "$WORK/$1" ]; then
  (cd "$WORK" && unzip -o -q "$JAR" "$1")
fi
"$JDK/javap" -p -c "$WORK/$1" "${@:2}"
