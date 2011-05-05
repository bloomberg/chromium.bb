#!/bin/bash
DIR="$(dirname "$0")"
NACL_DANGEROUS_SKIP_QUALIFICATION_TEST=1 \
"$DIR/bin/tsan" --suppressions="$DIR/nacl.supp" --ignore="$DIR/nacl.ignore" "$@"
