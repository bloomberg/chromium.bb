# ARM TOOLCHAIN SETTINGS FOR UNTRUSTED CODE
#
# NOTE: 'source' this file, so that the env changes take effect inside scons
#
# This is just a dummy setup for now
#
# A sample invocation would look like:
#
# ./scons MODE=nacl platform=arm  sdl_mode=none  sdl=none  naclsdk_mode=manual naclsdk_validate=0  test_exit
#
# or
#
# ./scons MODE=nacl,opt-linux  platform=arm  sdl_mode=none  sdl=none  naclsdk_mode=manual naclsdk_validate=0 --verbose  run_test_hello
#
# Other useful commands are
# ./scons platform=arm  sdl_mode=none sdl=none  naclsdk_mode=manual  naclsdk_validate=0 MODE=nacl_extra_sdk  extra_sdk_update
CODE_SOURCERY_INSTALL=/usr/local/crosstool-untrusted/codesourcery
CODE_SOURCERY_PREFIX=${CODE_SOURCERY_INSTALL}/arm-2007q3/bin/arm-none-linux-gnueabi-

LLVM_PREFIX=/usr/local/crosstool-untrusted/arm-none-linux-gnueabi/llvm-fake-sfi
LD_SCRIPT_UNTRUSTED=/usr/local/crosstool-untrusted/arm-none-linux-gnueabi/ld_script_arm_untrusted

# We reuse some of the c++ header from gcc as well (newlib does not provide any itself)
EXTRA_CPP_INCLUDE="${CODE_SOURCERY_INSTALL}/arm-2007q3/arm-none-linux-gnueabi/include/c++/4.2.1"

######################################################################
#
######################################################################

NACL_SDK_INSTALL="$(pwd)/src/third_party/nacl_sdk/arm-newlib/arm-none-linux-gnueabi"
export NACL_SDK_LIB="${NACL_SDK_INSTALL}/lib"
# NOTE: this is only use to find librt.a
export NACL_SDK_LIB2="${NACL_SDK_INSTALL}/usr/lib"
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

# NOTE: -isystem ${NACL_SDK_INCLUDE}/sys is not very robust:
#       as there are indentically names files in ${NACL_SDK_INCLUDE}
#       and ${NACL_SDK_INCLUDE}/sys
#       maybe we should selectively copy some files to
#       /usr/local/crosstool-untrusted/newlib_extra_header
#       but this maybe a bottomless pit

# NOTE: -lgcc provides softfloat bindings and needs to sfi'fied separately
# NOTE: if you want to switch back to codesourcery you need to also
#     * change setup_arm_newlib.sh to use gcc
#     * the control flow mask in src/trusted/service_runtime/nacl_config.h to 0
#PREFIX=${CODE_SOURCERY_PREFIX}
PREFIX=${LLVM_PREFIX}
export NACL_SDK_CC="${PREFIX}gcc -std=gnu99 ${SHARED_CFLAGS}"
export NACL_SDK_CXX="${PREFIX}g++ ${SHARED_CFLAGS}"
export NACL_SDK_LINK="${PREFIX}gcc \
                      -nostdlib \
                      -Wl,-T -Wl,${LD_SCRIPT_UNTRUSTED} \
                      -L${NACL_SDK_LIB} \
                      -L${NACL_SDK_LIB2}"

export NACL_SDK_AS="${CODE_SOURCERY_PREFIX}as"
export NACL_SDK_AR="${CODE_SOURCERY_PREFIX}ar"
export NACL_SDK_RANLIB="${CODE_SOURCERY_PREFIX}ranlib"

