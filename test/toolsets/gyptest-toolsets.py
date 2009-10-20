#!/usr/bin/env python

"""
Verifies that toolsets are correctly applied
"""

import TestGyp

# Multiple toolsets are currently only supported by the make generator.
test = TestGyp.TestGyp(formats=['make'])

test.run_gyp('toolsets.gyp')

test.build_all('toolsets.gyp')

test.run_built_executable('host-main', stdout="Host\n")
test.run_built_executable('target-main', stdout="Target\n")

test.pass_test()
