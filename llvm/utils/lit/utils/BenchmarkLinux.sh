#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
    bash BenchmarkLinux.sh <test_path> <workers> [warmup] [runs] [label]
    bash BenchmarkLinux.sh --help|-h

Arguments:
    test_path: Path to the lit tests to benchmark (e.g. llvm/test/CodeGen/X86)
    workers:   Number of parallel workers to use for benchmarking
    label:     Optional label to append to results directory (default: "run")
               use "baseline" before making changes
    warmup:    Number of warmup runs for hyperfine to warm cache lines
               before benchmarking (default: 8)
    runs:      Number of benchmark runs for hyperfine (default: 20)

Examples:
    bash BenchmarkLinux.sh llvm-project/llvm/test/CodeGen/X86 4
    bash BenchmarkLinux.sh llvm-project/llvm/test/CodeGen/X86 4 8 20 baseline
EOF
}

[[ "${1:-}" == "--help" || "${1:-}" == "-h" ]] && { usage; exit 0; }

# add LIT_TEST_PATH and LIT_WORKERS as env vars for lit to pick up
TEST_PATH="${LIT_TEST_PATH:-${1:-}}"
WORKERS="${LIT_WORKERS:-${2:-}}"
WARMUP="${3:-8}"
RUNS="${4:-20}"
LABEL="${5:-run}"

[[ -z "$TEST_PATH" || -z "$WORKERS" ]] && { echo "ERROR: test_path and workers required. Run with --help."; exit 1; }

# Expects build at parent directory of repo root
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-full"
LLVM_SRC="$REPO_ROOT/llvm-project/llvm"
NCPU=$(nproc)
TIMESTAMP=$(date +%d%m%Y-%H%M%S)
RESULTS_DIR="$REPO_ROOT/results/${TIMESTAMP}-${LABEL}"

check_cmd() {
    command -v "$1" &>/dev/null || { echo "ERROR: '$1' not found. sudo apt install $2"; exit 1; }
}

check_cmd cmake cmake
check_cmd ninja ninja
check_cmd python3 python3
check_cmd hyperfine hyperfine

if [ ! -f "$BUILD_DIR/build.ninja" ]; then
    echo "=== Configuring build ==="
    cmake -S "$LLVM_SRC" -B "$BUILD_DIR" \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_TARGETS_TO_BUILD=X86 \
      -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DLLVM_ENABLE_LLD=ON \
      -DLLVM_ENABLE_PROJECTS="" \
      -DLLVM_INCLUDE_TESTS=ON \
      -DLLVM_BUILD_TESTS=OFF \
      -DLLVM_ENABLE_ASSERTIONS=OFF
else
    echo "=== Build already configured (delete $BUILD_DIR to reconfigure) ==="
fi

echo "=== Building tools with $WORKERS workers ==="
ninja -C "$BUILD_DIR" -j "$WORKERS"
cd "$REPO_ROOT"

"$BUILD_DIR/bin/llc" --version | head -1
"$BUILD_DIR/bin/llvm-lit" --version

mkdir -p "$RESULTS_DIR"
echo "Results: $RESULTS_DIR"

# CPU setup
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo > /dev/null
sudo cpupower frequency-set -g performance > /dev/null 2>&1 || \
    echo "WARN: cpupower not found => install linux-tools-$(uname -r)"
sudo systemctl stop snapd apt-daily.timer apt-daily-upgrade.timer \
    apt-daily.service apt-daily-upgrade.service unattended-upgrades 2>/dev/null || true

LIT="$BUILD_DIR/bin/llvm-lit"

# Hyperfine benchmark
echo "=== Benchmarking: $TEST_PATH (j$WORKERS) ==="
sudo nice -n -20 taskset -c 0-5 hyperfine \
  --warmup $WARMUP \
  --runs $RUNS \
  --export-markdown "$RESULTS_DIR/hyperfine.md" \
  "$LIT $TEST_PATH -j$WORKERS --no-progress-bar"

# Restore CPU/system defaults
# Handle SIGTERM, SIGINT(??)
echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo > /dev/null
sudo cpupower frequency-set -g powersave > /dev/null 2>&1 || true
sudo systemctl start snapd apt-daily.timer apt-daily-upgrade.timer unattended-upgrades 2>/dev/null || true

echo "=== Done ==="
echo "Results: $RESULTS_DIR"
