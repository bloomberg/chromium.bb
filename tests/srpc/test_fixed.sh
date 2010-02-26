#!/bin/bash
#
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

#
# This test just statically exercises each of the types that can be passed.
# Further testing will fuzz the input values, the string lengths, etc.
set -o nounset
set -o errexit
trap 'echo "TEST FAILED" ; exit 1' HUP INT QUIT TERM ERR

readonly PROG="$(basename $0)"
readonly SCRIPTDIR="$(dirname $0)"
readonly SCONSOUT="${SCRIPTDIR}/../../scons-out"

usage () {
  cat 1>&2 <<EOF
usage: ${PROG} [mode platform]
Statically exercise each of the types that can be passed via SRPC.
The "mode platform" can be
   mode platform: e.g. opt-linux  x86-32
   mode-platform: e.g. opt-linux-x86-32
   nothing:  If there is only one possible directory under
             ${SCONSOUT}, use that directory as mode-platform
EOF
  exit 1
}

# Get the mode and platform to be tested.
mode=""
platform=""
mode_platform=""
dirlist=""
if [ "$#" -eq "2" ] ; then
  # E.g.: $0 opt-linux x86-32
  mode="${1}"
  platform="${2}"
elif [ "$#" -eq "1" ] ; then
  # E.g.: $0 opt-linux-x86-32
  mode_platform="${1}"
elif [ "$#" -eq "0" ] ; then
  # Get a default mode and platform, assuming there is only one built right
  # now.
  if [ "${OSTYPE:0:5}" = "linux" ] ; then
    host_os_name="linux"
  elif [ "${OSTYPE:0:6}" = "darwin" ] ; then
    host_os_name="mac"
  elif [ "${OSTYPE:0:6}" = "cygwin" ] ; then
    host_os_name="win"
  fi
  dirlist=$(ls -1 "${SCONSOUT}" \
    | grep -v '^[.]' \
    | grep -v '^nacl' \
    | grep "[-]${host_os_name}-")
  dircount=$(echo "${dirlist}" | wc -l | tr -d ' ')
  if [ "${dircount}" = "1" ] ; then
    mode_platform="${dirlist}"
  fi
fi

# If we know mode-platform, split into mode and platform.
if [ "X${mode_platform}" != "X" ] ; then
  mode=$(echo "${mode_platform}" | sed -e 's/-[^-]*-[^-]*$//')
  platform=$(echo "${mode_platform}" | sed -e 's/^[^-]*-[^-]*-//')
  if [ "X${mode}" = "X${mode_platform}" -o \
       "X${platform}" = "X${mode_platform}" ] ; then
    mode=""
    platform=""
  fi
fi

# Were we able to determine the mode and platform?
if [ "X${mode}" = "X" -o "X${platform}" = "X" ] ; then
  if [ "$#" -gt "0" ] ; then
    echo "${PROG}: Cannot determine the mode and platform from: $*" 1>&2
  else
    echo "${PROG}: Cannot guess mode-platform, contents of ${SCONSOUT}:" 1>&2
    echo "${dirlist}" 1>&2
  fi
  usage
fi
mode_platform="${mode}-${platform}"
echo "Testing mode='${mode}' and platform='${platform}'."

# Make sure all the test inputs exist.
readonly STAGING="${SCONSOUT}/${mode_platform}/staging"
readonly NACLSTAGING="${SCONSOUT}/nacl-${platform}/staging"
readonly SEL_UNIVERSAL="${STAGING}/sel_universal"
readonly TMP="/tmp/${PROG}.output"

if [ ! -d "${STAGING}" ] ; then
  echo "${PROG}: Platform staging directory does not exist: ${STAGING}" 1>&2
  exit 1
fi
if [ ! -d "${NACLSTAGING}" ] ; then
  echo "${PROG}: NaCl staging directory does not exist: ${NACLSTAGING}" 1>&2
  exit 1
fi
if [ ! -x "${SEL_UNIVERSAL}" ] ; then
  echo "${PROG}: sel_universal does not exist at ${SEL_UNIVERSAL}" 1>&2
  exit 1
fi


# Run the test.

Banner () {
  echo "# ============================================================"
  echo $*
  echo "# ============================================================"
}

Banner "# Running srpc_test"
${SEL_UNIVERSAL} -f "${NACLSTAGING}/srpc_test.nexe" \
  <"${SCRIPTDIR}/srpc_basic_test.stdin" >"${TMP}"
echo ""

Banner "# Checking srpc_test"
diff --ignore-space-change  "${SCRIPTDIR}/srpc_basic_test.stdout" "${TMP}"

echo "TEST PASSED"
