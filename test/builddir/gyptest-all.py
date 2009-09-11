#!/usr/bin/env python

"""
Verify the settings that cause a set of programs to be created in
a specific build directory, and that no intermediate built files
get created outside of that build directory hierarchy even when
referred to with deeply-nested ../../.. paths.
"""

import os

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('prog1.gyp', '--depth=..', chdir='src')

test.subdir('relocate')
os.rename('src', 'relocate/src')

test.subdir('relocate/builddir')

# Make sure that all the built ../../etc. files only get put under builddir,
# by making all of relocate read-only and then making only builddir writable.
test.writable('relocate', False)
test.writable('relocate/builddir', True)

# Suppress the test infrastructure's setting SYMROOT on the command line.
test.build_all('prog1.gyp', SYMROOT=None, chdir='relocate/src')

expect1 = """\
Hello from prog1.c
Hello from func1.c
"""

expect2 = """\
Hello from subdir2/prog2.c
Hello from func2.c
"""

expect3 = """\
Hello from subdir2/subdir3/prog3.c
Hello from func3.c
"""

expect4 = """\
Hello from subdir2/subdir3/subdir4/prog4.c
Hello from func4.c
"""

expect5 = """\
Hello from subdir2/subdir3/subdir4/subdir5/prog5.c
Hello from func5.c
"""

def run_builddir(prog, expect):
  dir = 'relocate/builddir/Default/'
  test.run(program=test.workpath(dir + prog), stdout=expect)

run_builddir('prog1', expect1)
run_builddir('prog2', expect2)
run_builddir('prog3', expect3)
run_builddir('prog4', expect4)
run_builddir('prog5', expect5)

test.pass_test()
