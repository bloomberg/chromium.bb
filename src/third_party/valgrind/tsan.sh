#!/bin/bash
DIR="$(dirname "$0")"
"$DIR/bin/tsan" --suppressions="$DIR/nacl.supp" --ignore="$DIR/nacl.ignore" "$@"
