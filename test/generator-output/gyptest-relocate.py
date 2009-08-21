#!/usr/bin/env python

"""
Verifies that a project hierarchy created with the --generator-output=
option can be built even when it's relocated to a different path.
"""

import sys

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('prog1.gyp',
             '-Dset_symroot=1',
             '--generator-output=' + test.workpath('gypfiles'),
             chdir='src')

import os
test.subdir('reloc')
os.rename('src', 'reloc/src')
os.rename('gypfiles', 'reloc/gypfiles')

test.build_all('prog1.gyp', chdir='reloc/gypfiles')

chdir = 'reloc/gypfiles'

if sys.platform in ('darwin',):
  chdir = 'reloc/src'
test.run_built_executable('prog1',
                          chdir=chdir,
                          stdout="Hello from prog1.c\n")

if sys.platform in ('darwin',):
  chdir = 'reloc/src/subdir'
test.run_built_executable('prog2',
                          chdir=chdir,
                          stdout="Hello from prog2.c\n")

test.pass_test()
