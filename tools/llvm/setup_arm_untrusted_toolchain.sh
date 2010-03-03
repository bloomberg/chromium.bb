# ARM TOOLCHAIN SETTINGS FOR UNTRUSTED CODE
#
# 'source' this file, so that the env changes take effect inside scons
#
######################################################################
# TODO(robertm): this should be merged with similar code in
#                tools/llvm/setup_arm_newlib.sh
######################################################################

CODE_SOURCERY_INSTALL="/usr/local/crosstool-untrusted/codesourcery/arm-2007q3"
CS_BIN_PATH="${CODE_SOURCERY_INSTALL}/bin"
LLVM_BIN_PATH="/usr/local/crosstool-untrusted/arm-none-linux-gnueabi"
ILLEGAL_TOOL="${LLVM_BIN_PATH}/llvm-fake-illegal"


LD_SCRIPT_UNTRUSTED="/usr/local/crosstool-untrusted/arm-none-linux-gnueabi/ld_script_arm_untrusted"

# We reuse some of the c++ header from gcc as well (newlib does not provide any itself)
EXTRA_CPP_INCLUDE="${CODE_SOURCERY_INSTALL}/arm-none-linux-gnueabi/include/c++/4.2.1"

NACL_SDK_INSTALL="$(pwd)/src/third_party/nacl_sdk/arm-newlib/arm-none-linux-gnueabi"

######################################################################
#
######################################################################

export NACL_SDK_LIB="${NACL_SDK_INSTALL}/lib"
# NOTE: this is only use to find librt.a
export NACL_SDK_LIB2="${NACL_SDK_INSTALL}/usr/lib"
# NOTE: for libgcc(_eh)
export NACL_SDK_LIB3="${LLVM_BIN_PATH}/llvm-gcc-4.2/lib/gcc/arm-none-linux-gnueabi/4.2.1"
# NOTE: for libstdc++
export NACL_SDK_LIB4="${LLVM_BIN_PATH}/llvm-gcc-4.2/arm-none-linux-gnueabi/lib"

export NACL_SDK_INCLUDE="${NACL_SDK_INSTALL}/include"

# NOTE: NACL_* defines are currently needed for building extra_sdk, e.g.
#       for including sel_ldr.h and atomic_ops.h from nc_thread.s
# TODO(robertm): fix this
SHARED_CFLAGS="-nostdinc \
               -nostdlib \
               -D__native_client__=1 \
               -DNACL_TARGET_ARCH=arm \
               -DNACL_TARGET_SUBARCH=32 \
               -DNACL_LINUX=1 \
               -march=armv6 \
               -ffixed-r9 \
               -static \
               -isystem ${NACL_SDK_INSTALL}/../newlib_extra_header \
               -isystem ${NACL_SDK_INSTALL}/../newlib_extra_header/c++/4.2.1 \
               -isystem ${NACL_SDK_INSTALL}/../newlib_extra_header/c++/4.2.1/arm-none-linux-gnueabi \
               -isystem ${NACL_SDK_INCLUDE} \
               -isystem ${NACL_SDK_INCLUDE}/sys"

LINKER_FLAGS="-T ${LD_SCRIPT_UNTRUSTED} \
              -static \
              -L${NACL_SDK_LIB} \
              -L${NACL_SDK_LIB2} \
              -L${NACL_SDK_LIB3} \
              -L${NACL_SDK_LIB4}"

# NOTE: -isystem ${NACL_SDK_INCLUDE}/sys is not very robust:
#       as there are indentically names files in ${NACL_SDK_INCLUDE}
#       and ${NACL_SDK_INCLUDE}/sys
#       maybe we should selectively copy some files to
#       /usr/local/crosstool-untrusted/newlib_extra_header
#       but this maybe a bottomless pit

# NOTE: c.f. setup_arm_untrusted_toolchain.sh for switching to codesourcery
#       This was used before we had a working sfi toolchain
# NOTE: if you want to switch back to regular (codesourcery) you need to also
#     * change setup_arm_newlib.sh to use gcc
#     * the control flow mask in src/trusted/service_runtime/nacl_config.h to 0

ConfigRegular() {
 export NACL_SDK_CC="${CS_BIN_PATH}/arm-none-linux-gnueabi-gcc -std=gnu99 ${SHARED_CFLAGS}"
 export NACL_SDK_CXX="${CS_BIN_PATH}/arm-none-linux-gnueabi-g++ ${SHARED_CFLAGS}"

 export NACL_SDK_ASCOM="${CS_BIN_PATH}/arm-none-linux-gnueabi-as -march=armv6 -mfpu=vfp"

 export NACL_SDK_LINK="${CS_BIN_PATH}/arm-none-linux-gnueabi-ld ${LINKER_FLAGS}"

 export NACL_SDK_AR="${CS_BIN_PATH}/arm-none-linux-gnueabi-ar"
 export NACL_SDK_NM="${CS_BIN_PATH}/arm-none-linux-gnueabi-nm"
 export NACL_SDK_RANLIB="${CS_BIN_PATH}/arm-none-linux-gnueabi-ranlib"
}


# This config generates native sandboxed libraries/executables
ConfigSfi() {
 export NACL_SDK_CC="${LLVM_BIN_PATH}/llvm-fake-sfigcc -std=gnu99 ${SHARED_CFLAGS}"
 export NACL_SDK_CXX="${LLVM_BIN_PATH}/llvm-fake-sfig++ ${SHARED_CFLAGS}"

 export NACL_SDK_ASCOM="${CS_BIN_PATH}/arm-none-linux-gnueabi-as -march=armv6 -mfpu=vfp"

 export NACL_SDK_LINK="${LLVM_BIN_PATH}/llvm-fake-sfild ${LINKER_FLAGS}"

 export NACL_SDK_AR="${CS_BIN_PATH}/arm-none-linux-gnueabi-ar"
 export NACL_SDK_NM="${CS_BIN_PATH}/arm-none-linux-gnueabi-nm"
 export NACL_SDK_RANLIB="${CS_BIN_PATH}/arm-none-linux-gnueabi-ranlib"
}


# This config generates bitcode libraries
ConfigBitcode() {
  export NACL_SDK_CC="${LLVM_BIN_PATH}/llvm-fake-bcgcc -std=gnu99 ${SHARED_CFLAGS}"
  export NACL_SDK_CXX="${LLVM_BIN_PATH}/llvm-fake-bcg++ ${SHARED_CFLAGS}"

  export NACL_SDK_ASCOM="${CS_BIN_PATH}/arm-none-linux-gnueabi-as -march=armv6 -mfpu=vfp"

  export NACL_SDK_LINK="${LLVM_BIN_PATH}/llvm-fake-bcld ${LINKER_FLAGS}"

  export NACL_SDK_AR="${LLVM_BIN_PATH}/llvm/bin/llvm-ar"
  export NACL_SDK_NM="${LLVM_BIN_PATH}/llvm/bin/llvm-nm"
  export NACL_SDK_RANLIB="${LLVM_BIN_PATH}/llvm/bin/llvm-ranlib"
}

# NOTE: pick one!
#ConfigBitcode
#ConfigRegular
ConfigSfi
