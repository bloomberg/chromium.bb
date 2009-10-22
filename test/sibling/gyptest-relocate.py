#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('build/all.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('build/all.gyp', test.ALL, chdir='relocate/src')

chdir = 'relocate/src/build'

if test.format == 'xcode':
  chdir = 'relocate/src/prog1'
test.run_built_executable('prog1',
                          chdir=chdir,
                          stdout="Hello from prog1.c\n")

if test.format == 'xcode':
  chdir = 'relocate/src/prog2'
test.run_built_executable('prog2',
                          chdir=chdir,
                          stdout="Hello from prog2.c\n")

test.pass_test()
