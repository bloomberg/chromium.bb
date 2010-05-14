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

declare -r usage="$0 [-h] [-t] -s script"
is_target="false"

while getopts "ts:" option; do
  case $option in
    h)
      echo "$usage" >&2
      exit 1
      ;;
    t)
      is_target="true"
      ;;
    s)
      script="$OPTARG"
      ;;
    *)
      echo "$usage" >&2
      exit 1
      ;;
  esac
done
declare -r is_target script

if [[ -z "$script" ]]; then
  echo "$usage" >&2
  exit 1
fi

program_name="${script##*/}"
declare -r pure_tool_name="${program_name##*-}"
declare -r program_name="${program_name/nacl/nacl64}"

program_dir=""
if [[ "$is_target" = "true" ]]; then
  program_dir="../../bin/"
  if [[ "$pure_tool_name" = "${program_name}" ]]; then
    program_name="nacl64-${program_name}"
  fi
fi
declare -r program_dir

case "$pure_tool_name" in
  as)
    opt32="\"--32\""
    ;;
  ld)
    opt32="\"-melf_nacl\""
    ;;
  ar|nm|objcopy|objdump|ranlib|strip)
    ln -sfn "${program_dir}${program_name}" "${script}"
    exit 0
    ;;
  c++|g++|gcc|gfortran)
    opt32="\"-m32\""
    ;;
  *)
    echo "Error: unknown tool: $pure_tool_name" >&2
    exit 1
    ;;
esac
declare -r opt32

if [[ "$opt32" = "-m32" ]]; then
  cat >"${script}" <<ENDSCRIPT
#!/bin/bash

# Put -V argument(s) prior to -m32. Otherwise the GCC driver does not accept it.
program_name="\${0%/*}/${program_dir}${program_name}"
if [[ "\$1" = "-V" ]]; then
  shift
  OPTV="\$1"
  shift
  exec "\$program_name" -V "\$OPTV" -m32 "\$@"
elif [[ "\${1:0:2}" = "-V" ]]; then
  OPTV="\$1"
  shift
  exec "\$program_name" "\$OPTV" -m32 "\$@"
else
  exec "\$program_name" -m32 "\$@"
fi
ENDSCRIPT
else
  cat >"${script}" <<ENDSCRIPT
#!/bin/bash

exec \${0%/*}/${program_dir}${program_name} ${opt32} "\$@"
ENDSCRIPT
fi

chmod 755 "${script}"
