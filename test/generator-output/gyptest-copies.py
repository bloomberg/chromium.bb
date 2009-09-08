#!/usr/bin/env python

"""
Verifies file copies using an explicit build target of 'all'.
"""

import os
import sys

import TestGyp

test = TestGyp.TestGyp()

test.writable(test.workpath('copies'), False)

test.run_gyp('copies.gyp',
             '--generator-output=' + test.workpath('gypfiles'),
             chdir='copies')

test.writable(test.workpath('copies'), True)

test.subdir('relocate')
os.rename('copies', 'relocate/copies')
os.rename('gypfiles', 'relocate/gypfiles')

test.writable(test.workpath('relocate/copies'), False)

test.writable(test.workpath('relocate/copies/build'), True)
test.writable(test.workpath('relocate/copies/copies-out'), True)

test.build_all('copies.gyp', chdir='relocate/gypfiles')

if sys.platform in ('darwin',):
  chdir = 'relocate/copies'
elif sys.platform in ('win32',):
  chdir = 'relocate\\copies'
else:
  chdir = 'relocate/gypfiles'

test.must_match([chdir, 'copies-out', 'file1'],
                "file1 contents\n")

if sys.platform in ('darwin',):
  chdir = 'relocate/copies/build'
elif sys.platform in ('win32',):
  chdir = 'relocate\\gypfiles'
else:
  chdir = 'relocate/gypfiles'
test.must_match([chdir, 'Default', 'copies-out', 'file2'], "file2 contents\n")

test.pass_test()
