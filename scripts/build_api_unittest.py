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
    self.config_json = os.path.join(self.tempdir, 'config.json')
    self.tee_log = os.path.join(self.tempdir, 'tee_out.txt')

    tee_config = '{"log_path": "%s"}' % self.tee_log

    osutils.WriteFile(self.input_json, '{}')
    osutils.WriteFile(self.config_json, tee_config)


  def testSmoke(self):
    """Basic sanity check"""
    build_api.main(['--input-json', self.input_json,
                    '--output-json', self.output_json,
                    'chromite.api.VersionService/Get'])

  def testTee(self):
    """Call build_api with tee-log set, verify log contents."""
    build_api.main(['--input-json', self.input_json,
                    '--output-json', self.output_json,
                    '--config-json', self.config_json,
                    'chromite.api.VersionService/Get'])
    contents = osutils.ReadFile(self.tee_log)
    self.assertIn('Teeing stdout', contents)

  def testEnvTee(self):
    """Call build_api with tee-log set, verify log contents."""
    os.environ['BUILD_API_TEE_LOG_FILE'] = self.tee_log
    build_api.main(['--input-json', self.input_json,
                    '--output-json', self.output_json,
                    'chromite.api.VersionService/Get'])
    contents = osutils.ReadFile(self.tee_log)
    self.assertIn('Teeing stdout and stderr to env path ', contents)
