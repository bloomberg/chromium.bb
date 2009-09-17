#!/usr/bin/env python

"""
Verifies --generator-output= behavior when using rules.
"""

import sys

import TestGyp

test = TestGyp.TestGyp()

test.writable(test.workpath('rules'), False)

test.run_gyp('rules.gyp',
             '--generator-output=' + test.workpath('gypfiles'),
             chdir='rules')

test.writable(test.workpath('rules'), True)

test.relocate('rules', 'relocate/rules')
test.relocate('gypfiles', 'relocate/gypfiles')

test.writable(test.workpath('relocate/rules'), False)

test.writable(test.workpath('relocate/rules/build'), True)
test.writable(test.workpath('relocate/rules/subdir1/build'), True)
test.writable(test.workpath('relocate/rules/subdir2/build'), True)
test.writable(test.workpath('relocate/rules/subdir2/rules-out'), True)

test.build_all('rules.gyp', chdir='relocate/gypfiles')

expect = """\
Hello from program.c
Hello from function1.in
Hello from function2.in
"""

if sys.platform in ('darwin',):
  chdir = 'relocate/rules/subdir1'
else:
  chdir = 'relocate/gypfiles'
test.run_built_executable('program', chdir=chdir, stdout=expect)

test.must_match('relocate/rules/subdir2/rules-out/file1.out',
                "Hello from file1.in\n")
test.must_match('relocate/rules/subdir2/rules-out/file2.out',
                "Hello from file2.in\n")

test.pass_test()
