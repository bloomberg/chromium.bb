# Copyright (C) 2022 Bloomberg L.P. All rights reserved.
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


import os
import sys


def main(argv):
  src_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
  chrome_version_path = os.path.join(src_dir, 'chrome', 'VERSION')
  bb_version_path = os.path.join(src_dir, 'BB_VERSION')

  with open(chrome_version_path) as f:
    lines = f.readlines()
    lines = [x[6:] for x in lines]  # strip MAJOR=,MINOR=,etc
    chromium_version = [int(x) for x in lines]

  if os.path.exists(bb_version_path):
    with open(bb_version_path) as f:
      lines = f.readlines()
      lines = [x[5:] for x in lines]  # strip TRML=,BBNO=
      bb_version = [int(x) for x in lines]
      assert(bb_version[1] < 100)
  else:
    bb_version = [0, 0]

  bb_version_number = bb_version[0] * 100 + bb_version[1]
  bb_version_str = str(bb_version_number)
  if bb_version_number < 10:
    bb_version_str = '0' + bb_version_str
  bb_version_str = 'bb' + bb_version_str

  print('"%d.%d.%d.%d_%s"' % (chromium_version[0],
                              chromium_version[1],
                              chromium_version[2],
                              chromium_version[3],
                              bb_version_str))
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
