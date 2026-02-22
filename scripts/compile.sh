#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

build_type="Debug"
build_dir="build-debug"
example_target="primehost_stub"

print_help() {
  cat <<'USAGE'
Usage: ./scripts/compile.sh [--release] [--debug] [--example <name>] [--all]

Builds PrimeHost example binaries.

Options:
  --release          Build with Release configuration (default: Debug)
  --debug            Build with Debug configuration
  --example <name>   Example to build: stub | gamepad_audio | <cmake-target>
  --all              Build all examples (default target)
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --release)
      build_type="Release"
      build_dir="build-release"
      shift
      ;;
    --debug)
      build_type="Debug"
      build_dir="build-debug"
      shift
      ;;
    --example)
      if [[ $# -lt 2 ]]; then
        echo "--example requires a value" >&2
        exit 1
      fi
      case "$2" in
        stub)
          example_target="primehost_stub"
          ;;
        gamepad_audio)
          example_target="primehost_gamepad_audio"
          ;;
        *)
          example_target="$2"
          ;;
      esac
      shift 2
      ;;
    --all)
      example_target=""
      shift
      ;;
    -h|--help)
      print_help
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      print_help
      exit 1
      ;;
  esac
done

cmake -S "$root_dir" -B "$root_dir/$build_dir" \
  -DCMAKE_BUILD_TYPE="$build_type" \
  -DPRIMEHOST_BUILD_EXAMPLES=ON \
  -DPRIMEHOST_BUILD_TESTS=OFF

if [[ -n "$example_target" ]]; then
  cmake --build "$root_dir/$build_dir" --target "$example_target"
else
  cmake --build "$root_dir/$build_dir"
fi
