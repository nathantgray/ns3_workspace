#!/usr/bin/env bash
# build_ns3.sh
# Build ns-3.44 from source with Python bindings, tap-bridge, and real-time support.
# Run as a normal user (NOT root). Usage: ./scripts/build_ns3.sh

set -euo pipefail

NS3_VERSION="3.44"
NS3_TARBALL="ns-allinone-${NS3_VERSION}.tar.bz2"
NS3_URL="https://www.nsnam.org/releases/${NS3_TARBALL}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="$ROOT_DIR/ns-allinone-${NS3_VERSION}/ns-${NS3_VERSION}"
INSTALL_DIR="$ROOT_DIR/ns3-install"
VENV_DIR="$ROOT_DIR/.venv"
VENV_PY="$VENV_DIR/bin/python"
LOG_FILE="$ROOT_DIR/build_ns3.log"

log()  { echo "[$(date +%H:%M:%S)] $*" | tee -a "$LOG_FILE"; }
hdr()  { log ""; log "=================================================================="; log "  $*"; log "=================================================================="; }
fail() { log "ERROR: $*"; exit 1; }

: > "$LOG_FILE"
log "Build started at $(date -Is)"
log "Root dir:    $ROOT_DIR"
log "Install dir: $INSTALL_DIR"

[[ ${EUID} -eq 0 ]] && fail "Do NOT run as root. sudo is invoked where needed."

hdr "1. Install system dependencies"
sudo apt-get update 2>&1 | tee -a "$LOG_FILE"
sudo apt-get install -y \
    g++ cmake ninja-build pkg-config git wget ca-certificates \
    python3 python3-dev python3-pip python3-venv \
    clang libclang-dev llvm-dev \
    libboost-all-dev libssl-dev libsqlite3-dev libgsl-dev libxml2-dev \
    libpcap-dev libgcrypt20-dev strace 2>&1 | tee -a "$LOG_FILE" \
    || fail "apt-get install failed"

hdr "2. Ensure python3.12-dev is present for the venv interpreter"
if [[ -x "$VENV_PY" ]]; then
    PY_VER="$("$VENV_PY" -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')"
    log "Venv Python: $PY_VER"
    sudo apt-get install -y "python${PY_VER}-dev" 2>&1 | tee -a "$LOG_FILE" \
        || log "python${PY_VER}-dev not available; continuing."
fi

hdr "3. Remove broken PyPI ns3 wheel"
if [[ -x "$VENV_PY" ]]; then
    "$VENV_PY" -m pip uninstall -y ns3 2>&1 | tee -a "$LOG_FILE" || true
fi

hdr "4. Download ns-3 source"
cd "$ROOT_DIR"
if [[ ! -d "$SRC_DIR" ]]; then
    [[ -f "$NS3_TARBALL" ]] || wget -q --show-progress "$NS3_URL" 2>&1 | tee -a "$LOG_FILE"
    tar xjf "$NS3_TARBALL" 2>&1 | tee -a "$LOG_FILE"
fi
[[ -d "$SRC_DIR" ]] || fail "Source not found: $SRC_DIR"

hdr "5. Install cppyy into venv (for Python bindings)"
if [[ -x "$VENV_PY" ]]; then
    "$VENV_PY" -m pip install --upgrade pip 2>&1 | tee -a "$LOG_FILE"
    "$VENV_PY" -m pip install "cppyy>=3.5.0" 2>&1 | tee -a "$LOG_FILE" \
        || fail "cppyy install failed"
fi

hdr "6. Configure ns-3"
cd "$SRC_DIR"
CFG_ENV=()
[[ -x "$VENV_PY" ]] && CFG_ENV+=("PYTHON=$VENV_PY")

env "${CFG_ENV[@]}" ./ns3 configure \
    --enable-examples \
    --enable-tests \
    --enable-python-bindings \
    --enable-sudo \
    --prefix="$INSTALL_DIR" \
    2>&1 | tee -a "$LOG_FILE" \
    || fail "configure failed"

hdr "7. Build ns-3 (10-30 minutes)"
./ns3 build -j"$(nproc)" 2>&1 | tee -a "$LOG_FILE" || fail "build failed"

hdr "8. Install ns-3"
sudo ./ns3 install 2>&1 | tee -a "$LOG_FILE" || fail "install failed"

hdr "9. Register libraries with ldconfig"
{
    echo "$INSTALL_DIR/lib"
    [[ -d "$INSTALL_DIR/lib64" ]] && echo "$INSTALL_DIR/lib64"
} | sudo tee /etc/ld.so.conf.d/ns3.conf >/dev/null
sudo ldconfig
log "ldconfig updated"

hdr "10. Locate tap-creator and verify linking"
TAP_CREATOR="$(find "$INSTALL_DIR" "$SRC_DIR/build" -name 'tap-creator*' -type f -executable 2>/dev/null | head -1 || true)"
if [[ -n "$TAP_CREATOR" ]]; then
    log "tap-creator: $TAP_CREATOR"
    ldd "$TAP_CREATOR" 2>&1 | tee -a "$LOG_FILE"
    ldd "$TAP_CREATOR" | grep -q 'not found' \
        && log "WARNING: unresolved libraries above" \
        || log "All libraries resolved"
else
    log "WARNING: tap-creator not found after install"
fi

hdr "11. Python bindings sanity check"
PY_DIST=""
for d in "$INSTALL_DIR/lib/python3/dist-packages" "$INSTALL_DIR/lib64/python3/dist-packages"; do
    [[ -d "$d" ]] && PY_DIST="$d" && break
done

if [[ -n "$PY_DIST" && -x "$VENV_PY" ]]; then
    log "Python bindings: $PY_DIST"
    PYTHONPATH="$PY_DIST" LD_LIBRARY_PATH="$INSTALL_DIR/lib:$INSTALL_DIR/lib64" \
        "$VENV_PY" -c "from ns import ns; print('ns import OK')" \
        2>&1 | tee -a "$LOG_FILE" \
        || log "WARNING: Python import failed"
else
    log "WARNING: Python bindings dir not found"
fi

hdr "12. Next steps"
cat <<EOF | tee -a "$LOG_FILE"

Build complete. Update scripts/run_example.sh [1] replacing the NS3_LIB_DIR /
NS3_LIBEXEC_DIR lines with:

    NS3_INSTALL="$INSTALL_DIR"
    NS3_LIB_DIR="\$NS3_INSTALL/lib"
    NS3_LIBEXEC_DIR="$(dirname "${TAP_CREATOR:-$INSTALL_DIR/libexec/ns3}")"
    PY_DIST="$PY_DIST"

And ensure both exports are present before the python call:

    export LD_LIBRARY_PATH="\$NS3_LIB_DIR\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
    export PYTHONPATH="\$PY_DIST\${PYTHONPATH:+:\$PYTHONPATH}"

Then run: sudo ./scripts/run_example.sh
EOF

log "Done at $(date -Is)"