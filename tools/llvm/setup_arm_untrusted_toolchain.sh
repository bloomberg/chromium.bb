# ARM TOOLCHAIN SETTINGS FOR UNTRUSTED CODE
#
# 'source' this file, so that the env changes take effect inside scons
#
# This script honors the TARGET_CODE=(bc|regular|sfi) variable.

source $(dirname ${BASH_SOURCE[0]})/tools.sh || return

NACL_SDK_INSTALL=\
"$(pwd)/src/third_party/nacl_sdk/arm-newlib/arm-none-linux-gnueabi"

######################################################################
#
######################################################################

# NOTE: this is used as a destination for extra sdk builds
export NACL_SDK_LIB="${NACL_SDK_INSTALL}/lib"

export NACL_SDK_INCLUDE="${NACL_SDK_INSTALL}/include"

# NOTE: NACL_* defines are currently needed for building extra_sdk, e.g.
#       for including sel_ldr.h and atomic_ops.h from nc_thread.s
# TODO(robertm): fix this
SHARED_CFLAGS="-nostdinc \
               -D__native_client__=1 \
               -DNACL_TARGET_ARCH=arm \
               -DNACL_TARGET_SUBARCH=32 \
               -DNACL_LINUX=1 \
               -march=armv6 \
               -ffixed-r9 \
               -isystem ${NACL_SDK_INSTALL}/../newlib_extra_header \
               -isystem ${NACL_SDK_INSTALL}/../newlib_extra_header/c++/4.2.1 \
               -isystem ${NACL_SDK_INSTALL}/../newlib_extra_header/c++/4.2.1/arm-none-linux-gnueabi \
               -isystem ${NACL_SDK_INCLUDE} \
               -isystem ${NACL_SDK_INCLUDE}/sys"

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

export NACL_SDK_CC="${CC_FOR_TARGET} -std=gnu99 ${SHARED_CFLAGS}"
export NACL_SDK_CXX="${CXX_FOR_TARGET} ${SHARED_CFLAGS}"
export NACL_SDK_AR=${AR_FOR_TARGET}
export NACL_SDK_NM=${NM_FOR_TARGET}
export NACL_SDK_RANLIB=${RANLIB_FOR_TARGET}
export NACL_SDK_ASCOM=${ASCOM_FOR_TARGET}
export NACL_SDK_LINK=${LD_FOR_TARGET}

unset CS_ROOT
unset CC_FOR_TARGET
unset CXX_FOR_TARGET
unset AR_FOR_TARGET
unset NM_FOR_TARGET
unset RANLIB_FOR_TARGET
unset LD_FOR_TARGET
unset ASCOM_FOR_TARGET
unset LLVM_BIN_PATH
