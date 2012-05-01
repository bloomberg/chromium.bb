#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""

Make sure the program in a command can be a called batch file, or an
application in the path. Specifically, this means not quoting something like
"call x.bat", lest the shell look for a program named "call x.bat", rather
than calling "x.bat".
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])
  CHDIR = 'command-quote'
  test.run_gyp('command-quote.gyp', chdir=CHDIR)
  test.build('command-quote.gyp', test.ALL, chdir=CHDIR)
  test.pass_test()
