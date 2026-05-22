#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

CRT1=$(gcc --print-file-name=crt1.o)
CRTI=$(gcc --print-file-name=crti.o)
CRTN=$(gcc --print-file-name=crtn.o)
CRTBEGIN=$(gcc --print-file-name=crtbegin.o)
CRTEND=$(gcc --print-file-name=crtend.o)
LIBC=$(gcc --print-file-name=libc.a)
LIBGCC=$(gcc --print-file-name=libgcc.a)
LIBGCC_EH=$(gcc --print-file-name=libgcc_eh.a)

cat > "$SCRIPT_DIR/crt-paths.sh" << EOF
CRT1=$CRT1
CRTI=$CRTI
CRTN=$CRTN
CRTBEGIN=$CRTBEGIN
CRTEND=$CRTEND
LIBC=$LIBC
LIBGCC=$LIBGCC
LIBGCC_EH=$LIBGCC_EH
EOF

echo "CRT1=$CRT1"
echo "CRTI=$CRTI"
echo "CRTN=$CRTN"
echo "CRTBEGIN=$CRTBEGIN"
echo "CRTEND=$CRTEND"
echo "LIBC=$LIBC"
echo "LIBGCC=$LIBGCC"
echo "LIBGCC_EH=$LIBGCC_EH"
