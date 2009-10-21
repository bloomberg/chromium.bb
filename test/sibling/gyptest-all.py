#!/usr/bin/env python

"""
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('build/all.gyp', chdir='src')

test.build('build/all.gyp', test.ALL, chdir='src')

chdir = 'src/build'

if test.format == 'xcode':
  chdir = 'src/prog1'
test.run_built_executable('prog1',
                          chdir=chdir,
                          stdout="Hello from prog1.c\n")

if test.format == 'xcode':
  chdir = 'src/prog2'
test.run_built_executable('prog2',
                          chdir=chdir,
                          stdout="Hello from prog2.c\n")

test.pass_test()
