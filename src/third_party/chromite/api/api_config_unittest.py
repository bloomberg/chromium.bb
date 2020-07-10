# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""API Config unit tests."""

from __future__ import print_function

from chromite.api.api_config import ApiConfig
from chromite.lib import cros_test_lib


class ApiConfigTest(cros_test_lib.TestCase):
  """ApiConfig tests."""

  def test_incompatible_arguments(self):
    """Test incompatible argument checks."""
    # Can only specify True for one of validate_only, mock_call, and mock_error.
    # Enumerating them was unnecessary, but it's only 4 cases.
    with self.assertRaises(AssertionError):
      ApiConfig(validate_only=True, mock_call=True)
    with self.assertRaises(AssertionError):
      ApiConfig(validate_only=True, mock_error=True)
    with self.assertRaises(AssertionError):
      ApiConfig(mock_call=True, mock_error=True)
    with self.assertRaises(AssertionError):
      ApiConfig(validate_only=True, mock_call=True, mock_error=True)

  def test_do_validation(self):
    """Sanity check for the do validation property being True."""
    # Should validate by default, and when only doing validation.
    config = ApiConfig()
    self.assertTrue(config.do_validation)
    config = ApiConfig(validate_only=True)
    self.assertTrue(config.do_validation)

  def test_no_do_validation(self):
    """Sanity check for skipping validation for mock calls."""
    config = ApiConfig(mock_call=True)
    self.assertFalse(config.do_validation)
    config = ApiConfig(mock_error=True)
    self.assertFalse(config.do_validation)
