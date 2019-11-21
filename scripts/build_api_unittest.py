# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for build_api.py"""

from __future__ import print_function

import os

from chromite.api import router as router_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.scripts import build_api


class BuildApiScriptTest(cros_test_lib.MockTempDirTestCase):
  """Tests for main()"""

  def setUp(self):
    self.find_mock = self.PatchObject(router_lib.Router, 'Route')
    self.input_json = os.path.join(self.tempdir, 'input.json')
    self.output_json = os.path.join(self.tempdir, 'output.json')
    self.tee_log = os.path.join(self.tempdir, 'tee_out.txt')
    input_json_contents = """
{
  "buildTarget": {
    "name": "amd64-generic"
  }
}
    """
    osutils.WriteFile(self.input_json, input_json_contents)

  def testSmoke(self):
    """Basic sanity check"""
    build_api.main(['--input-json', self.input_json,
                    '--output-json', self.output_json,
                    'chromite.api.PackageService/GetTargetVersions'])

  def testTee(self):
    """Call build_api with tee-log set, verify log contents."""
    build_api.main(['--input-json', self.input_json,
                    '--output-json', self.output_json,
                    '--tee-log', self.tee_log,
                    'chromite.api.PackageService/GetTargetVersions'])
    contents = osutils.ReadFile(self.tee_log)
    self.assertIn('Teeing stdout', contents)
