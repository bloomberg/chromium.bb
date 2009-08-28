#!/usr/bin/env python

"""
Verifies building a project rooted several layers under src_dir works.
"""

import os

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('prog3.gyp', chdir='src/subdir/subdir2')

test.subdir('relocate')
os.rename('src', 'relocate/src')

test.build_all('prog3.gyp', chdir='relocate/src/subdir/subdir2')

test.run_built_executable('prog3',
                          chdir='relocate/src/subdir/subdir2',
                          stdout="Hello from prog3.c\n")

test.pass_test()
