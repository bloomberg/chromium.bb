#!/bin/bash

set -o nounset
set -o errexit
set -x

## Ghetto setup script.
## TODO(jvoung): Use scons or something.

readonly MY_DIR=$(pwd)
readonly NACL_DIR=$(pwd)/../..
readonly PNACL_TOOLCHAIN_DIR=$(pwd)/../../toolchain/linux_arm-untrusted

# Rebuild translator.nexes and copy them over here under standard names.
echo "Ensuring Translator nexes are fresh and copying over!"
(cd ${NACL_DIR};
  tools/llvm/utman.sh install-translators srpc;
  tools/llvm/utman.sh prune-translator-install srpc;
  echo "Copying 32-bit";
  cp ${PNACL_TOOLCHAIN_DIR}/tools-sb/x8632/srpc/bin/llc \
    ${MY_DIR}/llc-x86-32.nexe;
  cp ${PNACL_TOOLCHAIN_DIR}/tools-sb/x8632/srpc/bin/ld \
    ${MY_DIR}/ld-x86-32.nexe;
  cp ${PNACL_TOOLCHAIN_DIR}/tools-sb/x8632/script/ld_script \
    ${MY_DIR}/ld_script32;
# We currently don't have a mechanism to choose 32-bit libs
#  cp ${PNACL_TOOLCHAIN_DIR}/libs-x8632/* ${MY_DIR}/.;

  echo "Copying 64-bit";
  cp ${PNACL_TOOLCHAIN_DIR}/tools-sb/x8664/srpc/bin/llc \
    ${MY_DIR}/llc-x86-64.nexe;
  cp ${PNACL_TOOLCHAIN_DIR}/tools-sb/x8664/srpc/bin/ld \
    ${MY_DIR}/ld-x86-64.nexe;
  # We currently assume 64-bit and use the 64-bit script / libs
  cp ${PNACL_TOOLCHAIN_DIR}/tools-sb/x8664/script/ld_script \
    ${MY_DIR}/ld_script;
  cp ${PNACL_TOOLCHAIN_DIR}/libs-x8664/* ${MY_DIR}/.)
  # TODO(jvoung) add ARM
