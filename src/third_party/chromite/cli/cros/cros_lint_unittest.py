# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros lint command."""

from __future__ import print_function

import os

from chromite.cli.cros import cros_lint
from chromite.lib import cros_test_lib
from chromite.lib import osutils


# pylint: disable=protected-access


class LintCommandTest(cros_test_lib.TestCase):
  """Test class for our LintCommand class."""

  def testOutputArgument(self):
    """Tests that the --output argument mapping for cpplint is complete."""
    self.assertEqual(
        set(cros_lint.LintCommand.OUTPUT_FORMATS),
        set(cros_lint.CPPLINT_OUTPUT_FORMAT_MAP.keys()) | {'default'})


class JsonTest(cros_test_lib.TempDirTestCase):
  """Tests for _JsonLintFile."""

  def testValid(self):
    """Verify valid json file is accepted."""
    path = os.path.join(self.tempdir, 'x.json')
    osutils.WriteFile(path, '{}\n')
    ret = cros_lint._JsonLintFile(path, None, None)
    self.assertEqual(ret.returncode, 0)

  def testInvalid(self):
    """Verify invalid json file is rejected."""
    path = os.path.join(self.tempdir, 'x.json')
    osutils.WriteFile(path, '{')
    ret = cros_lint._JsonLintFile(path, None, None)
    self.assertEqual(ret.returncode, 1)

  def testUnicodeBom(self):
    """Verify we skip the Unicode BOM."""
    path = os.path.join(self.tempdir, 'x.json')
    osutils.WriteFile(path, b'\xef\xbb\xbf{}\n', mode='wb')
    ret = cros_lint._JsonLintFile(path, None, None)
    self.assertEqual(ret.returncode, 0)
