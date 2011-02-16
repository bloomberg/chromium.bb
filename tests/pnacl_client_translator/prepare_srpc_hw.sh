#!/bin/bash

set -o nounset
set -o errexit
set -x

## Ghetto setup script.
## TODO(jvoung): Use scons or something.

readonly MY_DIR=$(pwd)
readonly NACL_DIR=$(pwd)/../..
readonly PNACL_TOOLCHAIN_DIR=$(pwd)/../../toolchain/linux_arm-untrusted/bin

# Generate the Bitcode for the hello_world app, from the native_client dir.
# Also try an x86-64 version for testing...
echo "Building bitcode"
${PNACL_TOOLCHAIN_DIR}/pnacl-gcc --pnacl-driver-set-CLEANUP=1 \
  ${MY_DIR}/srpc_hw.c \
  -lsrpc -limc -lpthread \
  -o ${MY_DIR}/hello_world.pexe ; \
${PNACL_TOOLCHAIN_DIR}/pnacl-translate --pnacl-driver-set-CLEANUP=1 \
  -arch x86-64 --pnacl-sb \
  ${MY_DIR}/hello_world.pexe -o ${MY_DIR}/hello_world.nexe
