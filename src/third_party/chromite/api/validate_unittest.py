# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the validate module."""

from __future__ import print_function

import os

from chromite.api import validate
from chromite.api.gen.chromiumos import common_pb2
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils


class ExistsTest(cros_test_lib.TempDirTestCase):
  """Tests for the exists validator."""

  def test_not_exists(self):
    """Test the validator fails when given a path that doesn't exist."""
    path = os.path.join(self.tempdir, 'DOES_NOT_EXIST')

    @validate.exists('path')
    def impl(_input_proto):
      self.fail('Incorrectly allowed method to execute.')

    with self.assertRaises(cros_build_lib.DieSystemExit):
      impl(common_pb2.Chroot(path=path))

  def test_exists(self):
    """Test the validator fails when given a path that doesn't exist."""
    path = os.path.join(self.tempdir, 'chroot')
    osutils.SafeMakedirs(path)

    @validate.exists('path')
    def impl(_input_proto):
      pass

    impl(common_pb2.Chroot(path=path))


class RequiredTest(cros_test_lib.TestCase):
  """Tests for the required validator."""

  def test_invalid_field(self):
    """Test validator fails when given an unset value."""

    @validate.require('does.not.exist')
    def impl(_input_proto):
      self.fail('Incorrectly allowed method to execute.')

    with self.assertRaises(cros_build_lib.DieSystemExit):
      impl(common_pb2.Chroot())

  def test_not_set(self):
    """Test validator fails when given an unset value."""

    @validate.require('env.use_flags')
    def impl(_input_proto):
      self.fail('Incorrectly allowed method to execute.')

    with self.assertRaises(cros_build_lib.DieSystemExit):
      impl(common_pb2.Chroot())

  def test_set(self):
    """Test validator passes when given set values."""

    @validate.require('path', 'env.use_flags')
    def impl(_input_proto):
      pass

    impl(common_pb2.Chroot(path='/chroot/path',
                           env={'use_flags': [{'flag': 'test'}]}))

  def test_mixed(self):
    """Test validator passes when given a set value."""

    @validate.require('path', 'env.use_flags')
    def impl(_input_proto):
      pass

    with self.assertRaises(cros_build_lib.DieSystemExit):
      impl(common_pb2.Chroot(path='/chroot/path'))
