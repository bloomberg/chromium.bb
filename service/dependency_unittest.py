# -*- coding: utf-8 -*-path + os.sep)
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the dependency.py module."""

from __future__ import print_function

from chromite.lib import cros_test_lib
from chromite.service import dependency


class DependencyTests(cros_test_lib.TestCase):
  """General unittests for dependency module."""

  def testNormalizeSourcePaths(self):
    self.assertEquals(
        dependency.NormalizeSourcePaths(['/foo', '/ab/cd', '/foo/bar']),
        ['/ab/cd', '/foo'])

    self.assertEquals(
        dependency.NormalizeSourcePaths(['/foo/bar', '/ab/cd', '/foo/bar/..',
                                         '/ab/cde']),
        ['/ab/cd', '/ab/cde', '/foo'])
