#!/usr/bin/env python

"""
Verifies that a project hierarchy created with the --generator-output=
option can be built even when it's relocated to a different path.
"""

import sys

import TestGyp

test = TestGyp.TestGyp()

test.writable(test.workpath('src'), False)

test.run_gyp('prog1.gyp',
             '-Dset_symroot=1',
             '--generator-output=' + test.workpath('gypfiles'),
             chdir='src')

test.writable(test.workpath('src'), True)

import os
test.subdir('relocate')
os.rename('src', 'relocate/src')
os.rename('gypfiles', 'relocate/gypfiles')

test.writable(test.workpath('relocate/src'), False)

test.writable(test.workpath('relocate/src/build'), True)
test.writable(test.workpath('relocate/src/subdir2/build'), True)
test.writable(test.workpath('relocate/src/subdir3/build'), True)

test.build_all('prog1.gyp', chdir='relocate/gypfiles')

chdir = 'relocate/gypfiles'

if sys.platform in ('darwin',):
  chdir = 'relocate/src'
test.run_built_executable('prog1',
                          chdir=chdir,
                          stdout="Hello from prog1.c\n")

if sys.platform in ('darwin',):
  chdir = 'relocate/src/subdir2'
test.run_built_executable('prog2',
                          chdir=chdir,
                          stdout="Hello from prog2.c\n")

if sys.platform in ('darwin',):
  chdir = 'relocate/src/subdir3'
test.run_built_executable('prog3',
                          chdir=chdir,
                          stdout="Hello from prog3.c\n")

test.pass_test()
