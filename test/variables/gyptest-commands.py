#!/usr/bin/env python

"""
Test variable expansion of '<!()' syntax commands.
"""

import os

import TestGyp

test = TestGyp.TestGyp(format='gypd')

expect = test.read('commands.gyp.stdout')

# Set $HOME so that gyp doesn't read the user's actual
# ~/.gyp/include.gypi file, which may contain variables
# and other settings that would change the output.
os.environ['HOME'] = test.workpath()

test.run_gyp('commands.gyp',
             '--debug', 'variables', '--debug', 'general',
             stdout=expect)

test.must_match('commands.gypd', test.read('commands.gypd.golden'))

test.pass_test()
