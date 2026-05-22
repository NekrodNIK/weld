#!/usr/bin/env bash
set +e
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BUILD_DIR="$SCRIPT_DIR/build"
OBJ_LIST="$SCRIPT_DIR/objects.txt"

if [ -f "$OBJ_LIST" ] && [ -s "$OBJ_LIST" ] && [ -d "$BUILD_DIR/objs" ]; then
    echo "Objects already built ($OBJ_LIST). Skipping."
    exit 0
fi

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

cp -r "$SRC_DIR"/* "$BUILD_DIR/"
cd "$BUILD_DIR"

echo "Configuring..."
./configure --without-http_rewrite_module --without-http_gzip_module --without-pcre --with-cc-opt="-fno-pie -O2"

LINK_RAW=$(grep -E '^LINK\s*=' objs/Makefile | head -1 | sed 's/^LINK\s*=\s*//')

sed -i 's/^LINK\s*=.*/LINK = echo/' objs/Makefile

echo "Building..."
make -j$(nproc)

cd "$SCRIPT_DIR"
find "$BUILD_DIR/objs" -name '*.o' | sort > "$OBJ_LIST"
echo "Collected $(wc -l < "$OBJ_LIST") object files"
echo "Done."
