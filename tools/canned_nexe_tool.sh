#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#@ This script is used to update the archive of canned nexes
#@ Note, it does not recreate every nexe from scratch but will
#@ update those it can (currently: spec and translator nexes).
set -o nounset
set -o errexit

readonly UP_DOWN_LOAD_SCRIPT=buildbot/file_up_down_load.sh

readonly CANNED_DIR=CannedNexes

readonly SPEC_BASE="tests/spec2k"
readonly SPEC_HARNESS=${SPEC_HARNESS:-${HOME}/cpu2000-redhat64-ia32}/

readonly SPEC_SETUPS_ARM=SetupPnaclArmOpt
readonly SPEC_SETUPS_X8632=SetupPnaclX8632Opt
readonly SPEC_SETUPS_X8664=SetupPnaclX8664Opt

# for ./run_all.sh invocation
readonly MAKEOPTS=-j8
export MAKEOPTS

######################################################################
# Helpers
######################################################################

Banner() {
  echo "######################################################################"
  echo $*
  echo "######################################################################"
}

help() {
  egrep "^#@" $0 | cut --bytes=3-
}

DownloadCannedNexes() {
  local arch=$1
  local rev=$2
  Banner "Downloading rev: ${rev} arch: ${arch}"
  ${UP_DOWN_LOAD_SCRIPT} DownloadArchivedNexes \
      ${rev} "${arch}_giant" giant_nexe.tar.bz2
  # Untaring the tarball will generate "${CANNED_DIR}/" in the current directory
  rm -rf ${CANNED_DIR}
  tar jxf  giant_nexe.tar.bz2
}

UploadCannedNexes() {
  local arch=$1
  local rev=$2
  Banner "Uploading rev: ${rev} arch: ${arch}"
  rm giant_nexe.tar.bz2
  tar jcf  giant_nexe.tar.bz2 ${CANNED_DIR}

  ${UP_DOWN_LOAD_SCRIPT} UploadArchivedNexes \
      ${rev} "${arch}_giant" giant_nexe.tar.bz2
}

AddTranslatorNexes() {
  local arch=$1
  local sysarch=""
  if [[ ${arch} == "arm" ]] ; then
    sysarch=armv7
  elif [[ ${arch} == "x86-32" ]] ; then
    sysarch=i686
  elif [[ ${arch} == "x86-64" ]] ; then
    sysarch=x86_64
  else
    echo "Error: unknown arch ${arch}"
  fi

  Banner "Updating Translator Nexes arch: ${arch}"
  cp toolchain/pnacl_translator/${sysarch}/bin/*.nexe ${CANNED_DIR}
}

AddSpecNexes() {
  local arch=$1
  local setups=
  local fext=

  if [[ ${arch} == "arm" ]] ; then
    setups="${SPEC_SETUPS_ARM}"
    fext=arm
  elif [[ ${arch} == "x86-32" ]] ; then
    setups="${SPEC_SETUPS_X8632}"
    fext=x8632
  elif [[ ${arch} == "x86-64" ]] ; then
    setups="${SPEC_SETUPS_X8664}"
    fext=x8664
  else
    echo "Error: unknown arch ${arch}"
  fi

  Banner "Updating Spec Nexes arch: ${arch} setups: ${setups}"

  pushd ${SPEC_BASE}
  ./run_all.sh BuildPrerequisites "$@" bitcode
  ./run_all.sh CleanBenchmarks
  ./run_all.sh PopulateFromSpecHarness "${SPEC_HARNESS}"

  for setup in ${setups}; do
    ./run_all.sh BuildBenchmarks 0 ${setup}
  done
  popd

  cp tests/spec2k/*/*.${fext} ${CANNED_DIR}
  # we chop of the arch extension so that benchmark names
  # are the same across architectures
  rename 's/[.](arm|x8664|x8632)$//'  ${CANNED_DIR}/*.${fext}
}

Update() {
  local arch=$1
  local rev_in=$2
  local rev_out=$3
  DownloadCannedNexes ${arch} ${rev_in}
  AddTranslatorNexes ${arch}
  AddSpecNexes ${arch}
  UploadCannedNexes ${arch} ${rev_out}
}

######################################################################
# "main"
######################################################################

if [ "$(type -t $1)" != "function" ]; then
  echo "ERROR: unknown function '$1'." >&2
  echo "For help, try:"
  echo "    $0 help"
  exit 1
fi

"$@"
