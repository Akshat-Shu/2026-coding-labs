#!/usr/bin/env bash
set -euo pipefail

# Usage: perf_report.sh [-e executable] [-s suffix] [-o outdir] [-- args]
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

data_file="$output_dir/perf.data"
report_file="$output_dir/perf_report.txt"
echo "Recording perf data for: $exe ${program_args[*]}"
taskset -c 0 perf record -o "$data_file" -g -- "$exe" "${program_args[@]}"
echo "Generating text report: $report_file"
perf report -i "$data_file" --stdio > "$report_file"
echo "Wrote: $data_file and $report_file"
