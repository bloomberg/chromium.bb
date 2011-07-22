#!/bin/bash

# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

# The script is located in "native_client/tests/spec2k"
# Set pwd to spec2k/
cd "$(dirname "$0")"
if [[ $(basename "$(pwd)") != "spec2k" ]] ; then
  echo "ERROR: cannot find the spec2k/ directory"
  exit -1
fi
source ../../tools/llvm/common-tools.sh
readonly NACL_ROOT="$(GetAbsolutePath "../../")"
SetScriptPath "$(pwd)/run_all.sh"
SetLogDirectory "${NACL_ROOT}/toolchain/test-log"

######################################################################
# CONFIGURATION
######################################################################
# TODO(robertm): make this configurable from the commandline

readonly LIST_INT_C="164.gzip 175.vpr 176.gcc 181.mcf 186.crafty 197.parser \
253.perlbmk 254.gap 255.vortex 256.bzip2 300.twolf"

readonly LIST_FP_C="177.mesa 179.art 183.equake 188.ammp"

readonly LIST_INT_CPP="252.eon"

SPEC2K_BENCHMARKS="${LIST_FP_C} ${LIST_INT_C} ${LIST_INT_CPP}"

# One of {./run.train.sh, ./run.ref.sh}
SPEC2K_SCRIPT="./run.train.sh"

# uncomment this to disable verification
# verification time will be part of  overall benchmarking time
# export VERIFY=no
export VERIFY=${VERIFY:-yes}
export MAKEOPTS=${MAKEOPTS:-}
export PERF_LOGGER="$(pwd)/emit_perf_log.sh"


######################################################################
# Helper
######################################################################

readonly SCONS_OUT="${NACL_ROOT}/scons-out"
readonly TC_ROOT="${NACL_ROOT}/toolchain"

readonly ARM_TRUSTED_TC="${TC_ROOT}/linux_arm-trusted"
readonly QEMU_TOOL="${ARM_TRUSTED_TC}/qemu_tool.sh"

readonly PNACL_TC="${TC_ROOT}/pnacl_${BUILD_PLATFORM}_${BUILD_ARCH}_newlib"
readonly NNACL_TC="${TC_ROOT}/${SCONS_BUILD_PLATFORM}_x86"
readonly RUNNABLE_LD_X8632="${NNACL_TC}/nacl/lib/runnable-ld.so"
readonly RUNNABLE_LD_X8664="${NNACL_TC}/nacl64/lib/runnable-ld.so"

gnu_size() {
  # If the PNaCl toolchain is installed, prefer to use its "size".
  if [ -d "${PNACL_TC}" ] ; then
    GNU_SIZE="${PNACL_TC}/bin/size"
  elif ${BUILD_PLATFORM_LINUX} ; then
    GNU_SIZE="size"
  else
    # There's nothing we can run here.
    # The system might have "size" installed, but if it is not GNU,
    # there's no guarantee it can handle ELF.
    return 0
  fi
  "${GNU_SIZE}" "$@"
}

######################################################################
# Various Setups
######################################################################
#@ invocation
#@    run_all.sh <mode> <mode-arg>*

#@ ------------------------------------------------------------
#@ Available Setups:
#@ ------------------------------------------------------------

#@
#@ SetupGccX8632
#@   use system compiler for x86-32
SetupGccX8632() {
  PREFIX=
  SUFFIX=gcc.x8632
}

#@
#@ SetupGccX8632Opt
#@   use system compiler for x86-32 with optimization
SetupGccX8632Opt() {
  PREFIX=
  SUFFIX=gcc.opt.x8632
}

#@
#@ SetupGccX8664
#@   use system compiler for x86-64
SetupGccX8664() {
  PREFIX=
  SUFFIX=gcc.x8664
}

#@
#@ SetupGccX8664Opt
#@   use system compiler for x86-64 with optimization
SetupGccX8664Opt() {
  PREFIX=
  SUFFIX=gcc.opt.x8664
}

######################################################################

SetupNaclX8632Common() {
  SEL_LDR=$(GetSelLdr "x86-32")
  PREFIX="${SEL_LDR} -a -f"
}

#@
#@ SetupNaClX8632
#@   use nacl-gcc compiler
SetupNaclX8632() {
  SetupNaclX8632Common
  SUFFIX=nacl.x8632
}

#@
#@ SetupNaClX8632Opt
#@   use nacl-gcc compiler with optimizations
SetupNaclX8632Opt() {
  SetupNaclX8632Common
  SUFFIX=nacl.opt.x8632
}

SetupNaclX8664Common() {
  SEL_LDR=$(GetSelLdr "x86-64")
  PREFIX="${SEL_LDR} -a -f"
}

#@
#@ SetupNaClX8664
#@   use nacl-gcc64 compiler
SetupNaclX8664() {
  SetupNaclX8664Common
  SUFFIX=nacl.x8664
}

#@
#@ SetupNaClX8664Opt
#@   use nacl-gcc64 compiler with optimizations
SetupNaclX8664Opt() {
  SetupNaclX8664Common
  SUFFIX=nacl.opt.x8664
}

SetupNaclDynX8632Common() {
  SEL_LDR=$(GetSelLdr "x86-32")
  PREFIX="${SEL_LDR} -a -s -f ${RUNNABLE_LD_X8632}"
}

#@
#@ SetupNaclDynX8632
#@   use nacl-gcc compiler with glibc toolchain and dynamic linking
SetupNaclDynX8632() {
  SetupNaclDynX8632Common
  SUFFIX=nacl.dyn.x8632
}

#@
#@ SetupNaclDynX8632Opt
#@   use nacl-gcc compiler with glibc toolchain and dynamic linking
SetupNaclDynX8632Opt() {
  SetupNaclDynX8632Common
  SUFFIX=nacl.dyn.opt.x8632
}

SetupNaclDynX8664Common() {
  SEL_LDR=$(GetSelLdr "x86-64")
  PREFIX="${SEL_LDR} -a -s -f ${RUNNABLE_LD_X8664}"
}

#@
#@ SetupNaclDynX8664
#@   use nacl64-gcc compiler with glibc toolchain and dynamic linking
SetupNaclDynX8664() {
  SetupNaclDynX8664Common
  SUFFIX=nacl.dyn.x8664
}

#@
#@ SetupNaclDynX8664Opt
#@   use nacl64-gcc compiler with glibc toolchain and dynamic linking
SetupNaclDynX8664Opt() {
  SetupNaclDynX8664Common
  SUFFIX=nacl.dyn.opt.x8664
}

######################################################################

SetupPnaclX8664Common() {
  CheckSDK
  SEL_LDR=$(GetSelLdr "x86-64")
  PREFIX="${SEL_LDR} -a -f"
}

#@
#@ SetupPnaclX8664
#@    use pnacl x86-64 compiler (no lto)
SetupPnaclX8664() {
  SetupPnaclX8664Common
  SUFFIX=pnacl.x8664
}

#@
#@ SetupPnaclX8664Opt
#@    use pnacl x86-64 compiler (with lto)
SetupPnaclX8664Opt() {
  SetupPnaclX8664Common
  SUFFIX=pnacl.opt.x8664
}

#@
#@ SetupPnaclTranslatorX8664
#@    use pnacl x8664 translator (no lto)
SetupPnaclTranslatorX8664() {
  CheckSelUniversal "x86-64"
  SetupPnaclX8664Common
  SUFFIX=pnacl_translator.x8664
}

#@
#@ SetupPnaclTranslatorX8664Opt
#@    use pnacl x8664 translator (with lto)
SetupPnaclTranslatorX8664Opt() {
  CheckSelUniversal "x86-64"
  SetupPnaclX8664Common
  SUFFIX=pnacl_translator.opt.x8664
}

SetupPnaclX8632Common() {
  CheckSDK
  SEL_LDR=$(GetSelLdr "x86-32")
  PREFIX="${SEL_LDR} -a -f"
}

#@
#@ SetupPnaclX8632
#@    use pnacl x86-32 compiler (no lto)
SetupPnaclX8632() {
  SetupPnaclX8632Common
  SUFFIX=pnacl.x8632
}

#@
#@ SetupPnaclX8632Opt
#@    use pnacl x86-32 compiler (with lto)
SetupPnaclX8632Opt() {
  SetupPnaclX8632Common
  SUFFIX=pnacl.opt.x8632
}


#@
#@ SetupPnaclTranslatorX8632
#@    use pnacl x8632 translator (no lto)
SetupPnaclTranslatorX8632() {
  CheckSelUniversal "x86-32"
  SetupPnaclX8632Common
  SUFFIX=pnacl_translator.x8632
}

#@
#@ SetupPnaclTranslatorX8632Opt
#@    use pnacl x8632 translator (with lto)
SetupPnaclTranslatorX8632Opt() {
  CheckSelUniversal "x86-32"
  SetupPnaclX8632Common
  SUFFIX=pnacl_translator.opt.x8632
}

#@
#@ SetupGccArm
#@   use CS cross compiler
SetupGccArm() {
  PREFIX="${QEMU_TOOL} run"
  SUFFIX=gcc.arm
}


SetupPnaclArmCommon() {
  CheckSDK
  SEL_LDR=$(GetSelLdr "arm")
  PREFIX="${QEMU_TOOL} run ${SEL_LDR} -a -Q -f"
  SUFFIX=pnacl.arm
}

#@
#@ SetupPnaclArmOpt
#@    use pnacl arm compiler (with lto)  -- run with QEMU
SetupPnaclArmOpt() {
  SetupPnaclArmCommon
  SUFFIX=pnacl.opt.arm
}

#@
#@ SetupPnaclArm
#@    use pnacl arm compiler (no lto)  -- run with QEMU
SetupPnaclArm() {
  SetupPnaclArmCommon
  SUFFIX=pnacl.arm
}

#@
#@ SetupPnaclTranslatorArm
#@    use pnacl arm translator (no lto)
SetupPnaclTranslatorArm() {
  CheckSelUniversal "arm"
  SetupPnaclArmCommon
  SUFFIX=pnacl_translator.arm
}

#@
#@ SetupPnaclTranslatorArmOpt
#@    use pnacl arm translator (with lto)
SetupPnaclTranslatorArmOpt() {
  CheckSelUniversal "arm"
  SetupPnaclArmCommon
  SUFFIX=pnacl_translator.opt.arm
}

SetupPnaclArmCommonHW() {
  SEL_LDR=$(GetSelLdr "arm")
  PREFIX="${SEL_LDR} -a -f"
  SUFFIX=pnacl.arm
}

#@
#@ SetupPnaclArmOptHW
#@    use pnacl arm compiler (with lto) -- run on ARM hardware
SetupPnaclArmOptHW() {
  SetupPnaclArmCommonHW
  SUFFIX=pnacl.opt.arm
}

#@
#@ SetupPnaclArmHW
#@    use pnacl arm compiler (no lto) -- run on ARM hardware
SetupPnaclArmHW() {
  SetupPnaclArmCommonHW
  SUFFIX=pnacl.arm
}

#@
#@ SetupPnaclTranslatorArmHW
#@    use pnacl arm translator (no lto) -- run on ARM hardware
SetupPnaclTranslatorArmHW() {
  CheckSelUniversal "arm"
  SetupPnaclArmCommonHW
  SUFFIX=pnacl_translator.hw.arm
}

#@
#@ SetupPnaclTranslatorArmOptHW
#@    use pnacl arm translator (with lto) -- run on ARM hardware
SetupPnaclTranslatorArmOptHW() {
  CheckSelUniversal "arm"
  SetupPnaclArmCommonHW
  SUFFIX=pnacl_translator.opt.hw.arm
}

ConfigInfo() {
  SubBanner "Config Info"
  echo "benchmarks: $(GetBenchmarkList "$@")"
  echo "script:     $(GetInputSize "$@")"
  echo "suffix      ${SUFFIX}"
  echo "verify      ${VERIFY}"
  echo "prefix     ${PREFIX}"

}

######################################################################
# Functions intended to be called
######################################################################
#@
#@ ------------------------------------------------------------
#@ Available Modes:
#@ ------------------------------------------------------------

#@
#@ GetBenchmarkList
#@
#@   Show available benchmarks
GetBenchmarkList() {
  if [[ $# -ge 1 ]]; then
      if [[ ($1 == "ref") || ($1 == "train") ]]; then
          shift
      fi
  fi

  if [[ $# = 0 ]] ; then
      echo "${SPEC2K_BENCHMARKS[@]}"
  else
      echo "$@"
  fi
}

#+
#+ GetInputSize [train|ref]
#+
#+  Picks input size for spec runs (train or ref)
GetInputSize() {
  if [[ $# -ge 1 ]]; then
    case $1 in
      train)
        echo "./run.train.sh"
        return
        ;;
      ref)
        echo "./run.ref.sh"
        return
        ;;
    esac
  fi
  echo ${SPEC2K_SCRIPT}
}

#+
#+ CheckFileBuilt <depname> <file> -
#+
#+   Check that a dependency is actually built.
CheckFileBuilt() {
  local depname="$1"
  local filename="$2"
  if [[ ! -x "${filename}" ]] ; then
    echo "You have not built ${depname} yet (${filename})!" 1>&2
    exit -1
  fi
}

#+
#+ GetSelLdr <arch>
#+
#+   Get sel_ldr for the given arch.
GetSelLdr() {
  local arch=$1
  SEL_LDR="${SCONS_OUT}/opt-${SCONS_BUILD_PLATFORM}-${arch}/staging/sel_ldr"
  CheckFileBuilt "sel_ldr" "${SEL_LDR}"
  echo "${SEL_LDR}"
}

CheckSelUniversal() {
  local arch=$1
  SEL_UNIV="${SCONS_OUT}/opt-${SCONS_BUILD_PLATFORM}-${arch}/staging/\
sel_universal"
  CheckFileBuilt "sel_universal" "${SEL_UNIV}"
}

CheckSDK() {
  if ! [ -d "${PNACL_TC}"/sdk ]; then
    echo "Spec2K for PNaCl requires the SDK to be installed." 1>&2
    echo "To install, run: tools/llvm/utman.sh sdk" 1>&2
    exit -1
  fi
}

#@
#@ CleanBenchmarks <benchmark>*
#@
#@   this is a deep clean and you have to rerun PoplateFromSpecHarness
CleanBenchmarks() {
  local list=$(GetBenchmarkList "$@")
  rm -rf bin/
  for i in ${list} ; do
    SubBanner "Cleaning: $i"
    cd $i
    make clean
    rm -rf src/ data/
    cd ..
  done
}

#@
#@ BuildBenchmarks <do_timing> <setup> <benchmark>*
#@
#@  Build all benchmarks according to the setup
#@  First arg should be either 0 (no timing) or 1 (run timing measure).
#@  Results are delivered to {execname}.compile_time
BuildBenchmarks() {
  export PREFIX=
  timeit=$1
  "$2"
  shift 2

  local list=$(GetBenchmarkList "$@")
  ConfigInfo "$@"
  for i in ${list} ; do
    SubBanner "Building: $i"
    cd $i

    make ${MAKEOPTS} measureit=${timeit} \
         PERF_LOGGER="${PERF_LOGGER}" \
         BUILD_PLATFORM=${BUILD_PLATFORM} \
         SCONS_BUILD_PLATFORM=${SCONS_BUILD_PLATFORM} \
         BUILD_ARCH=${BUILD_ARCH} \
         ${i#*.}.${SUFFIX}
    cd ..
  done
}


#@ TimedRunCmd <time_result_file> {actual_cmd }
#@
#@  Run the command under time and dump time data to file.
TimedRunCmd() {
  target=$1
  shift
  /usr/bin/time -f "%U %S %e %C" -o ${target} "$@"
}

#@
#@ RunBenchmarks <setup> [ref|train] <benchmark>*
#@
#@  Run all benchmarks according to the setup.
RunBenchmarks() {
  export PREFIX=
  "$1"
  shift
  local list=$(GetBenchmarkList "$@")
  local script=$(GetInputSize "$@")
  ConfigInfo "$@"
  for i in ${list} ; do
    SubBanner "Benchmarking: $i"
    cd $i
    target_file=./${i#*.}.${SUFFIX}
    gnu_size ${target_file}
    ${script} ${target_file}
    cd ..
  done
}


#@
#@ RunTimedBenchmarks <setup> [ref|train] <benchmark>*
#@
#@  Run all benchmarks according to the setup.
#@  All timing related files are stored in {execname}.run_time
#@  Note that the VERIFY variable effects the timing!
RunTimedBenchmarks() {
  export PREFIX=
  "$1"
  shift
  local list=$(GetBenchmarkList "$@")
  local script=$(GetInputSize "$@")

  ConfigInfo "$@"
  for i in ${list} ; do
    SubBanner "Benchmarking: $i"
    pushd $i
    local benchname=${i#*.}
    local target_file=./${benchname}.${SUFFIX}
    local time_file=${target_file}.run_time
    gnu_size  ${target_file}
    TimedRunCmd ${time_file} ${script} ${target_file}
    # TODO(jvoung): split runtimes by arch as well
    # i.e., pull "arch" out of SUFFIX and add to the "runtime" label.
    "${PERF_LOGGER}" LogUserSysTime "${time_file}" "runtime" \
      ${benchname} ${SUFFIX}
    popd
  done
}


#@
#@ BuildAndRunBenchmarks <setup> [ref|train] <benchmark>*
#@
#@   Builds and run all benchmarks according to the setup
BuildAndRunBenchmarks() {
  setup=$1
  shift
  BuildBenchmarks 0 ${setup} "$@"
  RunBenchmarks ${setup} "$@"
}

#@
#@ TimedBuildAndRunBenchmarks <setup> [ref|train] <benchmark>*
#@
#@   Builds and run all benchmarks according to the setup, using
#@   and records the time spent at each task..
#@   Results are saved in {execname}.compile_time and
#@   {execname}.run_time for each benchmark executable
#@   Note that the VERIFY variable effects the timing!
TimedBuildAndRunBenchmarks() {
  setup=$1
  shift
  BuildBenchmarks 1 ${setup} "$@"
  RunTimedBenchmarks ${setup} "$@"
}

#@
#@ ArchivedRunOnArmHW <user> <ip> <setup> [usual var-args for RunBenchmarks]
#@
#@   Copies ARM binaries built from a local QEMU run of spec to a remote
#@   ARM machine, and runs the tests without QEMU.
#@
#@   Note: <setup> should be the QEMU setup (e.g., SetupPnaclArmOpt)
#@   Note: As with the other modes in this script, this script should be
#@   run from the directory of the script (not the native_client directory).
#@
ArchivedRunOnArmHW() {
  local USER=$1
  local ARM_MACHINE_IP=$2
  # Assume binaries built w/ this setup (e.g. SetupPnaclArmOpt).
  # We will use this to determine a setup that doesn't use Qemu.
  local SPEC_SETUP=$3
  local SPEC_SETUP_HW=${SPEC_SETUP}HW

  # The rest of args should be the usual var-args for RunBenchmarks.
  shift 3
  local BENCH_LIST=$(GetBenchmarkList "$@")

  local TAR_TO=/tmp
  local SEL_ARCHIVE_ZIPPED=arm_sel_ldr.tar.gz
  local TESTS_ARCHIVE=arm_spec.tar
  local TESTS_ARCHIVE_ZIPPED=arm_spec.tar.gz
  local REMOTE_BUILD_DIR=/build/nacl_build_spec

  # Switch to native_client directory (from tests/spec2k) so that
  # when we extract, the builder will have a more natural directory layout.
  # Unfortunately, this means we can't reuse GetSelLdr()...
  local TAR_FROM="../.."
  tar -C ${TAR_FROM} -czvf ${TAR_TO}/${SEL_ARCHIVE_ZIPPED} \
    scons-out/opt-${SCONS_BUILD_PLATFORM}-arm/staging/sel_ldr
  # Carefully tar only the parts of the spec harness that we need.
  (cd ${TAR_FROM} ;
    find tests/spec2k -type f -maxdepth 1 -print |
    xargs tar --no-recursion -cvf ${TAR_TO}/${TESTS_ARCHIVE})
  tar -C ${TAR_FROM} -rvf ${TAR_TO}/${TESTS_ARCHIVE} tests/spec2k/bin
  for i in ${BENCH_LIST} ; do
    tar -C ${TAR_FROM} -rvf ${TAR_TO}/${TESTS_ARCHIVE} tests/spec2k/$i
  done
  gzip -f ${TAR_TO}/${TESTS_ARCHIVE}

  # Clean remote directory and copy over.
  echo "==== Nuking remote build dir: ${REMOTE_BUILD_DIR}"
  local CLEAN_CMD="(rm -rf ${REMOTE_BUILD_DIR} && \
    mkdir -p ${REMOTE_BUILD_DIR})"
  ssh ${USER}@${ARM_MACHINE_IP} ${CLEAN_CMD}
  echo "==== Copying files to: ${REMOTE_BUILD_DIR}"
  scp ${TAR_TO}/${SEL_ARCHIVE_ZIPPED} \
      ${TAR_TO}/${TESTS_ARCHIVE_ZIPPED} \
      ${USER}@${ARM_MACHINE_IP}:${REMOTE_BUILD_DIR}/.
  rm ${TAR_TO}/${SEL_ARCHIVE_ZIPPED} ${TAR_TO}/${TESTS_ARCHIVE_ZIPPED}

  # Now try to remotely run.
  echo "Extracting to ${REMOTE_BUILD_DIR} and running"
  local EXTRACT_RUN_CMD="(cd ${REMOTE_BUILD_DIR}; \
    tar -xzvf ${SEL_ARCHIVE_ZIPPED} && \
    tar -xzvf ${TESTS_ARCHIVE_ZIPPED}; \
    cd tests/spec2k; \
    ./run_all.sh RunTimedBenchmarks ${SPEC_SETUP_HW} $@)"
  ssh ${USER}@${ARM_MACHINE_IP} ${EXTRACT_RUN_CMD}
}


#@
#@ PopulateFromSpecHarness <path> <benchmark>*
#@
#@   populate a few essential directories (src, date) from
#@   the given spec2k harness
PopulateFromSpecHarness() {
  harness=$1
  shift
  cp -r ${harness}/bin .
  local list=$(GetBenchmarkList "$@")
  echo ${list}
  for i in ${list} ; do
    SubBanner "Populating: $i"
    # fix the dir with the same name inside spec harness
    src=$(find -H ${harness} -name $i)
    # copy relevant dirs over
    echo "COPY"
    rm -rf src/ data/
    cp -r ${src}/data ${src}/src $i
    # patch if necessary
    if [[ -e $i/diff ]] ; then
      echo "PATCH"
      patch -d $i --verbose -p0 < $i/diff
    fi

    echo "COMPLETE"
  done
}

######################################################################
# Main
######################################################################

[ $# = 0 ] && set -- help  # Avoid reference to undefined $1.

if [ "$(type -t $1)" != "function" ]; then
  Usage
  echo "ERROR: unknown mode '$1'." >&2
  exit 1
fi

"$@"
