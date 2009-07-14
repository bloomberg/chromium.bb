#!/usr/bin/python
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


import getopt
import sys


"""Changes the OS ABI number of Native Client executables.

This module contains code for updating the OS ABI version number of
Native Client executables.  This is needed, for example, for changing
the bad executables checked into the service runtime test data
directory -- those executables were the result of hand editing of ELF
headers, since such bad executables cannot be created with our
toolchain.

"""

ELF_ABIVERSION_OFFSET = 8


def SetOsAbiVersion(elf_file_data, version):
  """Change the OS ABI number of a string, interpreted as an NaCl executable.

  Args:
    elf_file_data: the ELF NaCl executable (as a string)
    version: the desired version number (as an integer)

  Returns:
    Updated ELF NaCl executable.
  """
  return (elf_file_data[:ELF_ABIVERSION_OFFSET]
          + chr(version) + elf_file_data[ELF_ABIVERSION_OFFSET + 1:])
#enddef


def ModifyFileOsAbiVersion(filename, version):
  """Changes the OS ABI number of the named file.

  No sanity checking is done on the file's contents to verify that it
  indeed is an ELF NaCl executable.

  Args:
    filename: the file to be modified.
    version: the new OS ABI version number.

  Returns:  nothing.

  Throws: IOError if file does not exist or cannot be written.

  """
  file_data = open(filename, 'rb').read()
  open(filename, 'wb').write(SetOsAbiVersion(file_data, version))
# enddef


def ReadFileOsAbiVersion(filename):
  """Read the named ELF NaCl executable and return its OS ABI version number.

  Args:
    filename:  the file from which the OS ABI number is extracted.

  Returns:
    The OS ABI version number (as an integer).
  """
  return ord(open(filename, 'rb').read()[ELF_ABIVERSION_OFFSET])
#enddef


def main(argv):
  abi_version = -1
  try:
    (opts, args) = getopt.getopt(argv[1:], 'v:')
  except getopt.error, e:
    print >>sys.stderr, str(e)
    print >>sys.stderr, 'Usage: set_abi_version [-v version_num] filename...'
    return 1
  # endtry
  for opt, val in opts:
    if opt == '-v':
      abi_version = int(val)
    # endif
  # endfor

  for filename in args:
    if abi_version == -1:
      new_abi = 1 + ReadFileOsAbiVersion(filename)
    else:
      new_abi = abi_version
    # endif

    ModifyFileOsAbiVersion(filename, new_abi)
  # endfor

  return 0
# enddef

if __name__ == '__main__':
  sys.exit(main(sys.argv))
# endif
