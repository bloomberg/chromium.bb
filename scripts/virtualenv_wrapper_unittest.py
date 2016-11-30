# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for virtualenv_wrapper"""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib

_MODULE_DIR = os.path.dirname(os.path.realpath(__file__))
_VENV_DIR = os.path.abspath(os.path.join(_MODULE_DIR, '..', '.venv'))


class VirtualEnvTest(cros_test_lib.TestCase):
  """Test that we are running in a virtualenv."""


  def testModuleIsFromVenv(self):
    """Test that we import |six| from the virtualenv."""
    # Note: The |six| module is chosen somewhat arbitrarily, but it happens to
    # be provided inside the chromite virtualenv.
    six = __import__('six')
    req_path = os.path.dirname(os.path.realpath(six.__file__))
    self.assertIn(_VENV_DIR, req_path)


  def testInsideVenv(self):
    """Test that we are inside a virtualenv."""
    self.assertIn('VIRTUAL_ENV', os.environ)
    venv_dir = os.environ['VIRTUAL_ENV']
    self.assertEqual(_VENV_DIR, venv_dir)
