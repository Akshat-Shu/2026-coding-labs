#!/usr/bin/env bash
set -euo pipefail

# Usage: run_all.sh [-s suffix] [-o outdir] source.cpp [-- args]
# Builds a source file into profiler-specific binaries and runs all helper
# scripts so their outputs land in the same output directory.

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$DIR/.." && pwd)"
suffix=""
base_outdir="$ROOT_DIR/outputs"
source_file=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -s|--suffix)
      suffix="${2:-}"
      shift 2
      ;;
    -o|--outdir)
      base_outdir="${2:-}"
      shift 2
      ;;
    -h|--help)
      echo "Usage: $0 [-s suffix] [-o outdir] source.cpp [-- args]"
      exit 0
      ;;
    --)
      shift
      break
      ;;
    *)
      if [ -z "$source_file" ]; then
        source_file="$1"
        shift
      else
        break
      fi
      ;;
  esac
done

program_args=("$@")

if [ -z "$source_file" ]; then
  echo "Missing source file. Usage: $0 [-s suffix] [-o outdir] source.cpp [-- args]" >&2
  exit 2
fi

if [ ! -f "$source_file" ]; then
  echo "Source file not found: $source_file" >&2
  exit 2
fi

output_dir="$base_outdir/${suffix:-default}"
mkdir -p "$output_dir" "$ROOT_DIR/build"

suffix_part=""
if [ -n "$suffix" ]; then
  suffix_part="_$suffix"
fi

stem="$(basename "$source_file")"
stem="${stem%.cpp}"

perf_exe="$ROOT_DIR/build/${stem}${suffix_part}_perf"
gprof_exe="$ROOT_DIR/build/${stem}${suffix_part}_gprof"
debug_exe="$ROOT_DIR/build/${stem}${suffix_part}_debug"
release_exe="$ROOT_DIR/build/${stem}${suffix_part}_release"

echo "Building perf binary: $perf_exe"
g++ -std=c++20 -Wall -Wextra -pedantic -O3 -g -march=native -fno-omit-frame-pointer "$source_file" -o "$perf_exe"

echo "Building gprof binary: $gprof_exe"
g++ -std=c++20 -Wall -Wextra -pedantic -O0 -g -pg "$source_file" -o "$gprof_exe"

echo "Building debug binary: $debug_exe"
g++ -std=c++20 -Wall -Wextra -pedantic -O0 -g "$source_file" -o "$debug_exe"

echo "Building release binary: $release_exe"
g++ -std=c++20 -Wall -Wextra -pedantic -O3 -g -march=native "$source_file" -o "$release_exe"

echo "Running perf stat"
"$DIR/perf_stat.sh" -e "$perf_exe" -s "$suffix" -o "$base_outdir" -- "${program_args[@]}"

echo "Recording perf report"
"$DIR/perf_report.sh" -e "$perf_exe" -s "$suffix" -o "$base_outdir" -- "${program_args[@]}"

echo "Building FlameGraph"
"$DIR/perf_flamegraph.sh" -s "$suffix" -o "$base_outdir"

echo "Running gprof"
"$DIR/gprof_run.sh" -e "$gprof_exe" -s "$suffix" -o "$base_outdir" -- "${program_args[@]}"

echo "Running Valgrind"
"$DIR/valgrind.sh" -e "$debug_exe" -s "$suffix" -o "$base_outdir" -- "${program_args[@]}"

echo "Running Callgrind"
"$DIR/callgrind.sh" -e "$perf_exe" -s "$suffix" -o "$base_outdir" -- "${program_args[@]}"

echo "Done. Outputs are in: $output_dir"
