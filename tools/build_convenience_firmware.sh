#!/usr/bin/env bash
set -euo pipefail
# Prerequisites: ESP32 Arduino core 3.3.11; autowp-mcp2515 1.3.1.
# Optional: ARDUINO_CLI=/path/to/arduino-cli ARDUINO_CONFIG_FILE=/path/to/arduino.yaml
repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
output_dir=${1:-/tmp/t2can-convenience-firmware}
mkdir -p "$output_dir"
output_dir=$(cd "$output_dir" && pwd)
staging_root=$(mktemp -d /tmp/t2can-sketch.XXXXXX)
trap 'rm -rf "$staging_root"' EXIT
sketch_name=T2CAN-Universal-v3.29.2-LP_YL
mkdir -p "$staging_root/$sketch_name"
for source_file in "$repo_root"/*.h "$repo_root"/*.ino "$repo_root"/*.cpp "$repo_root"/*.c "$repo_root"/*.csv; do
  [[ -f "$source_file" ]] && cp "$source_file" "$staging_root/$sketch_name/"
done
cli_args=()
if [[ -n "${ARDUINO_CONFIG_FILE:-}" ]]; then cli_args+=(--config-file "$ARDUINO_CONFIG_FILE"); fi
"${ARDUINO_CLI:-arduino-cli}" compile "${cli_args[@]}" \
  --fqbn 'esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,LoopCore=1,EventsCore=1' \
  --build-path "$output_dir/build" --output-dir "$output_dir" "$staging_root/$sketch_name"
