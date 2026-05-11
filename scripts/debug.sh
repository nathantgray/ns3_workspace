#!/usr/bin/env bash
# debug_tap_creator.sh
# Run with: sudo ./scripts/debug_tap_creator.sh
# Collects diagnostic information about the tap-creator setup and environment.

set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
    echo "Run as root: sudo $0"
    exit 1
fi

# ---------------------------------------------------------------------------
# Paths — mirrors run_example.sh [1]
# ---------------------------------------------------------------------------
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="$ROOT_DIR/.venv/bin/python"
NS3_LIB_DIR="$ROOT_DIR/.venv/lib/python3.12/site-packages/ns3/lib64"
NS3_LIBEXEC_DIR="$ROOT_DIR/.venv/lib/python3.12/site-packages/ns3/libexec/ns3"
TAP_CREATOR="$NS3_LIBEXEC_DIR/ns3.44-tap-creator"
LOG_FILE="$ROOT_DIR/debug_tap_creator.log"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
log()  { echo "$*" | tee -a "$LOG_FILE"; }
hdr()  { log ""; log "================================================================"; log "  $*"; log "================================================================"; }
sep()  { log ""; log "--- $* ---"; }

# Start fresh log
: > "$LOG_FILE"
log "Debug run started at $(date -Is)"
log "Log file: $LOG_FILE"

# ---------------------------------------------------------------------------
hdr "1. ENVIRONMENT"
# ---------------------------------------------------------------------------
sep "Effective user"
id | tee -a "$LOG_FILE"

sep "PATH"
echo "$PATH" | tee -a "$LOG_FILE"

sep "Working directory"
pwd | tee -a "$LOG_FILE"

sep "Kernel / OS"
uname -a | tee -a "$LOG_FILE"
cat /etc/os-release 2>/dev/null | tee -a "$LOG_FILE" || true

# ---------------------------------------------------------------------------
hdr "2. CPU FLAGS (relevant to illegal-instruction crashes)"
# ---------------------------------------------------------------------------
sep "CPU model"
grep -m1 'model name' /proc/cpuinfo | tee -a "$LOG_FILE"

sep "Virtualisation"
systemd-detect-virt 2>/dev/null | tee -a "$LOG_FILE" || \
    grep -m1 -E 'hypervisor|vmx|svm' /proc/cpuinfo | tee -a "$LOG_FILE" || \
    log "(could not detect)"

sep "Selected CPU flags"
grep -m1 '^flags' /proc/cpuinfo \
    | tr ' ' '\n' \
    | grep -E '^(avx|avx2|avx512|sse4|aes|bmi)' \
    | sort \
    | tee -a "$LOG_FILE" || log "(none matched)"

# ---------------------------------------------------------------------------
hdr "3. PYTHON INTERPRETER"
# ---------------------------------------------------------------------------
sep "Interpreter path and version"
if [[ -x "$PYTHON_BIN" ]]; then
    "$PYTHON_BIN" --version 2>&1 | tee -a "$LOG_FILE"
    "$PYTHON_BIN" -c "import sys; print(sys.executable)" 2>&1 | tee -a "$LOG_FILE"
else
    log "ERROR: Python interpreter not found or not executable: $PYTHON_BIN"
fi

sep "ns3 package location"
"$PYTHON_BIN" -c "import ns3, os; print(os.path.dirname(ns3.__file__))" \
    2>&1 | tee -a "$LOG_FILE" || log "ERROR: could not import ns3"

# ---------------------------------------------------------------------------
hdr "4. TAP-CREATOR BINARY"
# ---------------------------------------------------------------------------
sep "Contents of libexec/ns3"
if [[ -d "$NS3_LIBEXEC_DIR" ]]; then
    ls -la "$NS3_LIBEXEC_DIR" | tee -a "$LOG_FILE"
else
    log "ERROR: directory not found: $NS3_LIBEXEC_DIR"
fi

sep "Expected binary path"
log "$TAP_CREATOR"

sep "Binary exists / executable?"
if [[ -f "$TAP_CREATOR" ]]; then
    log "EXISTS: yes"
    if [[ -x "$TAP_CREATOR" ]]; then
        log "EXECUTABLE: yes"
    else
        log "EXECUTABLE: NO — fixing with chmod +x"
        chmod +x "$TAP_CREATOR"
        log "chmod +x applied"
    fi
else
    log "EXISTS: NO — tap-creator binary is missing"
fi

sep "ELF / file type"
file "$TAP_CREATOR" 2>&1 | tee -a "$LOG_FILE" || true

sep "Linked libraries (ldd)"
ldd "$TAP_CREATOR" 2>&1 | tee -a "$LOG_FILE" || log "(ldd failed)"

sep "Linux capabilities on binary"
getcap "$TAP_CREATOR" 2>&1 | tee -a "$LOG_FILE" || log "(getcap not available)"

sep "Unversioned symlink (tap-creator)"
SYMLINK="$NS3_LIBEXEC_DIR/tap-creator"
if [[ -e "$SYMLINK" ]]; then
    ls -la "$SYMLINK" | tee -a "$LOG_FILE"
else
    log "Symlink does not exist — creating it now"
    ln -sf ns3.44-tap-creator "$SYMLINK"
    log "Created: $SYMLINK -> ns3.44-tap-creator"
fi

# ---------------------------------------------------------------------------
hdr "5. DIRECT EXECUTION OF TAP-CREATOR (exit code + strace)"
# ---------------------------------------------------------------------------
sep "Direct execution (no arguments — expect usage error, not a crash)"
"$TAP_CREATOR" 2>&1 | tee -a "$LOG_FILE" || log "Exit code: $?"

sep "strace of tap-creator (first 40 lines)"
if command -v strace &>/dev/null; then
    strace -e trace=execve,openat,read,mmap "$TAP_CREATOR" 2>&1 \
        | head -40 \
        | tee -a "$LOG_FILE" || true
else
    log "strace not installed — install with: sudo apt-get install -y strace"
fi

# ---------------------------------------------------------------------------
hdr "6. SECURITY POLICIES"
# ---------------------------------------------------------------------------
sep "AppArmor status"
if command -v aa-status &>/dev/null; then
    aa-status 2>&1 | head -20 | tee -a "$LOG_FILE" || true
else
    log "(aa-status not available)"
fi

sep "Recent AppArmor / seccomp / audit denials (dmesg)"
dmesg 2>/dev/null \
    | grep -iE 'apparmor|seccomp|audit|denied' \
    | tail -20 \
    | tee -a "$LOG_FILE" || log "(nothing found)"

sep "Seccomp on current shell"
grep -i seccomp /proc/$$/status 2>/dev/null | tee -a "$LOG_FILE" || true

# ---------------------------------------------------------------------------
hdr "7. TAP DEVICES AND BRIDGES"
# ---------------------------------------------------------------------------
sep "ip link (tap and bridge interfaces)"
ip link show 2>&1 \
    | grep -E 'tap|br-tap|bridge' \
    | tee -a "$LOG_FILE" || log "(none found)"

sep "bridge link"
bridge link 2>/dev/null | tee -a "$LOG_FILE" || log "(bridge command not available)"

# ---------------------------------------------------------------------------
hdr "8. LIBRARY PATH"
# ---------------------------------------------------------------------------
sep "NS3_LIB_DIR contents (lib64)"
if [[ -d "$NS3_LIB_DIR" ]]; then
    ls "$NS3_LIB_DIR" | grep tap | tee -a "$LOG_FILE" || log "(no tap libs found)"
else
    log "ERROR: $NS3_LIB_DIR not found"
fi

sep "LD_LIBRARY_PATH that run_example.sh would set"
EFFECTIVE_LD="$NS3_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
log "$EFFECTIVE_LD"

# ---------------------------------------------------------------------------
hdr "9. SUMMARY AND SUGGESTED FIXES"
# ---------------------------------------------------------------------------
ISSUES=0

if [[ ! -f "$TAP_CREATOR" ]]; then
    log "[ISSUE] tap-creator binary missing — reinstall: uv sync --reinstall-package ns3"
    (( ISSUES++ )) || true
fi

if [[ ! -e "$SYMLINK" ]]; then
    log "[ISSUE] unversioned symlink missing (was it created above?)"
    (( ISSUES++ )) || true
fi

if ! ldd "$TAP_CREATOR" 2>/dev/null | grep -q 'linux-vdso\|ld-linux'; then
    log "[ISSUE] ldd output looks wrong — binary may be wrong architecture"
    (( ISSUES++ )) || true
fi

if ! grep -qm1 '^flags' /proc/cpuinfo; then
    log "[ISSUE] could not read CPU flags"
    (( ISSUES++ )) || true
fi

if [[ $ISSUES -eq 0 ]]; then
    log "No obvious issues detected — check strace output above for SIGILL/SIGSEGV"
fi

log ""
log "================================================================"
log "Full log written to: $LOG_FILE"
log "================================================================"