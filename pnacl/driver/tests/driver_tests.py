#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run all python tests in this directory."""

import os
import sys
import unittest

MODULES = [
    'expand_response_file_test',
    'help_message_test',
    'strip_test',
]

# The tested modules live in the parent directory. Set up the import path
# accordingly.
my_dir = os.path.dirname(sys.argv[0])
sys.path.append(os.path.join(my_dir, '..'))

suite = unittest.TestLoader().loadTestsFromNames(MODULES)
result = unittest.TextTestRunner(verbosity=2).run(suite)
if result.wasSuccessful():
  sys.exit(0)
else:
  sys.exit(1)
