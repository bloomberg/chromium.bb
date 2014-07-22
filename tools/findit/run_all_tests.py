#!/usr/bin/env python
# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import unittest

from chromium_deps_unittest import ChromiumDEPSTest


if __name__ == '__main__':
   all_tests_suite = unittest.defaultTestLoader.loadTestsFromModule(
             sys.modules[__name__])
   tests = unittest.TestSuite(all_tests_suite)
   result = unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(tests)
   sys.exit(len(result.failures) + len(result.errors))
