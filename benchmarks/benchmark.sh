#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

THREAD_COUNTS=(1 2 4 8 16)

if [ ! -f "$SCRIPT_DIR/objects.txt" ]; then
    echo "Error: objects.txt not found. Run build.sh first."
    exit 1
fi

echo "=== Nginx Static Link Benchmark ==="
echo "Objects: $(wc -l < "$SCRIPT_DIR/objects.txt") .o files"
echo ""

echo "Running benchmarks..."
HYPERFINE_CMD=(hyperfine --warmup 3)

# for t in "${THREAD_COUNTS[@]}"; do
#     HYPERFINE_CMD+=(-n "mold-${t}t" "./link.sh mold ${t}; rm -f nginx.mold.${t}t")
# done

for t in "${THREAD_COUNTS[@]}"; do
    HYPERFINE_CMD+=(-n "weld-${t}t" "./link.sh weld ${t}; rm -f nginx.weld.${t}t")
done

HYPERFINE_CMD+=(-n "ld" "./link.sh ld 1; rm -f nginx.ld.1t")
HYPERFINE_CMD+=(--export-markdown "$SCRIPT_DIR/results.md")
HYPERFINE_CMD+=(--export-json "$SCRIPT_DIR/results.json")

set -x
"${HYPERFINE_CMD[@]}"
set +x

{
echo "Nginx static link benchmark"
echo "============================"
echo "Date: $(date)"
echo "Objects: $(wc -l < "$SCRIPT_DIR/objects.txt") .o files"
echo ""
cat "$SCRIPT_DIR/results.md"
} | tee "$SCRIPT_DIR/summary.txt"

echo ""
echo "Results saved to $SCRIPT_DIR/results.md"
