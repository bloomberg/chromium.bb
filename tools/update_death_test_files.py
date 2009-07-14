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


"""Update the OS ABI number of known death test NaCl modules.

Driver code to update death test nexe files.  The user is expected to
run this on a Linux or OSX system (due to file paths) in the
native_client directory.

"""

import re
import sys

import set_abi_version


NACL_DEATH_TESTS = [
    'service_runtime/testdata/integer_overflow_while_madvising.nexe',
    'service_runtime/testdata/negative_hole.nexe',
    'service_runtime/testdata/text_too_big.nexe',
    ]


def GetCurrentAbiVersion():
  return int(re.search(r'^#define.*EF_NACL_ABIVERSION\s+(\d+)',
                       open('include/nacl_elf.h', 'rt').read(),
                       re.M).group(1))


def main():
  abi = GetCurrentAbiVersion()
  for f in NACL_DEATH_TESTS:
    print 'Updating: ', f
    try:
      set_abi_version.ModifyFileOsAbiVersion(f, abi)
    except IOError, e:
      print 'FAILED: ', str(e)
      return 1
    # endtry
  # endfor
# enddef

if __name__ == '__main__':
  sys.exit(main())
# endif
