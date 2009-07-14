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


#@ vim_tool.sh
#@
#@ usage:  vim_tool.sh <mode>
#@
#@ This script bootstraps a nacl module for the vim text editor off the web.
#@
#@ This works on linux only, assumes you have web access
#@ and that you have the typical linux tools installed
#@
set -o nounset
set -o errexit

readonly SCRIPT_DIR_REL=$(dirname $0)
readonly SCRIPT_DIR=$(cd $SCRIPT_DIR_REL && pwd)
echo "Building from $SCRIPT_DIR"

readonly STAGING_DIR=${STAGING_DIR:-$SCRIPT_DIR}
readonly LIB_DIR=${LIB_DIR:-$SCRIPT_DIR/../../scons-out/nacl-x86-32/lib}
readonly DIR_TMP=${DIR_TMP:-/tmp/nacl}
readonly DIR_INSTALL=${DIR_INSTALL:-$DIR_TMP}

echo "Building in $DIR_TMP"
echo "Installing to $DIR_INSTALL"

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
${SCRIPT_DIR}/../../src/third_party/nacl_sdk/$OS_SUBDIR/sdk/nacl-sdk/

readonly URL_VIM=ftp://ftp.vim.org/pub/vim/unix/vim-7.2.tar.bz2

readonly PATCH_VIM=${SCRIPT_DIR}/vim-7.2.patch


readonly NACL_DIM_W=640
readonly NACL_DIM_H=480
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

# params (tmp, url, patch)
DownloadVIM() {
  Banner "downloading vim"
  cd $1
  Download $2 vim.bz2
  rm -rf vim72
  tar jxf vim.bz2
  Banner "untaring, patching and autoconfing vim"
  patch -p0 < $3
  cd vim72
}

# params (tmp, install)
BuildVIM() {
  Banner "configuring vim"
  cd $1
  export PATH="$2/bin:${PATH}"
  export CC=${NACLCC}
  export AR=${NACLAR}
  export RANLIB=${NACLRANLIB}
  export LDFLAGS="-static -L${LIB_DIR} -L${DIR_AV_LIB}"
  export CFLAGS=""
  export CFLAGS="$CFLAGS -DNACL_DIM_H=${NACL_DIM_H}"
  export CFLAGS="$CFLAGS -DNACL_DIM_W=${NACL_DIM_W}"
  export CFLAGS="$CFLAGS -I${DIR_AV_INCLUDE}"
  export CFLAGS="$CFLAGS -I${SCRIPT_DIR}/../../common/console"
  export CFLAGS="$CFLAGS -D_SC_PHYS_PAGES=100000"
  export CFLAGS="$CFLAGS -D_SC_PAGESIZE=1024"
  export LIBS="-lav -lsrpc -lpthread"
  export vim_cv_toupper_broken="set"
  export vim_cv_terminfo="set"
  export vim_cv_tty_group="set"
  export vim_cv_tty_mode="set"
  export vim_cv_getcwd_broken="set"
  export vim_cv_stat_ignores_slash="set"
  export vim_cv_memmove_handles_overlap="set"
  export vim_cv_bcopy_handles_overlap="set"
  export vim_cv_memcpy_handles_overlap="set"
  export ac_cv_sizeof_int=4
  rm -rf vim-build
  # sadly vim seem wants to be built in-place
  cp -r vim72 vim-build
  cd vim-build
  ../vim72/configure\
      --host=nacl\
      --without-local-dir\
      --disable-sysmouse\
      --disable-gpm\
      --disable-acl\
      --enable-gui=no\
      --with-tlib="termcap -lstdc++"

  Banner "building and installing vim"
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
#@ download_vim
#@
#@
if [ ${MODE} = 'download_vim' ] ; then
  mkdir -p ${DIR_TMP}
  DownloadVIM ${DIR_TMP}  ${URL_VIM}  ${PATCH_VIM}
  exit 0
fi

#@
#@ build_vim
#@
#@
if [ ${MODE} = 'build_vim' ] ; then
  BuildVIM  ${DIR_TMP} ${DIR_INSTALL}
  exit 0
fi

#@
#@ all
#@
#@
if [ ${MODE} = 'all' ] ; then
  mkdir -p ${DIR_TMP}
  if [ $OS_SUBDIR = "windows" ]; then
    pushd ${SCRIPT_DIR}/../tparty
    ./tparty.sh cygfix
    popd
  fi
  DownloadVIM ${DIR_TMP}  ${URL_VIM}  ${PATCH_VIM}
  BuildVIM  ${DIR_TMP} ${DIR_INSTALL}

  Banner "Copying relevant files into this directory"
  cp ${DIR_TMP}/vim-build/src/vim ${STAGING_DIR}/vim.nexe

  Banner "To view the demo"

  echo "either point your browser at"
  echo
  echo "http://localhost:5103/tests/vim/vim.html"
  echo
  echo "after running tools/httpd.py while in the native_client directory"
  echo "or run"
  echo
  echo "../../scons-out/opt-linux-x86-32/staging/sel_ldr ./vim.nexe"
  exit 0
fi


######################################################################
# Mode is not handled
######################################################################

echo "unknown mode: ${MODE}"
exit -1


