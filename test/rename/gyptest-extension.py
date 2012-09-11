#!/usr/bin/env python

# Copyright (c) 2010 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Checks that files whose extension changes get rebuilt correctly.
"""

import os
import TestGyp

# .c -> .cc renames don't work with make.  # XXX
test = TestGyp.TestGyp()#formats=['!make'])
CHDIR = 'extension'
test.run_gyp('test.gyp', chdir=CHDIR)
test.build('test.gyp', test.ALL, chdir=CHDIR)
test.run_built_executable('extrename', stdout="C\n", chdir=CHDIR)

test.write('extension/test.gyp',
           test.read('extension/test.gyp').replace('file.c', 'file.cc'))
test.run_gyp('test.gyp', chdir=CHDIR)
test.build('test.gyp', test.ALL, chdir=CHDIR)
test.run_built_executable('extrename', stdout="C++\n", chdir=CHDIR)

test.pass_test()

