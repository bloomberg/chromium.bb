#!/usr/bin/env python

"""
Verifies behavior for different action configuration errors:
exit status of 1, and the expected error message must be in stderr.
"""

import TestGyp

test = TestGyp.TestGyp()


test.run_gyp('action_missing_input.gyp', chdir='src', status=1, stderr=None)
expect = [
  'Need at least one input in action missing_inputs in target broken_actions1',
]
test.must_contain_all_lines(test.stderr(), expect)


test.run_gyp('action_missing_name.gyp', chdir='src', status=1, stderr=None)
expect = [
  "Anonymous action in target broken_actions2.  An action must have an 'action_name' field.",
]
test.must_contain_all_lines(test.stderr(), expect)


test.pass_test()
