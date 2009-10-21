#!/usr/bin/env python

"""
Verifies that a scons build picks up tools modules specified
via 'scons_tools' in the 'scons_settings' dictionary.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('tools.gyp')

test.build('tools.gyp', test.ALL)

if test.format == 'scons':
  expect = "Hello, world!\n"
else:
  expect = ""
test.run_built_executable('tools', stdout=expect)

test.pass_test()
