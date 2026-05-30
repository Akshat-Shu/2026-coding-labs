#!/usr/bin/env bash
set -euo pipefail

# Usage: bench.sh [-n RUNS] [-c CPU] [-w WARMUP] EXECUTABLE [args...]
# Runs EXECUTABLE under taskset and reports min/median/max wall-clock time.

runs=10
cpu=2
warmup=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        -n|--runs)    runs="$2"; shift 2 ;;
        -c|--cpu)     cpu="$2"; shift 2 ;;
        -w|--warmup)  warmup="$2"; shift 2 ;;
        -h|--help)
            echo "Usage: $0 [-n RUNS] [-c CPU] [-w WARMUP] EXECUTABLE [args...]"
            echo "  -n RUNS     number of measured runs (default: 10)"
            echo "  -c CPU      CPU to pin to via taskset (default: 2 — a P-core on most Alder Lake)"
            echo "  -w WARMUP   number of unmeasured warmup runs (default: 1)"
            exit 0
            ;;
        --) shift; break ;;
        *)  break ;;
    esac
done

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 [-n RUNS] [-c CPU] [-w WARMUP] EXECUTABLE [args...]" >&2
    exit 2
fi

exe="$1"; shift

if [[ ! -x "$exe" ]]; then
    echo "Not executable: $exe" >&2
    exit 2
fi

echo "exe    = $exe $*" >&2
echo "pinned = CPU $cpu  |  warmup = $warmup  |  measured = $runs" >&2
echo "" >&2

for ((i = 1; i <= warmup; i++)); do
    taskset -c "$cpu" "$exe" "$@" >/dev/null 2>&1
    printf "warmup %d done\n" "$i" >&2
done

times=()
for ((i = 1; i <= runs; i++)); do
    start=$(date +%s.%N)
    taskset -c "$cpu" "$exe" "$@" >/dev/null
    end=$(date +%s.%N)
    t=$(awk -v s="$start" -v e="$end" 'BEGIN { printf "%.6f", e - s }')
    times+=("$t")
    printf "run %2d: %s s\n" "$i" "$t" >&2
done

sorted=$(printf "%s\n" "${times[@]}" | sort -n)
min=$(echo "$sorted" | head -1)
max=$(echo "$sorted" | tail -1)
median=$(echo "$sorted" | awk -v n="$runs" 'NR==int((n+1)/2)')

echo "" >&2
echo "min    = $min s"
echo "median = $median s"
echo "max    = $max s"
