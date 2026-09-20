#!/usr/bin/env bash
set -euo pipefail
mkdir -p /home/nataxcan/dev/folia-run
cd /home/nataxcan/dev/folia-run
curl -fL -A "cinder-bench/1.0" -o folia.jar \
  "https://fill-data.papermc.io/v1/objects/128a634192261cd38bb4a5dc54075018a0f896fd6c6f529e37dca6e99e32b3b3/folia-26.2-7.jar"
ls -lh folia.jar
