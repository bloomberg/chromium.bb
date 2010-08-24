#!/bin/bash
DIR="$(dirname "$0")"
"$DIR/bin/memcheck" --suppressions="$DIR/nacl.supp" "$@"
