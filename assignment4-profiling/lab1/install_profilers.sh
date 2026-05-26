#!/usr/bin/env bash
set -euo pipefail

FLAMEGRAPH_DIR="${HOME}/FlameGraph"
SYSCTL_CONF="/etc/sysctl.d/99-perf-lab.conf"

echo "Updating package index..."
sudo apt update

echo "Installing compiler and profiling tools..."
sudo apt install -y \
    binutils \
    build-essential \
    git \
    kcachegrind \
    linux-tools-common \
    linux-tools-generic \
    perl \
    valgrind

if ! sudo apt install -y "linux-tools-$(uname -r)"; then
    echo "WARNING: linux-tools-$(uname -r) was not available."
    echo "Continuing with linux-tools-generic; perf may still work via the generic package."
fi

echo "Installing FlameGraph into ${FLAMEGRAPH_DIR}..."
if [ -d "${FLAMEGRAPH_DIR}/.git" ]; then
    git -C "${FLAMEGRAPH_DIR}" pull --ff-only
elif [ -e "${FLAMEGRAPH_DIR}" ]; then
    echo "ERROR: ${FLAMEGRAPH_DIR} already exists but is not a git repository."
    echo "Move it aside and rerun this script, or install FlameGraph there manually."
    exit 1
else
    git clone https://github.com/brendangregg/FlameGraph.git "${FLAMEGRAPH_DIR}"
fi

echo "Configuring perf permissions..."
sudo tee "${SYSCTL_CONF}" >/dev/null <<'EOF'
# Settings for the educational profiling lab.
#
# perf_event_paranoid=-1 allows non-root users to collect CPU profiling data
# needed by perf stat, perf record, and perf report in this lab.
kernel.perf_event_paranoid = -1

# kptr_restrict=0 allows kernel symbols to be shown instead of hidden addresses.
# This makes perf reports easier to read when kernel frames appear.
kernel.kptr_restrict = 0
EOF

sudo sysctl -p "${SYSCTL_CONF}" >/dev/null

if ! command -v perf >/dev/null 2>&1; then
    PERF_BIN="$(find /usr/lib/linux-tools -type f -name perf 2>/dev/null | sort -V | tail -n 1 || true)"
    if [ -n "${PERF_BIN}" ]; then
        echo "Creating /usr/local/bin/perf symlink to ${PERF_BIN}..."
        sudo ln -sf "${PERF_BIN}" /usr/local/bin/perf
    fi
fi

echo "Checking installed tools..."
gprof --version | head -n 1
perf --version
valgrind --version
command -v kcachegrind
test -x "${FLAMEGRAPH_DIR}/flamegraph.pl"
test -x "${FLAMEGRAPH_DIR}/stackcollapse-perf.pl"

echo "Done."
echo "FlameGraph is available at: ${FLAMEGRAPH_DIR}"
echo "Use it as: ${FLAMEGRAPH_DIR}/stackcollapse-perf.pl perf.script > perf.folded"
echo "Then:      ${FLAMEGRAPH_DIR}/flamegraph.pl perf.folded > flamegraph.svg"
