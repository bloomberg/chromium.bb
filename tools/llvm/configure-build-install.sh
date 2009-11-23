#!/bin/bash
#
# This script must be run from the  native_client/ dir


CS_TARBALL=arm-2007q3-51-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2


#FIXME: do not check-in the next line
CS_URL=
#CS_URL=http://www.codesourcery.com/sgpp/lite/arm/portal/package1787/public/arm-none-linux-gnueabi/${CS_TARBALL}

export INSTALL_ROOT=/usr/local/crosstool2
export CODE_SOURCERY_PKG_PATH=${INSTALL_ROOT}/crosstool2/
export LLVM_PKG_PATH=$(readlink -f ../third_party/llvm)
export LLVM_SVN_REV=88663
export LLVMGCC_SVN_REV=88663


DownloadOrCopyCodeSourceryTarball() {
  if    ; then
    wget ${CS_URL} -O ${CODE_SOURCERY_PKG_PATH}/${CS_TARBALL}
  elif
    cp ${CS_URL} ${CODE_SOURCERY_PKG_PATH}/${CS_TARBALL}
  fi
}

# We need to get rid of the sudo stuff to enable automatic builds
ExtractLlvmBuildScript() {
  tar jxf ${LLVM_PKG_PATH}/llvm-88663.tar.bz2 llvm/utils/crosstool/ARM/build-install-linux.sh -O | sed -e 's/sudo//' > tools/llvm/build-install-linux.sh
}


ConfigureAndBuildLlvm() {
  export MAKE_OPTS="-j6 VERBOSE=1"
  export CC=$(readlink -f tools/llvm/mygcc32)
  export CXX=$(readlink -f tools/llvm/myg++32)

  nice tools/llvm/build-install-linux.sh
}



ExtractLlvmBuildScript
ConfigureAndBuildLlvm
