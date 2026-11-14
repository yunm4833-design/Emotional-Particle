#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
cmake -S . -B build-macos -G Xcode \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 "$@"
cmake --build build-macos --config Release --parallel 3
ctest --test-dir build-macos -C Release --output-on-failure
printf '%s\n' 'Built universal VST3, AU and Standalone in build-macos/EmotionalParticle_artefacts/Release/'
