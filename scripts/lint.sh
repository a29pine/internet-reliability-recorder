#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=${BUILD_DIR:-build}

if ! command -v clang-tidy >/dev/null 2>&1; then
  echo "clang-tidy not found. Please install it to run lint." >&2
  exit 0
fi

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "Build directory '$BUILD_DIR' not found. Run scripts/build.sh first." >&2
  exit 1
fi

# Run clang-tidy over sources using compile_commands.json from the build directory.
find src tests -name '*.cpp' | xargs clang-tidy -p "$BUILD_DIR"
