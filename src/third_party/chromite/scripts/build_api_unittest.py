# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the build_api script covering the base Build API functionality."""

from __future__ import print_function

from chromite.api.gen import build_api_test_pb2
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.scripts import build_api


class RouterTest(cros_test_lib.MockTestCase):
  """Test Router functionality."""
  _INPUT_JSON = '{"id":"Input ID"}'

  def setUp(self):
    self.router = build_api.Router()
    self.router.Register(build_api_test_pb2)

  def testInputOutputMethod(self):
    """Test input/output handling."""
    def impl(input_msg, output_msg):
      self.assertIsInstance(input_msg, build_api_test_pb2.TestRequestMessage)
      self.assertIsInstance(output_msg, build_api_test_pb2.TestResultMessage)

    self.PatchObject(self.router, '_GetMethod', return_value=impl)

    self.router.Route('chromite.api.TestApiService', 'InputOutputMethod',
                      self._INPUT_JSON)

  def testRenameMethod(self):
    """Test implementation name config."""
    def _GetMethod(_, method_name):
      self.assertEqual('CorrectName', method_name)
      return lambda x, y: None

    self.PatchObject(self.router, '_GetMethod', side_effect=_GetMethod)

    self.router.Route('chromite.api.TestApiService', 'RenamedMethod',
                      self._INPUT_JSON)

  def testInsideServiceChrootAsserts(self):
    """Test the chroot assertion handling with service inside configured."""
    # Helper variables/functions to make the patches simpler.
    should_be_called = False
    is_inside = False
    def impl(_input_msg, _output_msg):
      self.assertTrue(should_be_called,
                      'The implementation should not have been called.')
    def inside():
      return is_inside

    self.PatchObject(self.router, '_GetMethod', return_value=impl)
    self.PatchObject(cros_build_lib, 'IsInsideChroot', side_effect=inside)

    # Not inside chroot with inside requirement should raise an error.
    with self.assertRaises(cros_build_lib.DieSystemExit):
      self.router.Route('chromite.api.InsideChrootApiService',
                        'InsideServiceInsideMethod', self._INPUT_JSON)

    # Inside chroot with inside requirement.
    is_inside = should_be_called = True
    self.router.Route('chromite.api.InsideChrootApiService',
                      'InsideServiceInsideMethod', self._INPUT_JSON)

    # Inside chroot with outside override should raise assertion.
    is_inside = True
    should_be_called = False
    with self.assertRaises(cros_build_lib.DieSystemExit):
      self.router.Route('chromite.api.InsideChrootApiService',
                        'InsideServiceOutsideMethod', self._INPUT_JSON)

    is_inside = False
    should_be_called = True
    self.router.Route('chromite.api.InsideChrootApiService',
                      'InsideServiceOutsideMethod', self._INPUT_JSON)

  def testOutsideServiceChrootAsserts(self):
    """Test the chroot assertion handling with service outside configured."""
    # Helper variables/functions to make the patches simpler.
    should_be_called = False
    is_inside = False
    def impl(_input_msg, _output_msg):
      self.assertTrue(should_be_called,
                      'The implementation should not have been called.')

    self.PatchObject(self.router, '_GetMethod', return_value=impl)
    self.PatchObject(cros_build_lib, 'IsInsideChroot',
                     side_effect=lambda: is_inside)

    # Outside chroot with outside requirement should be fine.
    is_inside = False
    should_be_called = True
    self.router.Route('chromite.api.OutsideChrootApiService',
                      'OutsideServiceOutsideMethod', self._INPUT_JSON)

    # Inside chroot with outside requirement should raise error.
    is_inside = True
    should_be_called = False
    with self.assertRaises(cros_build_lib.DieSystemExit):
      self.router.Route('chromite.api.OutsideChrootApiService',
                        'OutsideServiceOutsideMethod', self._INPUT_JSON)

    # Outside chroot with inside override should raise error.
    is_inside = should_be_called = False
    with self.assertRaises(cros_build_lib.DieSystemExit):
      self.router.Route('chromite.api.OutsideChrootApiService',
                        'OutsideServiceInsideMethod', self._INPUT_JSON)

    # Inside chroot with inside override should be fine.
    is_inside = should_be_called = True
    self.router.Route('chromite.api.OutsideChrootApiService',
                      'OutsideServiceInsideMethod', self._INPUT_JSON)
