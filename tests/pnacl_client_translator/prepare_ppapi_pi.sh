#!/bin/bash

set -o nounset
set -o errexit
set -x

## Ghetto setup script.
## TODO(jvoung): Use scons or something.

readonly MY_DIR=$(pwd)
readonly NACL_DIR=$(pwd)/../..
readonly PNACL_TOOLCHAIN_DIR=$(pwd)/../../toolchain/linux_arm-untrusted/bin

echo "Building bitcode"
${PNACL_TOOLCHAIN_DIR}/pnacl-gcc \
  --pnacl-driver-verbose \
  ${MY_DIR}/pi_generator.cc ${MY_DIR}/pi_generator_module.cc \
  -lppruntime -lppapi_cpp -lplatform -lgio -lpthread -lsrpc -lm \
  -o ${MY_DIR}/pi_generator.pexe ; \
${PNACL_TOOLCHAIN_DIR}/pnacl-translate --pnacl-driver-set-CLEANUP=1 -arch x86-64 \
  --pnacl-sb --pnacl-driver-verbose \
  ${MY_DIR}/pi_generator.pexe -o ${MY_DIR}/pi_generator.nexe

