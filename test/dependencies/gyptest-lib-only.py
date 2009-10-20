#!/usr/bin/env python

"""
Verify that a link time only dependency will get pulled into the set of built
targets, even if no executable uses it.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('lib_only.gyp')

test.build_all('lib_only.gyp')

# Make doesn't put static libs in a common 'lib' directory, like it does with
# shared libs, so check in the obj path corresponding to the source path.
if test.format == 'make':
  test.built_lib_must_exist('a', libdir='obj.target')
else:
  test.built_lib_must_exist('a')

# TODO(bradnelson/mark):
# On linux and windows a library target will at least pull its link dependencies
# into the generated sln/_main.scons, since not doing so confuses users.
# This is not currently implemented on mac, which has the opposite behavior.
if test.format == 'xcode':
  test.built_lib_must_not_exist('b')
elif test.format == 'make':
  test.built_lib_must_exist('b', libdir='obj.target/b')
else:
  test.built_lib_must_exist('b')

test.pass_test()
