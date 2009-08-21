#!/usr/bin/env python

"""
"""

import os
import sys

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('build/all.gyp', chdir='src')

test.subdir('relocate')
os.rename('src', 'relocate/src')

test.build_all('build/all.gyp', chdir='relocate/src')

chdir = 'relocate/src/build'

if sys.platform in ('darwin',):
  chdir = 'relocate/src/prog1'
test.run_built_executable('prog1',
                          chdir=chdir,
                          stdout="Hello from prog1.c\n")

if sys.platform in ('darwin',):
  chdir = 'relocate/src/prog2'
test.run_built_executable('prog2',
                          chdir=chdir,
                          stdout="Hello from prog2.c\n")

test.pass_test()
