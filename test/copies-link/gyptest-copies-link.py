#!/usr/bin/env python

"""
Verifies file copies using the build tool default.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('copies-link.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('copies-link.gyp', chdir='relocate/src')

test.pass_test()
