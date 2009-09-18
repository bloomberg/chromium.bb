#!/usr/bin/env python

"""
Verifies simple build of a "Hello, world!" program with a static library.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('library.gyp', '-Dlibrary=static_library', chdir='src')

test.relocate('src', 'relocate/src')

test.build_all('library.gyp', chdir='relocate/src')

expect = """\
Hello from program.c
Hello from lib1.c
"""
test.run_built_executable('program', chdir='relocate/src', stdout=expect)

test.pass_test()
