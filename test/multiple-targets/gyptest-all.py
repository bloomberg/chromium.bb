#!/usr/bin/env python

"""
"""

import os

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('multiple.gyp', chdir='src')

test.subdir('relocate')
os.rename('src', 'relocate/src')

# TODO(sgk):  remove stderr=None when the --generator-output= support
# gets rid of the scons warning
test.build_all('multiple.gyp', chdir='relocate/src', stderr=None)

expect1 = """\
hello from prog1.c
hello from common.c
"""

expect2 = """\
hello from prog2.c
hello from common.c
"""

test.run_built_executable('prog1', stdout=expect1, chdir='relocate/src')
test.run_built_executable('prog2', stdout=expect2, chdir='relocate/src')

test.pass_test()
