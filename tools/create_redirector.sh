#!/bin/bash
# Copyright 2009, Google Inc.
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

# Creates an invocation script to invoke a 64 bit tool in 32 bit mode.
#
# Types of invocation redirection:
# * nacl-xxx to nacl64-xxx -m32 (--32)
# * xxx to ../../bin/nacl64-xxx -m32 (--32)
# * for ld, ar, objdump, etc. only a symbolic link is created

usage="$0 [-h] [-t] -s script"
istarget="false"

while getopts "ts:" option; do
  case $option in
    h) echo $usage;;
    t) istarget="true";;
    s) script=$OPTARG;;
    *) echo $usage
       exit 1;;
  esac
done

if [ "x$script" = "x" ]; then
  echo $usage && exit 1
fi

progname=${script##*/}
tool=${progname##*-}
progname=${progname/nacl/nacl64}

progdir=""
if [ "$istarget" = "true" ]; then
  progdir="../../bin/"
  if [ "$tool" = "$progname" ]; then
    progname="nacl64-$progname"
  fi
fi

case $tool in
  as) opt32="\"--32\"";;
  ld|ar|nm|objcopy|objdump|ranlib|strip)
      ln -sfn "${progdir}${progname}" "${script}"
      exit 0;;
  c++|g++|gcc|gfortran) opt32="\"-m32\"";;
  *) echo "Error: unknown tool: $tool"; exit 1;;
esac

cat >"${script}" <<ENDSCRIPT
#!/bin/bash

exec \${0%/*}/${progdir}${progname} "\$@" ${opt32}
ENDSCRIPT

chmod 755 "${script}"
