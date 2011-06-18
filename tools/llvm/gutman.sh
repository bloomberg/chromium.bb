#!/bin/bash


# The script is located in "native_client/tools/llvm".
# Set pwd to native_client/
cd "$(dirname "$0")"/../..
LIBMODE=glibc tools/llvm/utman.sh "$@"
