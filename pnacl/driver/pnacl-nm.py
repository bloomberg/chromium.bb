#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  pnacl/build.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

from driver_tools import RunWithLog
from driver_env import env

def main(argv):
  env.set('ARGS', *argv)
  return RunWithLog('${NM} --plugin=LLVMgold ${ARGS}', errexit=False)

def get_help(argv):
  return """
Usage: %s [option(s)] [file(s)]
 List symbols in [file(s)] (a.out by default).
 The options are:
  -a, --debug-syms       Display debugger-only symbols
  -A, --print-file-name  Print name of the input file before every symbol
  -B                     Same as --format=bsd
  -C, --demangle[=STYLE] Decode low-level symbol names into user-level names
                          The STYLE, if specified, can be `auto' (the default),
                          `gnu', `lucid', `arm', `hp', `edg', `gnu-v3', `java'
                          or `gnat'
      --no-demangle      Do not demangle low-level symbol names
  -D, --dynamic          Display dynamic symbols instead of normal symbols
      --defined-only     Display only defined symbols
  -e                     (ignored)
  -f, --format=FORMAT    Use the output format FORMAT.  FORMAT can be `bsd',
                           `sysv' or `posix'.  The default is `bsd'
  -g, --extern-only      Display only external symbols
  -l, --line-numbers     Use debugging information to find a filename and
                           line number for each symbol
  -n, --numeric-sort     Sort symbols numerically by address
  -o                     Same as -A
  -p, --no-sort          Do not sort the symbols
  -P, --portability      Same as --format=posix
  -r, --reverse-sort     Reverse the sense of the sort
  -S, --print-size       Print size of defined symbols
  -s, --print-armap      Include index for symbols from archive members
      --size-sort        Sort symbols by size
      --special-syms     Include special symbols in the output
      --synthetic        Display synthetic symbols as well
  -t, --radix=RADIX      Use RADIX for printing symbol values
      --target=BFDNAME   Specify the target object format as BFDNAME
  -u, --undefined-only   Display only undefined symbols
  -X 32_64               (ignored)
  @FILE                  Read options from FILE
  -h, --help             Display this information
  -V, --version          Display this program's version number
""" % env.getone('SCRIPT_NAME')
