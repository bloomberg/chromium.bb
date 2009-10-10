#!/usr/bin/env python

"""
Verify that a link time only dependency will get pulled into the set of built
targets, even if no executable uses it.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('lib_only.gyp')

test.build_all('lib_only.gyp')

test.built_lib_must_exist('a')

# TODO(bradnelson/mark):
# On linux and windows a library target will at least pull its link dependencies
# into the generated sln/_main.scons, since not doing so confuses users.
# This is not currently implemented on mac, which has the opposite behavior.
if test.format == 'xcode':
  test.built_lib_must_not_exist('b')
else:
  test.built_lib_must_exist('b')

test.pass_test()
