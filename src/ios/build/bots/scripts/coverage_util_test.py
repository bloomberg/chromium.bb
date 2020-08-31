# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests of coverage_util functions."""

import os
import shutil
import unittest

import coverage_util
import test_runner_test


class TestCoverageUtil(test_runner_test.TestCase):
  """Test cases for coverage_util.py"""

  def create_origin_profraw_file_if_not_exist(self):
    """Creates the profraw file in the correct udid data folder to move if it
    doesn't exist
    """
    if not os.path.exists(self.origin_profraw_file_path):
      with open(self.origin_profraw_file_path, 'w') as outfile:
        outfile.write("Some raw coverage data.\n")

  def setUp(self):
    super(TestCoverageUtil, self).setUp()

    self.test_folder = os.path.join(os.getcwd(), "coverage_util_test_data")
    self.simulators_folder = os.path.join(self.test_folder, "Devices")
    self.existing_udid = "existing-udid"
    self.existing_udid_folder = os.path.join(self.simulators_folder,
                                             "existing-udid")
    self.existing_udid_data_folder = os.path.join(self.simulators_folder,
                                                  self.existing_udid, "data")
    if not os.path.exists(self.existing_udid_data_folder):
      os.makedirs(self.existing_udid_data_folder)

    self.profraw_file_name = "default.profraw"
    self.origin_profraw_file_path = os.path.join(self.existing_udid_data_folder,
                                                 self.profraw_file_name)

    self.not_existing_udid = "not-existing-udid"
    self.not_existing_udid_data_folder = os.path.join(
        self.simulators_folder, self.not_existing_udid, "data")
    if os.path.exists(self.not_existing_udid_data_folder):
      shutil.rmtree(self.not_existing_udid_data_folder)

    self.output_folder = os.path.join(self.test_folder, "output")
    if not os.path.exists(self.output_folder):
      os.makedirs(self.output_folder)

    self.expected_profraw_output_path = os.path.join(
        self.output_folder, "profraw", self.profraw_file_name)

    self.mock(coverage_util, 'SIMULATORS_FOLDER', self.simulators_folder)

  def tearDown(self):
    shutil.rmtree(self.test_folder)

  def test_move_raw_coverage_data(self):
    """Tests if coverage_util can correctly move raw coverage data"""
    self.create_origin_profraw_file_if_not_exist()
    self.assertTrue(os.path.exists(self.origin_profraw_file_path))
    self.assertFalse(os.path.exists(self.expected_profraw_output_path))
    coverage_util.move_raw_coverage_data(self.existing_udid, self.output_folder)
    self.assertFalse(os.path.exists(self.origin_profraw_file_path))
    self.assertTrue(os.path.exists(self.expected_profraw_output_path))
    os.remove(self.expected_profraw_output_path)

  def test_move_raw_coverage_data_origin_not_exist(self):
    """Ensures that coverage_util won't break when raw coverage data folder or
    file doesn't exist
    """
    # Tests origin directory doesn't exist.
    coverage_util.move_raw_coverage_data(self.not_existing_udid,
                                         self.output_folder)
    self.assertFalse(os.path.exists(self.expected_profraw_output_path))

    # Tests profraw file doesn't exist.
    if os.path.exists(self.origin_profraw_file_path):
      os.remove(self.origin_profraw_file_path)
    self.assertFalse(os.path.exists(self.origin_profraw_file_path))
    self.assertFalse(os.path.exists(self.expected_profraw_output_path))
    coverage_util.move_raw_coverage_data(self.existing_udid, self.output_folder)
    self.assertFalse(os.path.exists(self.expected_profraw_output_path))


if __name__ == '__main__':
  unittest.main()
