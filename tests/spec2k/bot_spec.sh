#!/bin/bash

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

set -o nounset
set -o errexit

# Paths relative to native_client
SPEC_BASE="tests/spec2k"
TC_SCRIPT="./tools/llvm/utman.sh"

#######################################################################
# Helpers
#######################################################################

StepBanner() {
  echo "**********************************************************************"
  echo "**********************************************************************"
  echo " $@"
  echo "**********************************************************************"
  echo "**********************************************************************"
}

NoteBuildStep() {
  echo "@@@BUILD_STEP $1@@@"
}

Annotate() {
  local label=$1
  shift
  StepBanner "Running: $@"
  NoteBuildStep ${label}
}

AnnotatedRun() {
  Annotate "$@"
  local label=$1
  shift
  "$@"
}

AnnotatedMayFailRun() {
  Annotate "$@"
  local label=$1
  shift

  set +o errexit
  "$@"
  local errcode=$?
  if [[ ${errcode} -ne 0 ]]; then
      echo "@@@BUILD FAILED@@@"
  fi
  set -o errexit

  return ${errcode}
}

PrepareSpec() {
  local spec_dir=$1
  pushd ${SPEC_BASE}
  AnnotatedRun "Clean" "./run_all.sh" CleanBenchmarks
  AnnotatedRun "Populate" "./run_all.sh" PopulateFromSpecHarness "${spec_dir}"
  popd
}

BuildAndRunSetups() {
  local setups=$1
  local did_fail=0
  pushd ${SPEC_BASE}
  for setup in ${setups}; do
    if ! AnnotatedMayFailRun "${setup}" \
      "./run_all.sh" TimedBuildAndRunBenchmarks ${setup}; then
      did_fail=1
    fi
  done
  popd
  return ${did_fail}
}

PrepareTrusted() {
  local arch=$1
  AnnotatedRun "scons-${arch}" \
    "./scons" --mode=opt-linux sdl=none platform=${arch} sel_ldr sel_universal
}

#######################################################################
# Steps
#######################################################################

#@
#@ test-spec-bot -- determines what runs on the NaCl spec build bots
#@   must run from native_client
test-spec-bot() {
  if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <bot_id> <spec_dir>"
    echo "   vs $@"
    exit -1
  fi

  if [[ $(basename $(pwd)) != "native_client" ]] ; then
    echo "ERROR: run this script from the native_client/ dir"
    exit -1
  fi

  local bot_id=$1
  local spec_dir=$2
  case ${bot_id} in
    1)
      # The ARM bot.
      PrepareTrusted arm
      PrepareSpec ${spec_dir}
      BuildAndRunSetups "SetupPnaclArmOpt"
      exit $?
      ;;
    2)
      # The X86 bot.
      PrepareTrusted x86-32
      PrepareTrusted x86-64
      PrepareSpec ${spec_dir}
      BuildAndRunSetups "SetupNaclX8664 SetupNaclX8664Opt
SetupPnaclX8632 SetupPnaclX8632Opt
SetupPnaclX8664 SetupPnaclX8664Opt"
      exit $?
      ;;
    3)
      # The sandboxed translator bot.
      PrepareTrusted x86-32
      PrepareTrusted x86-64
      PrepareSpec ${spec_dir}
      BuildAndRunSetups "SetupPnaclTranslatorX8632 SetupPnaclTranslatorX8632Opt
SetupPnaclTranslatorX8664 SetupPnaclTranslatorX8664Opt"
      exit $?
      ;;

    BadTest)
      # Bad bot ID for testing. See that one failure doesn't prevent others
      # from running, and check bad return codes get propagated.
      echo "Testing fake bot ID with forced failure"
      PrepareTrusted x86-64
      PrepareSpec ${spec_dir}
      if ! BuildAndRunSetups "SetupBadBadBad SetupPnaclX8664Opt"; then
        echo "SUCCESS: See that build failed!"
        exit -1
      else
        exit 0
      fi
      ;;
    *)
      echo "$0 Given unknown bot-id -- ${bot_id}!"
      exit -1
  esac
}

######################################################################
# Main
######################################################################

test-spec-bot $@
