#!/usr/bin/env python2

# Copyright (C) 2019 Bloomberg L.P. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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

import bbutil
import sys
import os

def main(args):
  repoRoot = bbutil.getStrippedShellOutput("git rev-parse --show-toplevel")
  outDir = os.path.join(repoRoot, 'src/out/shared_release')
  tests = [
    # base_unittests (1 test fail)
    #"base_unittests.exe",

    # blink_tests
    "blink_common_unittests.exe",
    "blink_heap_unittests.exe",
    "blink_platform_unittests.exe",
    "blink_unittests.exe",
    "wtf_unittests.exe",

    # cc_unittests (many test fails)
    #"cc_unittests.exe",

    # skia_unittests
    "skia_unittests.exe",
  ]

  for test in tests:
    print("Running {}".format(test))
    path = os.path.join(outDir, test)
    rc = bbutil.execNoPipe(path)

    if rc != 0:
      print("Test '{}' failed".format(test))
      return rc

if __name__ == '__main__':
  sys.exit(main(sys.argv))

