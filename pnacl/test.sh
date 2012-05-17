#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#@                   PNaCl Test Helper
#@-------------------------------------------------------------------
#@
#@ SCons Test Usage:
#@
#@   test.sh test-<arch>-<mode> [extra_arguments_to_scons]
#@
#@      Runs the SCons tests with selected arch and mode.
#@      Valid arches:
#@          x86-32
#@          x86-64
#@          arm
#@      Available modes:
#@          newlib (same as omitting mode)
#@          pic
#@          sbtc
#@          glibc
#@          browser
#@          browser-glibc
#@
#@      For example: test.sh test-x86-32-glibc
#@
#@ The env variables: PNACL_CONCURRENCY, PNACL_BUILDBOT, PNACL_DEBUG
#@ control behavior of this script
#@
#@

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

# The script is located in "pnacl/".
# Set pwd to native_client/
cd "$(dirname "$0")"/..
if [[ $(basename "$(pwd)") != "native_client" ]] ; then
  echo "ERROR: cannot find native_client/ directory"
  exit -1
fi
readonly NACL_ROOT="$(pwd)"

readonly DRYRUN=${DRYRUN:-false}

# This is only used by Spec2K test scripts.
readonly PNACL_LIBMODE=${LIBMODE:-newlib}
export PNACL_LIBMODE

source pnacl/scripts/common-tools.sh
SetScriptPath "${NACL_ROOT}/pnacl/test.sh"
SetLogDirectory "${NACL_ROOT}/toolchain/test-log"

# For different levels of make parallelism change this in your env
readonly PNACL_CONCURRENCY=${PNACL_CONCURRENCY:-8}

readonly OTHER_TEST_SCRIPT="${NACL_ROOT}/buildbot/buildbot_pnacl.sh"

######################################################################
######################################################################
#
#   < TESTING >
#
######################################################################
######################################################################

# TODO(robertm): figure out what to do about concurrency in debug mode.
# Perhaps it is fine just tweaking that via PNACL_CONCURRENCY.
if ${PNACL_DEBUG} || ${PNACL_BUILDBOT}; then
  readonly SCONS_ARGS=(MODE=nacl,opt-host
                       bitcode=1
                       --verbose
                       -j${PNACL_CONCURRENCY})
else
  readonly SCONS_ARGS=(MODE=nacl,opt-host
                       bitcode=1
                       naclsdk_validate=0
                       sysinfo=0
                       -j${PNACL_CONCURRENCY})
fi

#@ show-tests            - see what tests can be run
show-tests() {
  StepBanner "SHOWING TESTS"
  cat $(find tests -name nacl.scons) | grep -o 'run_[A-Za-z_-]*' | sort | uniq
}

Run() {
  if ${DRYRUN}; then
    echo "$@"
  else
    "$@"
  fi
}

RunScons() {
  local arch="$1"
  shift 1
  Run ./scons "${SCONS_ARGS[@]}" platform=${arch} "$@"
}

# Returns true if the arguments specify a test
# or target name to SCons.
has-target-name() {
  while [ $# -gt 0 ]; do
    # Skip arguments of the form -foo, --foo, or foo=bar.
    if [[ "$1" =~ ^-.* ]] || [[ "$1" =~ = ]]; then
      shift 1
      continue
    fi
    return 0
  done
  return 1
}

scons-clean () {
  local arch=$1
  local mode=$2
  local frontend=clang
  # Clear both the pexe and the nonpexe scons-out directory, since we run
  # a mix of both tests.
  if [ "${mode}" == "newlib" ] ; then
    Run rm -rf scons-out/nacl-${arch}-pnacl-${frontend}
    Run rm -rf scons-out/nacl-${arch}-pnacl-pexe-${frontend}
  else
    Run rm -rf scons-out/nacl-${arch}-pnacl-${mode}-${frontend}
    Run rm -rf scons-out/nacl-${arch}-pnacl-${mode}-pexe-${frontend}
  fi
}

build-sbtc-prerequisites() {
  local arch=$1
  # Sandboxed translators currently only require irt_core since they do not
  # use PPAPI.
  RunScons ${arch} sel_ldr sel_universal irt_core
}

# TODO(pdox):
# "mode" is presently a combination of multiple bits:
# Newlib vs. GLibC, sandboxed vs unsandboxed, PIC vs non-PIC
# These options should be isolated and kept separate.
get-mode-flags() {
  local mode="$1"
  local modeflags=""
  case ${mode} in
  newlib)
    ;;
  sbtc)
    modeflags="use_sandboxed_translator=1"
    ;;
  pic)
    modeflags="nacl_pic=1"
    ;;
  glibc)
    # Disable pexe mode with glibc for now. This will result in running the
    # nonpexe tests twice but there aren't too many of them.
    modeflags="--nacl_glibc pnacl_generate_pexe=0"
    ;;
  sbtc-glibc)
    # Note "sbtc-glibc" just happens to be the order in which the modes
    # are slapped onto the scons-out name. E.g.,
    # nacl-x86-32-pnacl-sbtc-glibc-clang
    # This affects the scons-clean() function.
    modeflags="--nacl_glibc use_sandboxed_translator=1 pnacl_generate_pexe=0"
    ;;
  *)
    echo "Unknown mode" 1>&2
    exit -1
  esac
  echo ${modeflags}
}

#+ Run scons test under a certain configuration
#+ scons-tests <arch> <mode={newlib,etc.}> [optional list of test names]
#+ If no optional tests are listed, we will build all the tests then
#+ run the "smoke_tests" test suite.
scons-tests () {
  local arch="$1"
  local mode="$2"
  shift 2
  scons-clean ${arch} ${mode}

  if [ ${mode} == "sbtc" ]; then
    build-sbtc-prerequisites "${arch}"
  fi

  local modeflags=$(get-mode-flags ${mode})
  if has-target-name "$@" ; then
    # By default this uses pexe mode (except where not supported) but this
    # can be overridden
    RunScons ${arch} ${modeflags} "$@"
  else
    # For now, mostly just duplicate mode-buildbot-x86 in buildbot_pnacl.sh
    # (but don't bother separating build/run for now) until we
    # converge on exactly what we want
    RunScons ${arch} ${modeflags} "$@" smoke_tests
    # nonpexe tests
    RunScons ${arch} ${modeflags} pnacl_generate_pexe=0 "$@" nonpexe_tests
  fi
}

browser-tests() {
  local arch="$1"
  local extra="$2"
  # This calls out to the other buildbot test script. We should have
  # buildbot_toolchain_arm_untrusted.sh use the same tests directly.
  # TODO(jvoung): remove these when unified.
  Run ${OTHER_TEST_SCRIPT} browser-tests \
    "${arch}" \
    "--mode=opt-host,nacl,nacl_irt_test ${extra} -j${PNACL_CONCURRENCY}"
}

test-arm()        { scons-tests arm newlib "$@" ; }
test-x86-32()     { scons-tests x86-32 newlib "$@" ; }
test-x86-64()     { scons-tests x86-64 newlib "$@" ; }

test-arm-newlib()    { scons-tests arm newlib "$@" ; }
test-x86-32-newlib() { scons-tests x86-32 newlib "$@" ; }
test-x86-64-newlib() { scons-tests x86-64 newlib "$@" ; }

# ARM PIC is tested independently because there is no GlibC for ARM yet.
# BUG= http://code.google.com/p/nativeclient/issues/detail?id=1081
test-arm-pic()    { scons-tests arm pic "$@" ; }

test-arm-sbtc()    { scons-tests arm sbtc "$@" ; }
test-x86-32-sbtc() { scons-tests x86-32 sbtc "$@" ; }
test-x86-64-sbtc() { scons-tests x86-64 sbtc "$@" ; }


test-arm-glibc()    { scons-tests arm glibc "$@" ; }
test-x86-32-glibc() { scons-tests x86-32 glibc "$@" ; }
test-x86-64-glibc() { scons-tests x86-64 glibc "$@" ; }

test-arm-sbtc-glibc()    { scons-tests arm sbtc-glibc "$@" ; }
test-x86-32-sbtc-glibc() { scons-tests x86-32 sbtc-glibc "$@" ; }
test-x86-64-sbtc-glibc() { scons-tests x86-64 sbtc-glibc "$@" ; }

test-arm-browser()    { browser-tests "arm" "" ; }
test-x86-32-browser() { browser-tests "x86-32" "" ; }
test-x86-64-browser() { browser-tests "x86-64" "" ; }

test-arm-browser-glibc() { browser-tests "arm" "--nacl_glibc" ; }
test-x86-32-browser-glibc() { browser-tests "x86-32" "--nacl_glibc" ; }
test-x86-64-browser-glibc() { browser-tests "x86-64" "--nacl_glibc" ; }

#@
#@ test-all  - Run arm, x86-32, and x86-64 tests. (all should pass)
test-all() {
  if [ $# -ne 0 ]; then
    echo "test-all does not take any arguments"
    exit -1
  fi

  FAIL_FAST=true ${OTHER_TEST_SCRIPT} mode-test-all ${PNACL_CONCURRENCY}
}

#@
#@ test-spec <official-spec-dir> <setup> [ref|train] [<benchmarks>]*
#@                       - run spec tests
test-spec() {
  if [[ $# -lt 2 ]]; then
    echo "not enough arguments for test-spec"
    exit 1
  fi;
  official=$(GetAbsolutePath $1)
  setup=$2
  shift 2
  spushd tests/spec2k
  ./run_all.sh BuildPrerequisitesSetupBased ${setup}
  ./run_all.sh CleanBenchmarks "$@"
  ./run_all.sh PopulateFromSpecHarness ${official} "$@"
  ./run_all.sh BuildAndRunBenchmarks ${setup} "$@"
  spopd
}

#+ CollectTimingInfo <directory> <timing_result_file> <tagtype...>
#+  CD's into the directory in a subshell and collects all the
#+  relevant timed run info
#+  tagtype just gets printed out.
CollectTimingInfo() {
  wd=$1
  result_file=$2
  setup=$3
  (cd ${wd};
   mkdir -p $(dirname ${result_file})
   echo "##################################################" >>${result_file}
   date +"# Completed at %F %H:%M:%S %A ${result_file}" >> ${result_file}
   echo "# " ${wd}
   echo "#" $(uname -a) >> ${result_file}
   echo "# SETUP: ${setup}" >>${result_file}
   echo "##################################################" >>${result_file}
   echo "# COMPILE " >> ${result_file}
   for ff in $(find . -name "*.compile_time"); do
     cat ${ff} >> ${result_file}
   done
   echo "# RUN " >> ${result_file}
   for ff in $(find . -name "*.run_time"); do
     cat ${ff} >> ${result_file}
   done
   cat ${result_file}
  )
}

#@
#@ timed-test-spec <result-file> <official-spec-dir> <setup> ... - run spec and
#@  measure time / size data. Data is emitted to stdout, but also collected
#@  in <result-file>. <result-file> is not cleared across runs (but temp files
#@  are cleared on each run).
#@  Note that the VERIFY variable effects the timing!
timed-test-spec() {
  if ${BUILD_PLATFORM_MAC} ; then
    echo "Timed-test-spec is not currently supported on MacOS"
    exit -1
  fi
  if [ "$#" -lt "3" ]; then
    echo "timed-test-spec {result-file} {spec2krefdir} {setupfunc}" \
         "[ref|train] [benchmark]*"
    exit 1
  fi
  result_file=$1
  official=$(GetAbsolutePath $2)
  setup=$3
  shift 3
  spushd tests/spec2k
  ./run_all.sh BuildPrerequisitesSetupBased ${setup}
  ./run_all.sh CleanBenchmarks "$@"
  ./run_all.sh PopulateFromSpecHarness ${official} "$@"
  ./run_all.sh TimedBuildAndRunBenchmarks ${setup} "$@"
  CollectTimingInfo $(pwd) ${result_file} ${setup}
  spopd
}

#@ help                  - Usage information.
help() {
  Usage
}

#@ help-full             - Usage information including internal functions.
help-full() {
  Usage2
}

######################################################################
######################################################################
#
#                               < MAIN >
#
######################################################################
######################################################################

[ $# = 0 ] && set -- help  # Avoid reference to undefined $1.
if [ "$(type -t $1)" != "function" ]; then
  #Usage
  echo "ERROR: unknown function '$1'." >&2
  echo "For help, try:"
  echo "    $0 help"
  exit 1
fi

"$@"
