#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import unittest

from unexpected_passes import gpu_builders


class BuilderRunsTestOfInterestUnittest(unittest.TestCase):
  def setUp(self):
    self.instance = gpu_builders.GpuBuilders()

  def testMatch(self):
    """Tests that a match can be successfully found."""
    test_map = {
        'isolated_scripts': [
            {
                'args': [
                    'webgl_conformance',
                ],
                'isolate_name': 'telemetry_gpu_integration_test',
            },
        ],
    }
    self.assertTrue(
        self.instance._BuilderRunsTestOfInterest(test_map, 'webgl_conformance'))

  def testNoMatchIsolate(self):
    """Tests that a match is not found if the isolate name is not valid."""
    test_map = {
        'isolated_scripts': [
            {
                'args': [
                    'webgl_conformance',
                ],
                'isolate_name': 'not_telemetry',
            },
        ],
    }
    self.assertFalse(
        self.instance._BuilderRunsTestOfInterest(test_map, 'webgl_conformance'))

  def testNoMatchSuite(self):
    """Tests that a match is not found if the suite name is not valid."""
    test_map = {
        'isolated_scripts': [
            {
                'args': [
                    'not_a_suite',
                ],
                'isolate_name': 'telemetry_gpu_integration_test',
            },
        ],
    }
    self.assertFalse(
        self.instance._BuilderRunsTestOfInterest(test_map, 'webgl_conformance'))

  def testAndroidSuffixes(self):
    """Tests that Android-specific isolates are added."""
    isolate_names = self.instance.GetIsolateNames()
    for isolate in isolate_names:
      if 'telemetry_gpu_integration_test' in isolate and 'android' in isolate:
        return
    self.fail('Did not find any Android-specific isolate names')
