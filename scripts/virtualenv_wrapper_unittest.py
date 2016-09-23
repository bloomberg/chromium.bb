# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for virtualenv_wrapper"""

from __future__ import print_function

import os
import six

from chromite.lib import cros_test_lib

class VirtualEnvTest(cros_test_lib.TestCase):
  """Test that we are running in a virtualenv."""

  def testModuleIsFromVenv(self):
    """Test that the |six| module was provided by virtualenv."""
    # Note: The |six| module is chosen somewhat arbitrarily, but it happens to
    # be provided inside the chromite virtualenv.
    req_path = os.path.dirname(os.path.realpath(six.__file__))
    my_path = os.path.dirname(os.path.realpath(__file__))
    expected_venv_path = os.path.realpath(
        os.path.join(my_path, '..', 'venv', 'venv'))
    self.assertIn(expected_venv_path, req_path)
