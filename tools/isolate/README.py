#!/usr/bin/env python
# coding=utf-8
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

###
# Run me to generate the documentation!
###

"""Test tracing and isolation infrastructure.

Scripts are compartmentalized by their name:
- trace_*.py: Tracing infrastructure scripts.
- isolate_*.py: Executable isolation scripts. (TODO)
- *_test_cases.py: Scripts specifically managing GTest executables.

A few scripts have strict dependency rules:
- run_test_cases.py, run_test_from_archive.py, shard.py and trace_inputs.py
  depends on no other script so they can be run outside the checkout.
- The pure tracing scripts (trace_inputs.py and trace_test_cases.py) do not know
  about isolate infrastructure.
- Scripts without _test_cases suffix do not know about GTest.
- Scripts without isolate_ prefix do not know about the isolation
  infrastructure. (TODO)

See http://dev.chromium.org/developers/testing/isolated-testing for more info.
"""

import os
import sys


def main():
  for i in sorted(os.listdir(os.path.dirname(os.path.abspath(__file__)))):
    if not i.endswith('.py') or i == 'PRESUBMIT.py':
      continue
    module = __import__(i[:-3])
    if hasattr(module, '__doc__'):
      print module.__name__
      print ''.join('  %s\n' % i for i in module.__doc__.splitlines())
  return 0


if __name__ == '__main__':
  sys.exit(main())
