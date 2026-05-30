#!/usr/bin/env bash
set -euo pipefail

# Usage: gprof_run.sh [-e executable] [-s suffix] [-o outdir] [-- args]
# If no executable is supplied, the script rebuilds the default target with
# `make gprof` and runs ./grid_bfs from the script directory.

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$DIR/.." && pwd)"
exe=""
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

output_dir="$base_outdir/${suffix:-default}"
mkdir -p "$output_dir"

if [ -z "$exe" ]; then
  echo "Building gprof instrumented binary (make gprof) in $ROOT_DIR"
  (
    cd "$ROOT_DIR"
    make gprof
  )
  mkdir -p "$ROOT_DIR/build"
  cp -f "$ROOT_DIR/grid_bfs" "$ROOT_DIR/build/grid_bfs"
  exe="$ROOT_DIR/build/grid_bfs"
fi

if [ ! -x "$exe" ]; then
  echo "Executable not found or not executable: $exe" >&2
  exit 2
fi

gmon_file="$output_dir/gmon.out"
gprof_file="$output_dir/gprof.txt"

echo "Running instrumented binary: $exe ${program_args[*]}"
(
  cd "$ROOT_DIR"
  taskset -c 0 "$exe" "${program_args[@]}"
)

if [ ! -f "$ROOT_DIR/gmon.out" ]; then
  echo "gmon.out not found after run." >&2
  exit 2
fi

mv -f "$ROOT_DIR/gmon.out" "$gmon_file"
echo "Generating gprof report: $gprof_file"
gprof "$exe" "$gmon_file" > "$gprof_file"
echo "Wrote: $gmon_file and $gprof_file"
