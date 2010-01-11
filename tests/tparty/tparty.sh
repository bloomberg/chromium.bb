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


set -o nounset
set -o errexit

readonly OS_NAME=$(uname -s)

# windows: if your firefox is installed in a different location,
# you will need to locally modify the line below
readonly FIREFOX_WINDOWS="/cygdrive/c/Program Files/Mozilla Firefox/firefox.exe"

readonly TESTS="../"

# helper function to launch Firefox across all OS
# TODO: needs to trap errors returned by Firefox
Firefox() {
  if which firefox >& /dev/null ; then
    firefox ${1} || echo &
  elif [ ${OS_NAME} = "Darwin" ]; then
    open -a Firefox ${1} || echo
  else
    "${FIREFOX_WINDOWS}" ${1} || echo
  fi
}

# optional argument to fix Cygwin DLLs on Windows
# (NOTE: if DLLs don't match between Native Client and Cygwin,
# autoconf/confiure fails during Xaos build)
Cygfix() {
  readonly NACL_SDK="../../src/third_party/nacl_sdk"
  readonly GNU_BINUTILS="../../src/third_party/gnu_binutils"
  if [ ${OS_NAME} = "CYGWIN_NT-5.1" ]; then
    echo "Okay to copy cygwin DLLs from /usr/bin to ../../../third_party?"
    OPTIONS="Yes No"
    select opt in ${OPTIONS}; do
      if [ "${opt}" = "Yes" ]; then
        echo "Copying over your Native Client Cygwin files..."
        set -x #echo on
        chmod +w ${NACL_SDK}/windows/sdk/nacl-sdk/bin/cygwin1.dll
        chmod +w ${GNU_BINUTILS}/files/cygwin1.dll
        chmod +w ${GNU_BINUTILS}/files/cygiconv-2.dll
        chmod +w ${GNU_BINUTILS}/files/cygintl-3.dll
        cp /usr/bin/cygwin1.dll ${NACL_SDK}/windows/sdk/nacl-sdk/bin/.
        cp /usr/bin/cygwin1.dll ${GNU_BINUTILS}/files/.
        cp /usr/bin/cygiconv-2.dll ${GNU_BINUTILS}/files/.
        cp /usr/bin/cygintl-3.dll ${GNU_BINUTILS}/files/.
      else
        echo "Leaving Cygwin DLLs untouched"
      fi
      exit 0
    done
  else
    echo "cygfix argument not supported on this operating system"
    exit -1
  fi
}

# Print instructions and exit
PrintInstructions() {
  echo
  echo "Usage:"
  echo "  ./tparty.sh [cygfix]"
  echo
  echo "Arguments:"
  echo "  cygfix    copy over Native Client's Cygwin DLLs"
  echo "            w/ files from Cygwin install"
  echo
  echo "  With no arguments, tparty.sh will run through"
  echo "  sequence of tests, some of which require manual"
  echo "  user interaction."
  echo
  exit -1
}

# run through tests, many will require some user interaction
# TODO: can selenium automate some of these tests?
# TODO: option to run on browsers besides Firefox
# TODO: summary report at end of test run
# TODO: error checking to catch failed downloads
echo "Running on ${OS_NAME}"
if [ "${#}" != "0" ]; then
  if [ "${1}" = "cygfix" ]; then
    Cygfix
  elif [ "${1}" != "" ]; then
    PrintInstructions
  fi
fi
echo "Performing tests..."
# Earth demo
cd ${TESTS}/earth
make clean nacl
make debug nacl run
make release nacl run ARGS="-m8"
# Life demo
cd ../life
make clean nacl
make release nacl run
# Voronoi demo
cd ../voronoi
make clean nacl
make debug nacl run
make release nacl run ARGS="-m8"
# Quake demo
cd ../quake
make allclean nacl
make download nacl
make release nacl run
Firefox http://localhost:5103/tests/quake/quake.html
# Xaos demo
cd ../xaos
./xaos_tool.sh all
Firefox http://localhost:5103/tests/xaos/xaos.html
# Lua demo
cd ../lua
# TODO: the "nacl" stuff should be implemented as USE_NACL=1
make allclean nacl
make download nacl
make release nacl
Firefox http://localhost:5103/tests/lua/lua.html
# AWK demo
cd ../awk
make allclean nacl
make download nacl
make release nacl run dbgldr \
ARGS="'BEGIN { print \"Test PASSED:\" } NR==1' <README.nacl"
# Darkroom demo
cd ../photo
rm -f ./*.nexe
./photo_tool.sh
make clean nacl
make release nacl
Firefox http://localhost:5103/tests/photo/photo.html
# Anti-Grain Geometry demo
cd ../drawing
make download nacl
make clean nacl
make release nacl run
Firefox http://localhost:5103/tests/drawing/drawing.html

