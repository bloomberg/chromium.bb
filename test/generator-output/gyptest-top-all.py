#!/usr/bin/env python

"""
Verifies building a project hierarchy created when the --generator-output=
option is used to put the build configuration files in a separate
directory tree.
"""

import sys

import TestGyp

test = TestGyp.TestGyp()

test.writable(test.workpath('src'), False)

test.writable(test.workpath('src/build'), True)
test.writable(test.workpath('src/subdir2/build'), True)
test.writable(test.workpath('src/subdir3/build'), True)


test.run_gyp('prog1.gyp',
             '-Dset_symroot=1',
             '--generator-output=' + test.workpath('gypfiles'),
             chdir='src')

test.build_all('prog1.gyp', chdir='gypfiles')

chdir = 'gypfiles'

if sys.platform in ('darwin',):
  chdir = 'src'
test.run_built_executable('prog1',
                          chdir=chdir,
                          stdout="Hello from prog1.c\n")

if sys.platform in ('darwin',):
  chdir = 'src/subdir2'
test.run_built_executable('prog2',
                          chdir=chdir,
                          stdout="Hello from prog2.c\n")

if sys.platform in ('darwin',):
  chdir = 'src/subdir3'
test.run_built_executable('prog3',
                          chdir=chdir,
                          stdout="Hello from prog3.c\n")

test.pass_test()
