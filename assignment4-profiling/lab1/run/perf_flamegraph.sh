#!/usr/bin/env bash
set -euo pipefail

# Usage: perf_flamegraph.sh [-i perf.data] [-s suffix] [-o outdir]
# Default input: ./outputs/perf.data
# Optionally set FLAMEGRAPH_DIR to point to your FlameGraph installation.

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$DIR/.." && pwd)"
FLAMEGRAPH_DIR="${FLAMEGRAPH_DIR:-$HOME/FlameGraph}"
base_outdir="$ROOT_DIR/outputs"
suffix=""
input_file=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -i|--input)
      input_file="${2:-}"
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
    -h|--help)
      echo "Usage: $0 [-i perf.data] [-s suffix] [-o outdir]"
      exit 0
      ;;
    *)
      break
      ;;
  esac
done

output_dir="$base_outdir/${suffix:-default}"
mkdir -p "$output_dir"

if [ -z "$input_file" ]; then
  input_file="$output_dir/perf.data"
fi

if [ ! -f "$input_file" ]; then
  echo "perf data not found: $input_file" >&2
  exit 2
fi

stackcollapse="$FLAMEGRAPH_DIR/stackcollapse-perf.pl"
flamegraph="$FLAMEGRAPH_DIR/flamegraph.pl"

if [ ! -x "$stackcollapse" ] || [ ! -x "$flamegraph" ]; then
  echo "Cannot find FlameGraph tools in $FLAMEGRAPH_DIR" >&2
  echo "Set FLAMEGRAPH_DIR to your FlameGraph clone, or install it at ~/FlameGraph" >&2
  exit 2
fi

script_file="$output_dir/perf_script.txt"
folded_file="$output_dir/perf_folded.txt"
svg_file="$output_dir/flamegraph.svg"

perf script -i "$input_file" > "$script_file"
"$stackcollapse" "$script_file" > "$folded_file"
"$flamegraph" "$folded_file" > "$svg_file"
echo "Wrote: $script_file, $folded_file, $svg_file"
