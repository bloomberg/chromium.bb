# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import constraints


class WebBluetoothConstraintsTest(unittest.TestCase):

    def testGetRandomValidUUIDString(self):
        # Regex from: https://webbluetoothcg.github.io/web-bluetooth/#uuids
        self.assertRegexpMatches(
            constraints.GetValidUUIDString(),
            '^\'[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\'$')
