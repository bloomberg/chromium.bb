#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys
import tempfile
import json
import unittest

from core import path_util
sys.path.insert(1, path_util.GetTelemetryDir())
sys.path.insert(
    1, os.path.join(path_util.GetTelemetryDir(), 'third_party', 'mock'))

from telemetry import decorators

import mock

import process_perf_results as ppr_module


class _FakeLogdogStream(object):

  def write(self, data):
    pass

  def close(self):
    pass

  def get_viewer_url(self):
    return 'http://foobar.not.exit'


class ProcessPerfResultsIntegrationTest(unittest.TestCase):
  def setUp(self):
    self.test_dir = tempfile.mkdtemp()
    self.output_json = os.path.join(self.test_dir, 'output.json')
    self.service_account_file = os.path.join(
        self.test_dir, 'fake_service_account.json')
    with open(self.service_account_file, 'w') as f:
      json.dump([1,2,3,4], f)
    self.task_output_dir = os.path.join(
        os.path.dirname(__file__), 'testdata', 'task_output_dir')

    m1 = mock.patch(
        'process_perf_results.logdog_helper.text',
        return_value = 'http://foo.link')
    m1.start()
    self.addCleanup(m1.stop)

    m2 = mock.patch(
        'process_perf_results.logdog_helper.open_text',
        return_value=_FakeLogdogStream())
    m2.start()
    self.addCleanup(m2.stop)

    m3 = mock.patch('core.results_dashboard.SendResults')
    m3.start()
    self.addCleanup(m3.stop)


  def tearDown(self):
    shutil.rmtree(self.test_dir)

  @decorators.Disabled('chromeos')  # crbug.com/865800
  def testIntegration(self):
    build_properties = json.dumps({
        'perf_dashboard_machine_group': 'test-builder',
        'buildername': 'test-builder',
        'buildnumber': 777,
        'got_v8_revision': 'beef1234',
        'got_revision_cp': 'refs/heads/master@{#1234}',
        'got_webrtc_revision': 'fee123',
        'git_revision': 'deadbeef'
    })
    ppr_module.process_perf_results(
        self.output_json, configuration_name='test-builder',
        service_account_file = self.service_account_file,
        build_properties=build_properties,
        task_output_dir=self.task_output_dir,
        smoke_test_mode=False)

if __name__ == '__main__':
  unittest.main()
