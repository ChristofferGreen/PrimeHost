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

primestage_repo=""
primeframe_repo=""
primestudio_repo=""
primestage_source_dir=""
primeframe_source_dir=""
primestudio_source_dir=""
if [[ -d "$root_dir/../PrimeStage/.git" ]]; then
  primestage_repo="$root_dir/../PrimeStage"
  primestage_source_dir="$root_dir/../PrimeStage"
fi
if [[ -d "$root_dir/../PrimeFrame/.git" ]]; then
  primeframe_repo="$root_dir/../PrimeFrame"
  primeframe_source_dir="$root_dir/../PrimeFrame"
fi
if [[ -d "$root_dir/../PrimeStudio/.git" ]]; then
  primestudio_repo="$root_dir/../PrimeStudio"
  primestudio_source_dir="$root_dir/../PrimeStudio"
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
  ${primestage_repo:+-DPRIMEHOST_PRIMESTAGE_GIT_REPOSITORY="$primestage_repo"} \
  ${primestudio_repo:+-DPRIMEHOST_PRIMESTUDIO_GIT_REPOSITORY="$primestudio_repo"} \
  ${primeframe_repo:+-DPRIMEFRAME_GIT_REPOSITORY="$primeframe_repo"} \
  ${primestage_source_dir:+-DFETCHCONTENT_SOURCE_DIR_PRIMESTAGE="$primestage_source_dir"} \
  ${primestudio_source_dir:+-DFETCHCONTENT_SOURCE_DIR_PRIMESTUDIO="$primestudio_source_dir"} \
  ${primeframe_source_dir:+-DFETCHCONTENT_SOURCE_DIR_PRIMEFRAME="$primeframe_source_dir"} \
  ${cxx_compiler:+-DCMAKE_CXX_COMPILER="$cxx_compiler"} \
  ${objcxx_compiler:+-DCMAKE_OBJCXX_COMPILER="$objcxx_compiler"}

if [[ -n "$example_target" ]]; then
  cmake --build "$root_dir/$build_dir" --target "$example_target"
else
  cmake --build "$root_dir/$build_dir"
fi
