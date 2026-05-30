#!/usr/bin/env bash
set -euo pipefail

# Usage: perf_stat.sh [-e executable] [-s suffix] [-o outdir] [-- args]
# Default executable: ./grid_bfs (script directory)

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$DIR/.." && pwd)"
exe="$ROOT_DIR/build/grid_bfs"
suffix=""
base_outdir="$ROOT_DIR/outputs"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -e|--exe)
      exe="${2:-}"
      shift 2
      ;;
    -s|--suffix)
      suffix="${2:-}"
      shift 2
      ;;
    -o|--outdir)
      base_outdir="${2:-}"
      shift 2
      ;;
    --)
      shift
      break
      ;;
    -h|--help)
      echo "Usage: $0 [-e executable] [-s suffix] [-o outdir] [-- args]"
      exit 0
      ;;
    *)
      break
      ;;
  esac
done

program_args=("$@")

if [ ! -x "$exe" ]; then
  echo "Executable not found or not executable: $exe" >&2
  exit 2
fi

output_dir="$base_outdir/${suffix:-default}"
mkdir -p "$output_dir"

out_file="$output_dir/perf_stat.txt"
echo "Running perf stat on: $exe ${program_args[*]}"
taskset -c 0 perf stat -e cycles,instructions,branches,branch-misses,cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses "$exe" "${program_args[@]}" > "$out_file" 2>&1
echo "Wrote: $out_file"
