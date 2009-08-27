#!/usr/bin/env python

"""
Test variable expansion of '<!()' syntax commands.

This is complicated by the fact that the output changes if the
user has a ~/.gyp/include.gpi file.  A variable expansion shows
up in the "--debug variables" output, and the include.gypi file
shows up in the 'included_files' list in the commands.gypd output.
We do some regular expression substitutions to strip those before
comparing against our expected output, which is somewhat grungy
but probably less of a hassle overall than trying to massage the
expected output.
"""

import re

import TestGyp

test = TestGyp.TestGyp(format='gypd')

test.run_gyp('commands.gyp', '--debug', 'variables', '--debug', 'general')


# Strip the variable expansion line from the output
# before we compare it by hand.

include_gypi_line = r"^VARIABLES: Expanding .*\.gyp/include\.gypi'$\n"
include_gypi = re.compile(include_gypi_line, re.M)
output = include_gypi.sub('', test.stdout())

expect = test.read('commands.gyp.stdout')

if output != expect:
  test.diff(expect, output, 'STDOUT ')
  test.fail_test()


# Strip the include.gypi string from commands.gypd output
# before we compare it by hand.

include_gypi = re.compile(",[^']*'[^']*\.gyp/include\.gypi',[^']*'", re.M)
contents = include_gypi.sub(", '", test.read('commands.gypd'))

expect = test.read('commands.gypd.golden')

if contents != expect:
  test.diff(expect, contents, 'commands.gypd ')
  test.fail_test()


test.pass_test()
