#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium wrapper for pylint for passing args via stdin.

This will be executed by vpython with the right pylint versions.
"""

from __future__ import print_function

import os
import sys

from pylint import lint


HERE = os.path.dirname(os.path.abspath(__file__))
PYLINT = os.path.join(HERE, 'pylint_main.py')
RC_FILE = os.path.join(HERE, 'pylintrc')

ARGS_ON_STDIN = '--args-on-stdin'


def main(argv):
  """Our main wrapper."""
  # Add support for a custom mode where arguments are fed line by line on
  # stdin. This allows us to get around command line length limitations.
  if ARGS_ON_STDIN in argv:
    argv = [x for x in argv if x != ARGS_ON_STDIN]
    argv.extend(x.strip() for x in sys.stdin)

  # We prepend the command-line with the depot_tools rcfile. If another rcfile
  # is to be used, passing --rcfile a second time on the command-line will work
  # fine.
  if os.path.isfile(RC_FILE):
    # The file can be removed to test 'normal' pylint behavior.
    argv.insert(0, '--rcfile=%s' % RC_FILE)

  lint.Run(argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
