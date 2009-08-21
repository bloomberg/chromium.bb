#!/usr/bin/env python

"""
Verifies building a target from a .gyp file a few subdirectories
deep when the --generator-output= option is used to put the build
configuration files in a separate directory tree.
"""

import sys

import TestGyp

test = TestGyp.TestGyp()

test.writable(test.workpath('src'), False)

test.writable(test.workpath('src/subdir/subdir2/build'), True)

test.run_gyp('prog3.gyp',
             '-Dset_symroot=1',
             '--generator-output=' + test.workpath('gypfiles'),
             chdir='src/subdir/subdir2')

test.build_all('prog3.gyp', chdir='gypfiles')

chdir = 'gypfiles'

if sys.platform in ('darwin',):
  chdir = 'src/subdir/subdir2'
test.run_built_executable('prog3',
                          chdir=chdir,
                          stdout="Hello from prog3.c\n")

test.pass_test()
