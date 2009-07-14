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

#@ photo_tool.sh
#@
#@ usage:  photo_tool.sh <mode>
#@
#@ this script bootstraps a nacl module for the photo demo
#@
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
readonly NACL_SDK_BASE=\
${SAVE_PWD}/../../src/third_party/nacl_sdk/$OS_SUBDIR/sdk/nacl-sdk/


readonly URL_JPEG=http://downloads.sourceforge.net/libjpeg/jpegsrc.v6b.tar.gz
readonly URL_CARPE_JS=http://carpe.ambiprospect.com/slider/scripts/slider.js
readonly URL_CARPE_CSS=http://carpe.ambiprospect.com/slider/styles/default.css

readonly PATCH_JPEG=${SAVE_PWD}/jpeg-6b.patch
readonly PATCH_CARPE=${SAVE_PWD}/carpe.patch

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


Dos2Unix() {
  if which dos2unix ; then
    dos2unix $1
  else
    mv $1 $1.$$.tmp
    tr -d '\015' < $1.$$.tmp > $1
    rm $1.$$.tmp
  fi
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


DownloadAndPatch() {
  Banner "downloading jpeg"
  mkdir -p $1
  cd $1
  Download ${URL_JPEG} jpeg.tgz
  Banner "Untaring and patching jpeg"
  rm -rf jpeg-6b
  tar zxf jpeg.tgz
  patch -p0 < ${PATCH_JPEG}
  Banner "configuring gsl"
  cd $1
  export CC=${NACLCC}
  export AR=${NACLAR}
  export RANLIB=${NACLRANLIB}
  export LIBS="-lm"
  rm -rf jpeg-build
  mkdir jpeg-build
  cd jpeg-build
  ../jpeg-6b/configure\
    --host=nacl\
    --disable-shared\
    --prefix=${URL_JPEG}
  make clean
  make
  cp libjpeg.a ${SAVE_PWD}
  cp ../jpeg-6b/jpeglib.h ${SAVE_PWD}
  cp ../jpeg-6b/jmorecfg.h ${SAVE_PWD}
  cp ../jpeg-6b/jerror.h ${SAVE_PWD}
  cp ../jpeg-6b/jconfig.doc ${SAVE_PWD}/jconfig.h
  cd ${SAVE_PWD}
  # remove libjpeg src, tgz after building library
  rm -rf jpeg-build
  rm -rf jpeg.tgz
  Banner "Downloading and patching Carpe Design Slider"
  Download ${URL_CARPE_JS} slider.js
  Download ${URL_CARPE_CSS} default.css
  # add a newline to the end of default.css so patch can be applied
  Dos2Unix default.css
  Dos2Unix slider.js
  echo >> default.css
  patch -p1 < ${PATCH_CARPE}
}


DownloadAndPatch ${DIR_TMP}
exit 0

