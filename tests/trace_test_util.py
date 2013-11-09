# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import functools
import os
import sys

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(TESTS_DIR))

import trace_inputs


def check_can_trace(fn):
  """Function decorator that skips test that need to be able trace."""
  @functools.wraps(fn)
  def hook(self, *args, **kwargs):
    if not trace_inputs.can_trace():
      self.fail('Please rerun this test with admin privileges.')
    return fn(self, *args, **kwargs)
  return hook
