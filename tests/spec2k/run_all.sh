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

readonly LIST_INT_C="164.gzip 175.vpr 176.gcc 181.mcf 186.crafty 197.parser \
253.perlbmk 254.gap 255.vortex 256.bzip2 300.twolf"

readonly LIST_FP_C="177.mesa 179.art 183.equake 188.ammp"

readonly LIST_INT_CPP="252.eon"

# TODO(robertm): make "252.eon" work
LIST="${LIST_FP_C} ${LIST_INT_C}"
#Pick one
#SCRIPT=./run.ref.sh
SCRIPT=./run.train.sh

# uncomment this to disable verification
# verification time will be part of  overall benchmarking time
# export VERIFY=no
export VERIFY=yes

# Pick a setup

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
  SEL_LDR=../../scons-out/opt-linux-x86-32/staging/sel_ldr
  if [[ ! -x ${SEL_LDR} ]] ; then
    echo "you have not build the sel_ldr yet"
    exit -1
  fi
  SEL_LDR=$(readlink -f ${SEL_LDR})
  PREFIX="${SEL_LDR} -d -f"
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
  SEL_LDR=../../scons-out/opt-linux-x86-64/staging/sel_ldr
  if [[ ! -x ${SEL_LDR} ]] ; then
    echo "you have not build the sel_ldr yet"
    exit -1
  fi
  SEL_LDR=$(readlink -f ${SEL_LDR})
  PREFIX="${SEL_LDR} -d -f"
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

######################################################################

SetupPnaclX8664Common() {
  SEL_LDR=../../scons-out/opt-linux-x86-64/staging/sel_ldr
  if [[ ! -x ${SEL_LDR} ]] ; then
    echo "you have not build the sel_ldr yet"
    exit -1
  fi
  SEL_LDR=$(readlink -f ${SEL_LDR})
  PREFIX="${SEL_LDR} -d -f"
}

#@
#@ SetupPnaclX8664
#@    use pnacl x86-64 compiler (no lto)
SetupPnaclX8664() {
  SetupPnaclX8664Common
  SUFFIX=pnacl.x8664
}

#@
#@ SetupPnaclX864Opt
#@    use pnacl x86-64 compiler (with lto)
SetupPnaclX8664Opt() {
  SetupPnaclX8664Common
  SUFFIX=pnacl.opt.x8664
}

SetupPnaclX8632Common() {
  SEL_LDR=../../scons-out/opt-linux-x86-32/staging/sel_ldr
  if [[ ! -x ${SEL_LDR} ]] ; then
    echo "you have not build the sel_ldr yet"
    exit -1
  fi
  SEL_LDR=$(readlink -f ${SEL_LDR})
  PREFIX="${SEL_LDR} -d -f"
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
#@ SetupGccArm
#@   use CS cross compiler
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

#@
#@ SetupPnaclArm
#@    use pnacl arm compiler (no lto)
SetupPnaclArmOpt() {
  SetupPnaclArmCommon
  SUFFIX=pnacl.opt.arm
}

#@
#@ SetupPnaclArmOpt
#@    use pnacl arm compiler (with lto)
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
#@ ------------------------------------------------------------
#@ Available Modes:
#@ ------------------------------------------------------------

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
#@   this is a deep clean and you have to rerun PoplateFromSpecHarness
CleanBenchmarks() {
  local list=$(GetBenchmarkList "$@")
  rm -rf bin/
  for i in ${list} ; do
    Banner "Cleaning: $i"
    cd $i
    make clean
    rm -rf src/ data/
    cd ..
  done
}

#@
#@ BuildBenchmarks <setup> <benchmark>*
#@
#@  Build all benchmarks according to the setup
BuildBenchmarks() {
  export PREFIX=
  "$1"
  shift
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
#@ RunBenchmarks <setup> <benchmark>*
#@
#@  Run all benchmarks according to the setup
RunBenchmarks() {
  export PREFIX=
  "$1"
  shift
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
#@ BuildAndRunBenchmarks <setup> <benchmark>*
#@
#@   Builds and run all benchmarks according to the setup
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
  cp -r ${harness}/bin .
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

#@
#@ Default mode is:  BuildAndRunBenchmarks SetupPnaclArm
if [[ $# = 0 ]] ; then
  BuildAndRunBenchmarks SetupPnaclArm
elif [ "$(type -t $1)" != "function" ]; then
  Usage
  echo "ERROR: unknown mode '$1'." >&2
  exit 1
fi

eval "$@"
