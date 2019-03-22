# -*- coding: utf-8 -*-path + os.sep)
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the dependency.py module."""

from __future__ import print_function

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.service import dependency

from os.path import join


class DependencyTests(cros_test_lib.TestCase):
  """General unittests for dependency module."""

  def testNormalizeSourcePathsCollapsingSubPaths(self):
    self.assertEquals(
        dependency.NormalizeSourcePaths(['/foo', '/ab/cd', '/foo/bar']),
        ['/ab/cd', '/foo'])

    self.assertEquals(
        dependency.NormalizeSourcePaths(['/foo/bar', '/ab/cd', '/foo/bar/..',
                                         '/ab/cde']),
        ['/ab/cd', '/ab/cde', '/foo'])

  def testNormalizeSourcePathsFormatingDirectoryPaths(self):
    with osutils.TempDir() as tempdir:
      foo_dir = join(tempdir, 'foo')
      bar_baz_dir = join(tempdir, 'bar', 'baz')
      osutils.SafeMakedirs(join(tempdir, 'ab'))
      ab_cd_file = join(tempdir, 'ab', 'cd')

      osutils.SafeMakedirs(foo_dir)
      osutils.SafeMakedirs(bar_baz_dir)
      osutils.WriteFile(ab_cd_file, 'alphabet')

      self.assertEquals(
          dependency.NormalizeSourcePaths([foo_dir, ab_cd_file, bar_baz_dir]),
          [ab_cd_file, bar_baz_dir + '/', foo_dir + '/'])
