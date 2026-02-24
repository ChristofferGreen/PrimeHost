#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

build_type="Debug"
build_dir="build-debug"
example_target=""

print_help() {
  cat <<'USAGE'
Usage: ./scripts/compile.sh [--release] [--debug] [--example <name>] [--all]

Builds PrimeHost example binaries.

Options:
  --release          Build with Release configuration (default: Debug)
  --debug            Build with Debug configuration
  --example <name>   Example to build: stub | app | gamepad_audio | <cmake-target>
  --all              Build all examples (default)
Notes:
  On macOS, this script uses clang++ (Objective-C++ requires Clang).
  On macOS, the PrimeStage UI example builds by default.
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
        app)
          example_target="primehost_app"
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

if [[ "$(uname -s)" == "Darwin" ]]; then
  cxx_compiler="clang++"
  objcxx_compiler="clang++"
else
  cxx_compiler=""
  objcxx_compiler=""
fi

if [[ "$(uname -s)" == "Darwin" ]]; then
  cache_file="$root_dir/$build_dir/CMakeCache.txt"
  if [[ -f "$cache_file" ]]; then
    cached_cxx="$(awk -F= '/^CMAKE_CXX_COMPILER:FILEPATH=/{print $2}' "$cache_file" | tail -n 1)"
    cached_objcxx="$(awk -F= '/^CMAKE_OBJCXX_COMPILER:FILEPATH=/{print $2}' "$cache_file" | tail -n 1)"
    if [[ "$cached_cxx" != *clang++* || "$cached_objcxx" != *clang++* ]]; then
      echo "Removing $build_dir (compiler mismatch: requires clang++ on macOS)" >&2
      rm -rf "$root_dir/$build_dir"
    fi
  fi
fi

cmake -S "$root_dir" -B "$root_dir/$build_dir" \
  -DCMAKE_BUILD_TYPE="$build_type" \
  -DPRIMEHOST_BUILD_EXAMPLES=ON \
  -DPRIMEHOST_BUILD_TESTS=OFF \
  ${cxx_compiler:+-DCMAKE_CXX_COMPILER="$cxx_compiler"} \
  ${objcxx_compiler:+-DCMAKE_OBJCXX_COMPILER="$objcxx_compiler"}

if [[ -n "$example_target" ]]; then
  cmake --build "$root_dir/$build_dir" --target "$example_target"
else
  cmake --build "$root_dir/$build_dir"
fi
