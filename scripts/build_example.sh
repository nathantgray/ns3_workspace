#!/usr/bin/env bash
# build_example.sh
# Copies example.cc into the ns-3 scratch directory and builds it.
# Usage: ./scripts/build_example.sh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NS3_SRC_DIR="$ROOT_DIR/ns-allinone-3.44/ns-3.44"
EXAMPLE_SRC="$ROOT_DIR/example.cc"
SCRATCH_DST="$NS3_SRC_DIR/scratch/example.cc"
EXAMPLE_BIN="$NS3_SRC_DIR/build/scratch/ns3.44-example-optimized"

# ---------------------------------------------------------------------------
# Sanity checks
# ---------------------------------------------------------------------------
if [[ ${EUID} -eq 0 ]]; then
    echo "Do NOT run this script as root. ns-3's ./ns3 refuses to run as root."
    exit 1
fi

if [[ ! -f "$EXAMPLE_SRC" ]]; then
    echo "Source file not found: $EXAMPLE_SRC"
    exit 1
fi

if [[ ! -d "$NS3_SRC_DIR" ]]; then
    echo "ns-3 source directory not found: $NS3_SRC_DIR"
    echo "Run ./scripts/build_ns3.sh first to set up the source tree."
    exit 1
fi

if [[ ! -x "$NS3_SRC_DIR/ns3" ]]; then
    echo "ns-3 build wrapper not found at $NS3_SRC_DIR/ns3"
    exit 1
fi

# ---------------------------------------------------------------------------
# Copy example.cc into scratch (only if changed, so ninja/make can cache)
# ---------------------------------------------------------------------------
if [[ ! -f "$SCRATCH_DST" ]] || ! cmp -s "$EXAMPLE_SRC" "$SCRATCH_DST"; then
    echo "Copying $EXAMPLE_SRC -> $SCRATCH_DST"
    cp "$EXAMPLE_SRC" "$SCRATCH_DST"
else
    echo "Scratch copy is up to date; skipping copy"
fi

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
echo "Building scratch/example..."
cd "$NS3_SRC_DIR"
./ns3 build scratch/example

# ---------------------------------------------------------------------------
# Verify binary
# ---------------------------------------------------------------------------
if [[ -x "$EXAMPLE_BIN" ]]; then
    echo ""
    echo "Build successful."
    echo "Binary: $EXAMPLE_BIN"
    echo ""
    echo "Run with: sudo ./scripts/run_example_cc.sh"
else
    echo ""
    echo "Build command completed but binary not found at:"
    echo "  $EXAMPLE_BIN"
    echo ""
    echo "Check the build output above for errors, or look for the binary under:"
    find "$NS3_SRC_DIR/build/scratch" -name '*example*' -type f 2>/dev/null || true
    exit 1
fi