# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""chroot_lib unit tests."""

from __future__ import print_function

from chromite.lib import chroot_lib
from chromite.lib import cros_test_lib


class ChrootTest(cros_test_lib.TestCase):
  """Chroot class tests."""

  def testGetEnterArgsEmpty(self):
    """Test empty instance behavior."""
    chroot = chroot_lib.Chroot()
    self.assertFalse(chroot.GetEnterArgs())

  def testGetEnterArgsAll(self):
    """Test complete instance behavior."""
    path = '/chroot/path'
    cache_dir = '/cache/dir'
    expected = ['--chroot', path, '--cache-dir', cache_dir]
    chroot = chroot_lib.Chroot(path=path, cache_dir=cache_dir)

    self.assertItemsEqual(expected, chroot.GetEnterArgs())
