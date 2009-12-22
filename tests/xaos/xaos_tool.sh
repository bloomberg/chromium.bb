#!/bin/bash
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#@ xaos_tool.sh
#@
#@ usage:  xaos_tool.sh <mode>
#@
#@ this script bootstraps a nacl module for the xaos fractal rendering
#@ engine off the web including the gsl package it depends on.
#@
#@ This works on linux only, assumes you have web access
#@ and that you have the typical linux tools installed
#@
set -o nounset
set -o errexit

readonly SAVE_PWD=$(pwd)

readonly DIR_TMP=${DIR_TMP:-/tmp/nacl}
readonly DIR_INSTALL=${DIR_INSTALL:-/tmp/nacl}
readonly OS_NAME=$(uname -s)
if [ $OS_NAME = "Darwin" ]; then
  readonly OS_SUBDIR="mac"
elif [ $OS_NAME = "Linux" ]; then
  readonly OS_SUBDIR="linux"
else
  readonly OS_SUBDIR="windows"
fi

#readonly NACL_SDK_BASE=/usr/local/nacl-sdk/
readonly NACL_SDK_BASE=\
${SAVE_PWD}/../../src/third_party/nacl_sdk/$OS_SUBDIR/sdk/nacl-sdk/

readonly URL_XAOS=http://downloads.sourceforge.net/xaos/XaoS-3.4.tar.gz
readonly URL_GSL=http://ftp.gnu.org/pub/gnu/gsl/gsl-1.9.tar.gz

readonly PATCH_GSL=${SAVE_PWD}/gsl-1.9.patch
readonly PATCH_XAOS=${SAVE_PWD}/XaoS-3.4.patch


# TODO: the dimensions need a little bit of more work in the xaos patch
readonly NACL_DIM_W=800
readonly NACL_DIM_H=600
readonly NACLCC=${NACL_SDK_BASE}/bin/nacl-gcc
readonly NACLAR=${NACL_SDK_BASE}/bin/nacl-ar
readonly NACLRANLIB=${NACL_SDK_BASE}/bin/nacl-ranlib
readonly DIR_AV_LIB=${NACL_SDK_BASE}/nacl/lib
readonly DIR_AV_INCLUDE=${NACL_SDK_BASE}/nacl/include


######################################################################
# Helper functions
######################################################################

Banner() {
  echo "######################################################################"
  echo $*
  echo "######################################################################"
}


Usage() {
  egrep "^#@" $0 | cut --bytes=3-
}


ReadKey() {
  read
}


Download() {
  if which wget ; then
    wget $1 -O $2
  elif which curl ; then
    curl --location --url $1 -o $2
  else
     Banner "Problem encountered"
     echo "Please install curl or wget and rerun this script"
     echo "or manually download $1 to $2"
     echo
     echo "press any key when done"
     ReadKey
  fi

  if [ ! -s $2 ] ; then
    echo "ERROR: could not find $2"
    exit -1
  fi
}


DownloadGsl() {
  Banner "downloading gsl"
  cd $1
  Download $2 gsl.tgz
  Banner "untaring and patching gsl"
  rm -rf gsl-1.9
  tar zxf gsl.tgz
  patch -p0 < $3
}


# params (tmp, install)
BuildGsl() {
  Banner "configuring gsl"
  cd $1
  export CC=${NACLCC}
  export AR=${NACLAR}
  export RANLIB=${NACLRANLIB}
  export LIBS="-lm -lsrpc -lpthread"
  rm -rf gsl-build
  mkdir gsl-build
  cd gsl-build
  ../gsl-1.9/configure\
    --host=nacl\
    --disable-shared\
    --prefix=$2
  Banner "building and installing gsl"
  make
  make install
}

# params (tmp, url, patch)
DownloadXaos() {
  Banner "downloading xaos"
  cd $1
  Download $2 xaos.tgz
  rm -rf XaoS-3.4
  tar zxf xaos.tgz
  Banner "untaring, patching and autoconfing xaos"
  patch -p0 < $3
  cd XaoS-3.4
  autoconf
}

# params (tmp, install)
BuildXaos() {
  Banner "configuring xaos"
  cd $1
  export PATH="$2/bin:${PATH}"
  export CC=${NACLCC}
  export AR=${NACLAR}
  export RANLIB=${NACLRANLIB}
  export LDFLAGS="-static"
  export CFLAGS=""
  export CFLAGS="${CFLAGS} -DNACL_DIM_H=${NACL_DIM_H}"
  export CFLAGS="${CFLAGS} -DNACL_DIM_W=${NACL_DIM_W}"
  export CFLAGS="${CFLAGS} -I${DIR_AV_INCLUDE}"
  export LIBS="-L${DIR_AV_LIB} -lav -lsrpc -lpthread"
  rm -rf xaos-build
  # sadly xaos seem wants to be built in-place
  cp -r  XaoS-3.4 xaos-build
  cd xaos-build
  ../XaoS-3.4/configure\
      --with-png=no\
      --host=nacl\
      --with-x11-driver=no

  Banner "building and installing xaos"
  make
}

######################################################################
# Parse the mode and extract the needed arguments
######################################################################
if [ $# -eq 0  ] ; then
  echo "no mode specified"
  Usage
  exit -1
fi

readonly MODE=$1
shift

#@
#@ help
#@
#@   Prints help for all modes.
if [ ${MODE} = 'help' ] ; then
  Usage
  exit 0
fi

#@
#@ download_gsl
#@
#@
if [ ${MODE} = 'download_gsl' ] ; then
  mkdir -p ${DIR_TMP}
  DownloadGsl ${DIR_TMP}  ${URL_GSL}  ${PATCH_GSL}
  exit 0
fi

#@
#@ build_gsl
#@
#@
if [ ${MODE} = 'build_gsl' ] ; then
  BuildGsl  ${DIR_TMP} ${DIR_INSTALL}
  exit 0
fi

#@
#@ download_xaos
#@
#@
if [ ${MODE} = 'download_xaos' ] ; then
  mkdir -p ${DIR_TMP}
  DownloadXaos ${DIR_TMP}  ${URL_XAOS}  ${PATCH_XAOS}
  exit 0
fi

#@
#@ build_xaos
#@
#@
if [ ${MODE} = 'build_xaos' ] ; then
  BuildXaos  ${DIR_TMP} ${DIR_INSTALL}
  exit 0
fi

#@
#@ all
#@
#@
if [ ${MODE} = 'all' ] ; then
  mkdir -p ${DIR_TMP}
  DownloadGsl ${DIR_TMP}  ${URL_GSL}  ${PATCH_GSL}
  BuildGsl  ${DIR_TMP} ${DIR_INSTALL}
  DownloadXaos ${DIR_TMP}  ${URL_XAOS}  ${PATCH_XAOS}
  BuildXaos  ${DIR_TMP} ${DIR_INSTALL}

  Banner "Copying relevant files into this directory"
  cp ${DIR_TMP}/xaos-build/bin/xaos ${SAVE_PWD}/xaos.nexe
  cp ${DIR_TMP}/xaos-build/help/xaos.hlp ${SAVE_PWD}/

  Banner "To view the demo"

  echo "either point your browser at"
  echo
  echo "http://localhost:5103/tests/xaos/xaos.html"
  echo
  echo "after running tools/httpd.py while in the native_client directory"
  echo "or run"
  echo
  echo "../../scons-out/opt-linux-x86-32/staging/sel_ldr ./xaos.nexe"
  exit 0
fi


######################################################################
# Mode is not handled
######################################################################

echo "unknown mode: ${MODE}"
exit -1


