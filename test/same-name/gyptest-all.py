#!/usr/bin/env python

"""
Build a .gyp with two targets that share a common .c source file.
"""

import os

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('all.gyp', chdir='src')

test.subdir('relocate')
os.rename('src', 'relocate/src')

test.build_all('all.gyp', chdir='relocate/src')

expect1 = """\
Hello from prog1.c
Hello prog1 from func.c
"""

expect2 = """\
Hello from prog2.c
Hello prog2 from func.c
"""

test.run_built_executable('prog1', chdir='relocate/src', stdout=expect1)
test.run_built_executable('prog2', chdir='relocate/src', stdout=expect2)

test.pass_test()
