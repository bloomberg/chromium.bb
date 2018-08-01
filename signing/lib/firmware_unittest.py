# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests Firmware related signing"""

from __future__ import print_function

from chromite.lib import cros_test_lib
from chromite.signing.lib import firmware
from chromite.signing.lib import signer_unittest


class TestECSigner(cros_test_lib.RunCommandTestCase,
                   cros_test_lib.TempDirTestCase):
  """Test ECSigner."""

  def testGetCmdArgs(self):
    ecs = firmware.ECSigner()
    ks = signer_unittest.KeysetFromSigner(ecs, self.tempdir)

    self.assertListEqual(ecs.GetFutilityArgs(ks, 'foo', 'bar'),
                         ['sign',
                          '--type', 'rwsig',
                          '--prikey', ks.keys['ec'].private,
                          'foo', 'bar'])
