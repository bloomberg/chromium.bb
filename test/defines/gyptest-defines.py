#!/usr/bin/env python

"""
Verifies build of an executable with C++ defines.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('defines.gyp')

test.build('defines.gyp')

expect = """\
FOO is defined
VALUE is 1
"""
test.run_built_executable('defines', stdout=expect)

test.pass_test()
