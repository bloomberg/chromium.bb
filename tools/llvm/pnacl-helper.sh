#!/bin/bash
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
#@ This script performs various helper tasks for pnacl.
#@ NOTE: It must be run from the native_client/ directory.
######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

readonly PNACL_TOOLCHAIN_ROOT=toolchain/pnacl-untrusted

readonly PNACL_ARM_ROOT=${PNACL_TOOLCHAIN_ROOT}/arm
readonly PNACL_X8632_ROOT=${PNACL_TOOLCHAIN_ROOT}/x8632
readonly PNACL_X8664_ROOT=${PNACL_TOOLCHAIN_ROOT}/x8664
readonly PNACL_BITCODE_ROOT=${PNACL_TOOLCHAIN_ROOT}/bitcode

readonly PNACL_HG_CLIENT=$(readlink -f ${PNACL_TOOLCHAIN_ROOT}/hg)

readonly ARM_UNTRUSTED=$(readlink -f toolchain/linux_arm-untrusted)
readonly LLVM_DIS=${ARM_UNTRUSTED}/arm-none-linux-gnueabi/llvm/bin/llvm-dis
readonly LLVM_AR=${ARM_UNTRUSTED}/arm-none-linux-gnueabi/llvm-gcc-4.2/bin/arm-none-linux-gnueabi-ar

# NOTE: temporary measure until we have a unified llc
readonly SYMLINK_LLC_X86_32=${ARM_UNTRUSTED}/llc-x86-32-sfi
readonly SYMLINK_LLC_X86_64=${ARM_UNTRUSTED}/llc-x86-64-sfi

######################################################################
# Helpers
######################################################################

Banner() {
  echo "######################################################################"
  echo "$@"
  echo "######################################################################"
}

# Print the usage message to stdout.
Usage() {
  egrep "^#@" $0 | cut --bytes=3-
}

# Usage: VerifyObject <regexp> <filename>
# Ensure that the ELF "file format" string for <filename> matches <regexp>.
VerifyObject() {
  local pattern=$1 filename=$2
  echo -n "verify $filename [$pattern]: "
  local format=$(objdump -a "$filename" |
                 sed -ne 's/^.* file format //p' |
                 head -1)
  if echo "$format" | grep -q -e "^$pattern$"; then
    echo "PASS"
  else
    echo "FAIL (got '$format')"
    exit -1
  fi
}


# Usage: VerifyLLVMArchive <filename>
VerifyLLVMArchive() {
  echo -n "verify $1: "
  saved=$(pwd)
  local archive=$1
  local tmp=/tmp/ar-verify
  rm -rf ${tmp}
  mkdir -p ${tmp}
  cp ${archive} ${tmp}
  cd ${tmp}
  ${LLVM_AR} x $(basename ${archive})
  for i in *.o ; do
    if [ "$i" = "*.o" ]; then
      echo "FAIL (no object files in ${archive})"
      exit -1
    elif ! ${LLVM_DIS} $i -o $i.ll; then
      echo "FAIL (disassembly of $(pwd)/$i failed)"
      exit -1
    elif grep -ql asm $i.ll; then
      echo "FAIL ($(pwd)/$i.ll contained inline assembly)"
      exit -1
    else
      : ok
    fi
  done
  echo "PASS"
  cd ${saved}
}

# Usage: VerifyLLVMObj <filename>
VerifyLLVMObj() {
  echo -n "verify $1: "
  t=$(${LLVM_DIS} $1 -o -)

  if  grep asm <<<$t ; then
    echo
    echo "ERROR"
    echo
    exit -1
  fi
  echo "PASS"
}

######################################################################
# Actions
######################################################################

#@
#@ help
#@
#@   Print help for all modes.
help() {
  Usage
}

#@
#@ download-toolchains
#@
#@   Download all the toolchains + SDKs needed by PNaCl.
download-toolchains() {
  ./scons platform=arm --download sdl=none
  ./scons platform=x86-64 --download sdl=none
}

#@
#@ clean
#@
#@   Removes the toolchain/pnacl-untrusted directory, undoing the effect
#@   of all other steps except download-toolchains.
clean() {
  rm -rf ${PNACL_TOOLCHAIN_ROOT}
}

#@
#@ organize-native-code
#@
#@   Saves the native code libraries (e.g. crt*.o) for each architecture
#@   into the toolchain/pnacl-untrusted/<arch> directories.
#@
#@   (You'll need to re-run this step after you modify the native code sources
#@   and re-run the "nacl_extra_sdk" build.)
organize-native-code() {
  readonly arm_src=toolchain/linux_arm-untrusted
  readonly x86_src=toolchain/linux_x86/sdk/nacl-sdk
  readonly arm_llvm_gcc=${arm_src}/arm-none-linux-gnueabi/llvm-gcc-4.2
  Banner "arm native code: ${PNACL_ARM_ROOT}"
  mkdir -p ${PNACL_ARM_ROOT}
  # TODO(espindola): There is a transitive dependency from libgcc to
  # libc, libnosys and libnacl. We should try to factor this better.
  cp -f ${arm_llvm_gcc}/lib/gcc/arm-none-linux-gnueabi/4.2.1/libgcc.a \
    ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/libc.a \
    ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/libnosys.a \
    ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/libnacl.a \
    ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/crt*.o \
    ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/libcrt*.a \
    ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/intrinsics.o \
    ${PNACL_ARM_ROOT}
  ls -l ${PNACL_ARM_ROOT}

  # TODO(espindola): These files have been built with the convectional
  # nacl-gcc. The ABI might not be exactly the same as the one used by
  # PNaCl. We should build these files with PNaCl.
  # TODO(espindola): As for ARM, try to refactor things so that we
  # don't need as many libraries.
  Banner "x86-32 native code: ${PNACL_X8632_ROOT}"
  mkdir -p ${PNACL_X8632_ROOT}
  cp -f ${x86_src}/lib/gcc/nacl64/4.4.3/32/libgcc.a \
    ${x86_src}/nacl64/lib/32/libc.a \
    ${x86_src}/nacl64/lib/32/libnosys.a \
    ${x86_src}/nacl64/lib/32/libnacl.a \
    ${x86_src}/nacl64/lib/32/crt*.o \
    ${x86_src}/nacl64/lib/32/libcrt*.a \
    ${x86_src}/nacl64/lib/32/intrinsics.o \
    ${PNACL_X8632_ROOT}
  ls -l ${PNACL_X8632_ROOT}

  Banner "x86-64 native code: ${PNACL_X8664_ROOT}"
  mkdir -p ${PNACL_X8664_ROOT}
  cp -f ${x86_src}/lib/gcc/nacl64/4.4.3/libgcc.a \
    ${x86_src}/nacl64/lib/libc.a \
    ${x86_src}/nacl64/lib/libnosys.a \
    ${x86_src}/nacl64/lib/libnacl.a \
    ${x86_src}/nacl64/lib/crt*.o \
    ${x86_src}/nacl64/lib/libcrt*.a \
    ${PNACL_X8664_ROOT}

  HAS_INTRINSICS=0
  if [ -f ${x86_src}/nacl64/lib/intrinsics.o ]; then
    cp -f ${x86_src}/nacl64/lib/intrinsics.o ${PNACL_X8664_ROOT}
    HAS_INTRINSICS=1
  fi
  ls -l ${PNACL_X8664_ROOT}

  # TODO(pdox): Remove this banner when it is safe to assume
  #             that the SDK is up to date.
  #
  if [ $HAS_INTRINSICS -eq 0 ]; then
    echo ""
    echo ""
    echo "---------------------------- Warning -----------------------------"
    echo "|                                                                |"
    echo "|                                                                |"
    echo "|                  X86-64 intrinsics.o not found.                |"
    echo "|                                                                |"
    echo "|                                                                |"
    echo "--------------------- Please update your SDK. --------------------"
    echo ""
    echo ""
  fi
}

#@
#@ build-bitcode-cpp
#@
#@   Build bitcode libraries for C++.
build-bitcode-cpp() {
  Banner "C++"
  mkdir -p ${PNACL_BITCODE_ROOT}
  tools/llvm/untrusted-toolchain-creator.sh libstdcpp-libonly \
      ${PNACL_BITCODE_ROOT}

  Banner "Summary  ${PNACL_BITCODE_ROOT}"
  ls -l ${PNACL_BITCODE_ROOT}
}

#@
#@ build-bitcode-newlib
#@
#@   Build bitcode libraries for newlib
build-bitcode-newlib() {
  export TARGET_CODE=bc-arm  # "bc" is important; "arm" is incidental.
  Banner "Newlib"
  mkdir -p ${PNACL_BITCODE_ROOT}
  tools/llvm/untrusted-toolchain-creator.sh newlib-libonly \
       $(pwd)/${PNACL_BITCODE_ROOT}
}

#@
#@ build-bitcode-extra-sdk
#@
#@   Build bitcode libraries for extra sdk
#@   Note: clobbers toolchain/pnacl-untrusted/bitcode tree, so subsequent
#@   "verify" calls will fail.
build-bitcode-extra-sdk() {
  export TARGET_CODE=bc-arm  # "bc" is important; "arm" is incidental.
  Banner "Extra SDK"
  ./scons MODE=nacl_extra_sdk \
      platform=arm \
      sdl=none \
      naclsdk_validate=0 \
      disable_nosys_linker_warnings=1 \
      extra_sdk_clean \
      extra_sdk_update_header \
      install_libpthread \
      extra_sdk_update

  # NOTE: as collateral damage we also generate these as (arm) native code
  rm -f ${PNACL_BITCODE_ROOT}/crt*.o \
    ${PNACL_BITCODE_ROOT}/intrinsics.o \
    ${PNACL_BITCODE_ROOT}/libcrt_platform.a
}

#@
#@ build-bitcode
#@
#@   Builds bitcode libraries
build-bitcode() {
  rm -rf ${PNACL_BITCODE_ROOT}
  mkdir -p ${PNACL_BITCODE_ROOT}

  build-bitcode-newlib
  build-bitcode-extra-sdk
  build-bitcode-cpp
}

#@
#@ verify-llvm-archive <archive>
#@
#@   Verifies that a given archive is bitcode and free of ASMSs
verify-llvm-archive() {
  Banner "verify $1"
  VerifyLLVMArchive "$@"
}

#@
#@ verify-llvm-object <archive>
#@
#@   Verifies that a given .o file is bitcode and free of ASMSs
verify-llvm-object() {
  Banner "verify $1"
  VerifyLLVMObj "$@"
}

#@
#@ verify
#@
#@   Verifies that toolchain/pnacl-untrusted ELF files are of the correct
#@   architecture.
verify() {
  Banner "Verify ${PNACL_ARM_ROOT}"
  for i in ${PNACL_ARM_ROOT}/*.[oa] ; do
    VerifyObject 'elf32-little\(arm\)\?' $i  # objdumps vary in their output.
  done

  Banner "Verify ${PNACL_X8632_ROOT}"
  for i in ${PNACL_X8632_ROOT}/*.[oa] ; do
    VerifyObject elf32-i386  $i
  done

  Banner "Verify ${PNACL_X8664_ROOT}"
  for i in ${PNACL_X8664_ROOT}/*.[oa] ; do
    VerifyObject elf64-x86-64 $i
  done

  Banner "Verify ${PNACL_BITCODE_ROOT}"
  for i in ${PNACL_BITCODE_ROOT}/*.a ; do
    VerifyLLVMArchive $i
  done
  # we currently do not expect any .o files in this directory
  #for i in ${PNACL_BITCODE_ROOT}/*.o ; do
  #  VerifyLLVMObj $i
  #done
}

#@
#@ test-preparation
#@
#@   prepare for tests, i.e. set up the pnacl toolchain including"
#@   * copying of native libraries from other toolchains
#@   * building the bitcode libraries
test-preparation() {
  clean
  organize-native-code
  build-bitcode
  verify
}

readonly SCONS_ARGS=(MODE=nacl
                     platform=arm
                     sdl=none
                     naclsdk_validate=0
                     sysinfo=
                     bitcode=1)
#@
#@ test-arm-old
#@
#@   run arm tests via the old  toolchain
test-arm-old() {
  ./scons platform=arm sdl=none naclsdk_validate=0 sel_ldr
  export TARGET_CODE=sfi
  rm -rf scons-out/nacl-arm
  ./scons ${SCONS_ARGS[@]} \
          force_sel_ldr=scons-out/opt-linux-arm/staging/sel_ldr \
          smoke_tests "$@"
}

#@
#@ test-arm
#@
#@   run arm tests via pnacl toolchain
test-arm() {
  ./scons platform=arm sdl=none naclsdk_validate=0 sel_ldr
  export TARGET_CODE=bc-arm
  rm -rf scons-out/nacl-arm
  ./scons ${SCONS_ARGS[@]} \
          force_sel_ldr=scons-out/opt-linux-arm/staging/sel_ldr \
          smoke_tests "$@"
}

#@
#@ test-x86-32
#@
#@   run x86-32 tests via pnacl toolchain
test-x86-32() {
  ./scons platform=x86-32 sdl=none naclsdk_validate=0 sel_ldr
  export TARGET_CODE=bc-x86-32
  rm -rf scons-out/nacl-arm
  ./scons ${SCONS_ARGS[@]} \
          force_emulator= \
          force_sel_ldr=scons-out/opt-linux-x86-32/staging/sel_ldr \
          smoke_tests "$@"
}

#@
#@ test-x86-64
#@
#@   run x86-64 tests via pnacl toolchain
test-x86-64() {
  ./scons platform=x86-64 sdl=none sel_ldr
  export TARGET_CODE=bc-x86-64
  rm -rf scons-out/nacl-arm
  ./scons ${SCONS_ARGS[@]} \
          force_emulator= \
          force_sel_ldr=scons-out/opt-linux-x86-64/staging/sel_ldr \
          smoke_tests "$@"
}

#@
#@ test-x86-64-currently-working
#@
#@   these tests are currently working and should remain so
test-x86-64-currently-working() {
  ./scons platform=x86-64 sdl=none sel_ldr
  export TARGET_CODE=bc-x86-64
  rm -rf scons-out/nacl-arm
  ./scons ${SCONS_ARGS[@]} \
          force_emulator= \
          force_sel_ldr=scons-out/opt-linux-x86-64/staging/sel_ldr \
          run_barebones_exit_test \
          run_barebones_fib_test \
          run_barebones_hello_world_test \
          run_barebones_regs_test \
          run_barebones_switch_test \
          run_barebones_vtable_test \
          run_hello_world_test \
          run_at_exit_test \
          run_exit_test \
          run_misc_math_test \
          run_sysbasic_test \
          run_tls_test \
          run_noop_test \
          "$@"
}

#@
#@ test-all
#@
#@   all these tests should pass
test-all() {
  test-preparation
  test-arm-old
  test-arm
  test-x86-64-currently-working
  test-x86-32 -k
}

#@
#@ rebuild-all-extra-sdks
#@
#@   rebuild extra sdks for all platforms
rebuild-all-extra-sdks() {
    local PLATFORMS=(arm x86-32 x86-64)
    for p in ${PLATFORMS} ; do
      ./scons MODE=nacl_extra_sdk \
              platform=$p \
              sdl=none \
              naclsdk_validate=0 \
              extra_sdk_clean \
              extra_sdk_update_header \
              install_libpthread \
              extra_sdk_update \
              "$@"
    done
}

#@
#@ test-spec <official-spec-dir> <setup>
#@
#@   run spec tests
test-spec() {
  official=$(readlink -f $1)
  cd  tests/spec2k
  ./run_all.sh CleanBenchmarks
  ./run_all.sh PoplateFromSpecHarness ${official}
  ./run_all.sh BuildAndRunBenchmarks $2
}

#@
#@ checkout-and-build-llc
#@
#@   Checkout and build llc from the pnacl-sfi branch
#@   the tree is placed in ${PNACL_HG_CLIENT}.
checkout-and-build-llc() {
  local saved_dir=$(pwd)
  # NOTE: work-around for a socket issue in hg which is unhappy
  #       with long file names: we check-out the repo under /tmp
  #       which results in a short pathname and move it to the final
  #       location later.
  PNACL_HG_CLIENT_TMP=/tmp/pnacl-xxx
  Banner "creating llvm client in ${PNACL_HG_CLIENT}"
  rm -rf ${PNACL_HG_CLIENT}
  rm -rf ${PNACL_HG_CLIENT_TMP}
  mkdir -p ${PNACL_HG_CLIENT_TMP}

  echo "create intial hg repo in ${PNACL_HG_CLIENT_TMP}"
  cd ${PNACL_HG_CLIENT_TMP}
  hg clone https://nacl-llvm-branches.googlecode.com/hg/ nacl-llvm-branches
  cd nacl-llvm-branches
  hg up pnacl-sfi

  echo "moving  ${PNACL_HG_CLIENT_TMP} ->  ${PNACL_HG_CLIENT}"
  mv ${PNACL_HG_CLIENT_TMP} ${PNACL_HG_CLIENT}
  cd  ${PNACL_HG_CLIENT}/nacl-llvm-branches/llvm-trunk

  echo "configure + make"
  ./configure --disable-jit \
              --enable-optimized \
              --enable-targets=x86,x86_64 \
              --target=arm-none-linux-gnueabi
  make -j 6 tools-only

  cd ${saved_dir}
  Banner "do not forget to run  tools/llvm/pnacl-helper.sh install-llc"
}


#@
#@ install-llc
#@
#@   Copy a locally built llc into the toolchain for testing
install-llc() {
  pushd ${PNACL_HG_CLIENT}/nacl-llvm-branches/llvm-trunk

  if [ -f Debug/bin/llc ] ; then
     echo "ERROR: there is also a debug build in your setup"
     exit -1
  fi

  local llc=$(readlink -f Release/bin/llc)

  if [ ! -f ${llc} ] ; then
     echo "ERROR: there is no ${llc} - maybe run 'make -j 5 tools-only'"
     exit -1
  fi

  local dst=toolchain/linux_arm-untrusted/arm-none-linux-gnueabi/llvm/bin
  # back up llc the first time we install a local one
  if [ ! -f ${dst}/llc.saved ] ; then
    cp ${dst}/llc ${dst}/llc.saved
  fi

  cp -f ${llc} ${dst}
  popd
}


######################################################################
# Main

if [ $(basename $(pwd)) != "native_client" ]; then
  echo "You must run this script from the 'native_client' directory." >&2
  exit -1
fi

[ $# = 0 ] && set -- help  # Avoid reference to undefined $1.
if [ "$(type -t $1)" != "function" ]; then
  Usage
  echo "ERROR: unknown mode '$1'." >&2
  exit 1
fi

eval "$@"
