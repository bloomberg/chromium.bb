# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from http_client_local import HttpClientLocal

class HttpClientLocalTest(unittest.TestCase):

  def testGetWithoutParameters(self):
    code, deps = HttpClientLocal.Get(
        'https://src.chromium.org/chrome/trunk/src/DEPS')
    self.assertEqual(200, code)
    self.assertTrue(isinstance(deps, str))
