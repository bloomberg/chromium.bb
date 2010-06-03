#!/bin/bash

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

set -o nounset
set -o errexit

######################################################################
# CONFIGURATION
######################################################################
# TODO(robertm): make this configurable from the commandline

readonly FULL_LIST="164.gzip 175.vpr 176.gcc 181.mcf 186.crafty 197.parser \
252.eon 253.perlbmk 254.gap 255.vortex 256.bzip2 300.twolf"

# we skip eon for now
readonly LIST="164.gzip 175.vpr 176.gcc 181.mcf 186.crafty 197.parser \
253.perlbmk 254.gap 255.vortex 256.bzip2 300.twolf"

#Pick one
#SCRIPT=./run.ref.sh
SCRIPT=./run.train.sh

# uncomment this to disable verification
# verification time will be part of  overall benchmarking time
# export VERIFY=no
export VERIFY=yes

# Pick a setup
#readonly SETUP=SetupPnaclArm
#readonly SETUP=SetupGccX8632
#readonly SETUP=SetupNaclX8632
readonly SETUP=SetupPnaclArmOpt
######################################################################
# Helper
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

######################################################################
# Various Setups
######################################################################

SetupGccX8632() {
  PREFIX=
  SUFFIX=gcc.x8632
}


SetupNaclX8632() {
  SEL_LDR=../../scons-out/opt-linux-x86-32/staging/sel_ldr
  if [[ ! -x ${SEL_LDR} ]] ; then
    echo "you have not build the sel_ldr yet"
    exit -1
  fi
  SEL_LDR=$(readlink -f ${SEL_LDR})
  PREFIX="${SEL_LDR} -d -f"
  SUFFIX=nacl.x8632
}


SetupPnaclX8632() {
  SEL_LDR=../../scons-out/opt-linux-x86-32/staging/sel_ldr
  if [[ ! -x ${SEL_LDR} ]] ; then
    echo "you have not build the sel_ldr yet"
    exit -1
  fi
  SEL_LDR=$(readlink -f ${SEL_LDR})
  PREFIX="${SEL_LDR} -d -f"
  SUFFIX=pnacl.x8632
}


SetupGccArm() {
  PREFIX=$(readlink -f ../../toolchain/linux_arm-trusted/qemu_tool.sh run)
  SUFFIX=gcc.arm
}


SetupPnaclArmCommon() {
  QEMU=$(readlink -f ../../toolchain/linux_arm-trusted/qemu_tool.sh)
  SEL_LDR=../../scons-out/opt-linux-arm/staging/sel_ldr

  if [[ ! -x ${SEL_LDR} ]] ; then
    echo "you have not build the sel_ldr yet"
    exit -1
  fi
  SEL_LDR=$(readlink -f ${SEL_LDR})
  PREFIX="${QEMU} run ${SEL_LDR} -d -Q -f"
  SUFFIX=pnacl.arm
}


SetupPnaclArmOpt() {
  SetupPnaclArmCommon
  SUFFIX=pnacl.opt.arm
}


SetupPnaclArm() {
  SetupPnaclArmCommon
  SUFFIX=pnacl.arm
}


ConfigInfo() {
  Banner "Config Info"
  echo "script ${SCRIPT} "
  echo "suffix ${SUFFIX}"
  echo "verify ${VERIFY}"
  echo "prefix ${PREFIX}"

}

######################################################################
# Functions intended to be called
######################################################################

#@
#@ GetBenchmarkList
#@
#@   Show avilable benchmarks
GetBenchmarkList() {
  if [[ $# = 0 ]] ; then
      echo "${LIST[@]}"
  else
      echo "$@"
  fi
}

#@
#@ CleanBenchmarks
#@
#@
CleanBenchmarks() {
  local list=$(GetBenchmarkList "$@")

  for i in ${list} ; do
    Banner "Cleanig: $i"
    cd $i
    make clean
    rm -rf src/ data/
    cd ..
  done
}

#@
#@ BuildBenchmarks
#@
#@
BuildBenchmarks() {
  export PREFIX=
  eval ${SETUP}
  local list=$(GetBenchmarkList "$@")

  ConfigInfo
  for i in ${list} ; do
    Banner "Building: $i"
    cd $i

    make ${i#*.}.${SUFFIX}
    cd ..
done

}


#@
#@ RunBenchmarks
#@
#@
RunBenchmarks() {
  export PREFIX=
  eval ${SETUP}
  local list=$(GetBenchmarkList "$@")

  ConfigInfo
  for i in ${list} ; do
    Banner "Benchmarking: $i"
    cd $i
    size  ./${i#*.}.${SUFFIX}
    time ${SCRIPT} ./${i#*.}.${SUFFIX}
    cd ..
  done
}

#@
#@ BuildAndRunBenchmarks
#@
#@   builds and run benchmarks fro the current configuration
BuildAndRunBenchmarks() {
  BuildBenchmarks "$@"
  RunBenchmarks "$@"
}

#@
#@ PoplateFromSpecHarness <path>
#@
#@   populate a few essential directories (src, date) from
#@   the given spec2k harness
PoplateFromSpecHarness() {
  harness=$1
  echo ${LIST}
  for i in ${LIST} ; do
    Banner "Populating: $i"
    # fix the dir with the same name inside spec harness
    src=$(find $1 -name $i)
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

if [[ $# = 0 ]] ; then
  BuildAndRunBenchmarks
elif [ "$(type -t $1)" != "function" ]; then
  Usage
  echo "ERROR: unknown mode '$1'." >&2
  exit 1
fi

eval "$@"
