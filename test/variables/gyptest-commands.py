#!/usr/bin/env python

"""
Test variable expansion of '<!()' syntax commands.
"""

import TestGyp

test = TestGyp.TestGyp(format='gypd')

expect = test.read('commands.gyp.stdout')

test.run_gyp('commands.gyp',
             '--debug', 'variables', '--debug', 'general',
             stdout=expect)

test.must_match('commands.gypd', test.read('commands.gypd.golden'))

test.pass_test()
