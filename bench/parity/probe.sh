#!/usr/bin/env bash
# Compile and run the in-process vanilla worldgen probe (vanilla_probe.java).
#
# The probe loads the real 26.2 worldgen registries from the server jar's data/ tree via
# RegistryDataLoader and prints reference values for the C port:
#
#   probe.sh --seed 12345 --points pts.txt --router final_density
#   probe.sh --seed 12345 --points pts.txt --df minecraft:overworld/sloped_cheese
#   probe.sh --seed 12345 --points pts.txt --noise minecraft:temperature
#   probe.sh --seed 12345 --points pts.txt --biome
#
# All --<key> <value> arguments are forwarded verbatim; --data <packDir> overrides the extracted
# vanilla data pack, --points - reads the points from stdin and --hex appends the exact hex float.
#
# Requires: the deobfuscated vanilla 26.2 server jar plus its libraries, JDK 25, unzip.
set -euo pipefail

JAR="${JAR:-/home/nataxcan/dev/c2me-run/versions/26.2/server-26.2.jar}"
LIBS="${LIBS:-/home/nataxcan/dev/c2me-run/libraries}"
JAVA="${JAVA:-$HOME/jdk-25/bin/java}"
JAVAC="${JAVAC:-$HOME/jdk-25/bin/javac}"
SCRATCH="${SCRATCH:-/home/nataxcan/dev/vanilla-probe}"

here="$(cd "$(dirname "$0")" && pwd)"
pack="$SCRATCH/pack"
classes="$SCRATCH/classes"

# Extract the jar's data/ tree once (re-extracted only when the jar is newer than the stamp).
stamp="$SCRATCH/jar.stamp"
if [ ! -d "$pack/data" ] || [ ! -f "$stamp" ] || [ "$JAR" -nt "$stamp" ]; then
   rm -rf "$pack"
   mkdir -p "$pack"
   (cd "$pack" && unzip -qo "$JAR" 'data/*' -x 'data/minecraft/datapacks/*')
   # The server jar has no pack.mcmeta (VanillaPackResources supplies it in-process); PathPackResources
   # only needs the data/ tree, but a minimal file keeps the directory a valid pack root.
   printf '{"pack":{"pack_format":80,"description":"extracted vanilla 26.2 server data"}}\n' > "$pack/pack.mcmeta"
   touch "$stamp"
fi

CP="$(find "$LIBS" -name '*.jar' -print | paste -sd: -):$JAR"
mkdir -p "$classes"
"$JAVAC" -nowarn -cp "$CP" -d "$classes" "$here/vanilla_probe.java"

# The server jar's log4j2.xml sends logs to stdout, which would interleave with the reference values.
# A minimal config keeps the payload on stdout clean and the logs on stderr.
logconf="$SCRATCH/log4j2-probe.xml"
if [ ! -f "$logconf" ]; then
   cat > "$logconf" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<Configuration status="error">
   <Appenders>
      <Console name="ProbeStderr" target="SYSTEM_ERR">
         <PatternLayout pattern="[%d{HH:mm:ss}] [%t/%level]: %msg%n"/>
      </Console>
   </Appenders>
   <Loggers>
      <Root level="warn">
         <AppenderRef ref="ProbeStderr"/>
      </Root>
   </Loggers>
</Configuration>
XML
fi

if [ "$#" -eq 0 ]; then
   echo "usage: $0 --seed <n> --points <file|-> [--data <packDir>] [--hex] (--df <id> | --router <entry|all> | --noise <id> | --biome)" >&2
   exit 1
fi

exec "$JAVA" -Dlog4j2.configurationFile="$logconf" -cp "$classes:$CP" VanillaProbe --data "$pack" "$@"
