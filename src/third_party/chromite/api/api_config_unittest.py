# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""API Config unit tests."""

from __future__ import print_function

import sys

from chromite.api.api_config import ApiConfig
from chromite.lib import cros_test_lib


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class ApiConfigTest(cros_test_lib.TestCase):
  """ApiConfig tests."""

  def test_do_validation(self):
    """Sanity check for the do validation property being True."""
    # Should validate by default, and when only doing validation.
    config = ApiConfig()
    self.assertTrue(config.do_validation)
    config = ApiConfig(call_type=ApiConfig.CALL_TYPE_VALIDATE_ONLY)
    self.assertTrue(config.do_validation)

  def test_no_do_validation(self):
    """Sanity check for skipping validation for mock calls."""
    config = ApiConfig(call_type=ApiConfig.CALL_TYPE_MOCK_SUCCESS)
    self.assertFalse(config.do_validation)
    config = ApiConfig(call_type=ApiConfig.CALL_TYPE_MOCK_FAILURE)
    self.assertFalse(config.do_validation)
