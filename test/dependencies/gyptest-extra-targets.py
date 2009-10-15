#!/usr/bin/env python

"""
Verify that dependencies don't pull unused targets into the build.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('extra_targets.gyp')

# This should fail if it tries to build 'c_unused' since 'c/c.c' has a syntax
# error and won't compile.
test.build_all('extra_targets.gyp')

test.pass_test()
