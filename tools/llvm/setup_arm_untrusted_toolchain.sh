# ARM TOOLCHAIN SETTINGS FOR UNTRUSTED CODE
#
# 'source' this file, so that the env changes take effect inside scons
#
# This script honors the TARGET_CODE=(bc|regular|sfi) variable.

source $(dirname ${BASH_SOURCE[0]})/tools.sh || return

NACL_SDK_INSTALL=\
"$(pwd)/toolchain/linux_arm-untrusted/arm-newlib/arm-none-linux-gnueabi"

######################################################################
#
######################################################################

# NOTE: this is used as a destination for extra sdk builds
export NACL_SDK_LIB="${NACL_SDK_INSTALL}/lib"

# if we use bitcode we need to redirect to the bitcode libdir
if [ ${TARGET_CODE} == "bc-arm" -o  ${TARGET_CODE} == "bc-x86" ] ; then
  export NACL_SDK_LIB="$(pwd)/toolchain/pnacl-untrusted/bitcode"
fi

export NACL_SDK_INCLUDE="${NACL_SDK_INSTALL}/include"

# NOTE: NACL_* defines are currently needed for building extra_sdk, e.g.
#       for including sel_ldr.h and atomic_ops.h from nc_thread.s
# TODO(robertm): fix this

# TODO(robetrm): there is a gross hack involving the __GXX... and _IS...
#                symbols below. This likely only fixes symptoms not
#                the real cause and needs to be investigated
#                c.f.: http://code.google.com/p/nativeclient/issues/detail?id=234

SHARED_CFLAGS=

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
export NACL_SDK_CCAS="${CCAS_FOR_TARGET} -std=gnu99 ${SHARED_CFLAGS}"
export NACL_SDK_AR=${AR_FOR_TARGET}
export NACL_SDK_NM=${NM_FOR_TARGET}
export NACL_SDK_RANLIB=${RANLIB_FOR_TARGET}
export NACL_SDK_LINK=${LD_FOR_TARGET}

unset CS_ROOT
unset CC_FOR_TARGET
unset CXX_FOR_TARGET
unset CCAS_FOR_TARGET
unset AR_FOR_TARGET
unset NM_FOR_TARGET
unset RANLIB_FOR_TARGET
unset LD_FOR_TARGET
unset LLVM_BIN_PATH
