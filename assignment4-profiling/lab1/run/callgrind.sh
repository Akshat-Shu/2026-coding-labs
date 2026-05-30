#!/usr/bin/env bash
set -euo pipefail

# Usage: callgrind.sh [-e executable] [-s suffix] [-o outdir] [-- args]
# Default executable: ./grid_bfs (script directory)
# Produces callgrind.out.* and callgrind.txt in the output directory.

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
if [ ${#program_args[@]} -eq 0 ]; then
  program_args=(--small)
fi

if [ ! -x "$exe" ]; then
  echo "Executable not found or not executable: $exe" >&2
  exit 2
fi

output_dir="$base_outdir/${suffix:-default}"
mkdir -p "$output_dir"

out_base="$output_dir/callgrind"
echo "Running Callgrind on: $exe ${program_args[*]}"
taskset -c 0 valgrind --tool=callgrind --cache-sim=yes --callgrind-out-file="$out_base.out.%p" "$exe" "${program_args[@]}"

latest_callgrind_file="$(ls -1t "$out_base".out.* 2>/dev/null | head -n 1 || true)"
if [ -z "$latest_callgrind_file" ]; then
  echo "No callgrind.out.* file was produced." >&2
  exit 2
fi

annotated_file="$output_dir/callgrind.txt"
callgrind_annotate "$latest_callgrind_file" > "$annotated_file"
echo "Wrote: $latest_callgrind_file and $annotated_file"
