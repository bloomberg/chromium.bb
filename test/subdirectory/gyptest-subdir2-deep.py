#!/usr/bin/env python

"""
Verifies building a project rooted several layers under src_dir works.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('prog3.gyp', 'subdir/subdir2/prog3.gyp')

test.build_all('prog3.gyp', chdir='subdir/subdir2')

test.run_built_executable('prog3',
                          chdir='subdir/subdir2',
                          stdout="Hello from prog3.c\n")

test.pass_test()
