#!/usr/bin/env python

"""
Verifies use of include_dirs when using the default build target.
"""

import os
import sys

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('includes.gyp', chdir='src')

test.subdir('relocate')
os.rename('src', 'relocate/src')

test.build_all('includes.gyp', chdir='relocate/src')

expect = """\
Hello from includes.c
Hello from inc.h
Hello from include1.h
Hello from subdir/inc2/include2.h
"""
test.run_built_executable('includes', stdout=expect, chdir='relocate/src')

if sys.platform in ('darwin',):
  chdir='relocate/src/subdir'
else:
  chdir='relocate/src'

expect = """\
Hello from subdir/subdir_includes.c
Hello from subdir/inc.h
Hello from include1.h
Hello from subdir/inc2/include2.h
"""
test.run_built_executable('subdir_includes', stdout=expect, chdir=chdir)

test.pass_test()
