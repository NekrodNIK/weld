#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

MODE="${1:-}"
THREADS="${2:-1}"

if [ -z "$MODE" ]; then
    echo "Usage: $0 {mold|ld} [threads]"
    exit 1
fi

case "$MODE" in
    mold|ld) ;;
    *) echo "Unknown mode: $MODE (use mold or ld)"; exit 1 ;;
esac

source "$SCRIPT_DIR/crt-paths.sh"

OBJ_LIST="$SCRIPT_DIR/objects.txt"
if [ ! -f "$OBJ_LIST" ]; then
    echo "Error: $OBJ_LIST not found. Run build.sh first."
    exit 1
fi

OBJ_COUNT=$(wc -l < "$OBJ_LIST")
echo "Linking $OBJ_COUNT object files with $MODE (${THREADS} thread(s))..."

RESP_FILE="$SCRIPT_DIR/link.rsp"
{
    echo "$CRT1"
    echo "$CRTI"
    echo "$CRTBEGIN"
    cat "$OBJ_LIST"
    echo "$CRTEND"
    echo "$CRTN"
} > "$RESP_FILE"

LIBS_FILE="$SCRIPT_DIR/libs.rsp"
{
    echo "$LIBC"
    echo "/usr/lib64/libcrypt.a"
    echo "$LIBGCC_EH"
    echo "$LIBGCC"
} > "$LIBS_FILE"

OUT_FILE="$SCRIPT_DIR/nginx.${MODE}.${THREADS}t"

case "$MODE" in
    mold)
        cmd=(mold -static "--threads=${THREADS}" -o "$OUT_FILE" "@$RESP_FILE" "--start-group" "@$LIBS_FILE" "--end-group")
        ;;
    ld)
        cmd=(ld.bfd -static -o "$OUT_FILE" "@$RESP_FILE" "--start-group" "@$LIBS_FILE" "--end-group")
        ;;
esac

echo "Running: ${cmd[*]}"
time "${cmd[@]}"

echo "Output: $OUT_FILE"
ls -lh "$OUT_FILE"

echo "Verifying: $OUT_FILE -v"
"$OUT_FILE" -v > /dev/null 2>&1 || {
    echo "ERROR: $OUT_FILE -v failed (exit code $?)"
    exit 1
}
echo "Verification passed."
