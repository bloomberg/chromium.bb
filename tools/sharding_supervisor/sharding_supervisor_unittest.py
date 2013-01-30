#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verify basic usage of sharding_supervisor."""

import difflib
import os
import subprocess
import sys
import unittest

from xml.dom import minidom

import sharding_supervisor_old as sharding_supervisor

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
SHARDING_SUPERVISOR = os.path.join(ROOT_DIR, 'sharding_supervisor.py')
DUMMY_TEST = os.path.join(ROOT_DIR, 'dummy_test.py')
NUM_CORES = sharding_supervisor.DetectNumCores()
SHARDS_PER_CORE = sharding_supervisor.SS_DEFAULT_SHARDS_PER_CORE


def generate_expected_output(start, end, num_shards):
  """Generate the expected stdout and stderr for the dummy test."""
  stdout = ''
  stderr = ''
  for i in range(start, end):
    stdout += 'Running shard %d of %d%s' % (i, num_shards, os.linesep)
  stdout += '%sALL SHARDS PASSED!%sALL TESTS PASSED!%s' % (os.linesep,
                                                           os.linesep,
                                                           os.linesep)

  return (stdout, stderr)


class ShardingSupervisorUnittest(unittest.TestCase):
  def test_basic_run(self):
    # Default test.
    expected_shards = NUM_CORES * SHARDS_PER_CORE
    (expected_out, expected_err) = generate_expected_output(
        0, expected_shards, expected_shards)
    p = subprocess.Popen([sys.executable, SHARDING_SUPERVISOR, '--no-color',
                          DUMMY_TEST], stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)

    (out, err) = p.communicate()
    self.assertEqual(expected_out, out)
    self.assertEqual(expected_err, err)
    self.assertEqual(0, p.returncode)

  def test_shard_per_core(self):
    """Test the --shards_per_core parameter."""
    expected_shards = NUM_CORES * 25
    (expected_out, expected_err) = generate_expected_output(
        0, expected_shards, expected_shards)
    p = subprocess.Popen([sys.executable, SHARDING_SUPERVISOR, '--no-color',
                          '--shards_per_core', '25', DUMMY_TEST],
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    (out, err) = p.communicate()
    self.assertEqual(expected_out, out)
    self.assertEqual(expected_err, err)
    self.assertEqual(0, p.returncode)

  def test_slave_sharding(self):
    """Test the --total-slaves and --slave-index parameters."""
    total_shards = 6
    expected_shards = NUM_CORES * SHARDS_PER_CORE * total_shards

    # Test every single index to make sure they run correctly.
    for index in range(total_shards):
      begin = NUM_CORES * SHARDS_PER_CORE * index
      end = begin + NUM_CORES * SHARDS_PER_CORE
      (expected_out, expected_err) = generate_expected_output(
          begin, end, expected_shards)
      p = subprocess.Popen([sys.executable, SHARDING_SUPERVISOR, '--no-color',
                            '--total-slaves', str(total_shards),
                            '--slave-index', str(index),
                            DUMMY_TEST],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)

      (out, err) = p.communicate()
      self.assertEqual(expected_out, out)
      self.assertEqual(expected_err, err)
      self.assertEqual(0, p.returncode)

  def test_append_to_xml(self):
    shard_xml_path = os.path.join(ROOT_DIR, 'data', 'gtest_results.xml')
    expected_xml_path = os.path.join(
        ROOT_DIR, 'data', 'gtest_results_expected.xml')
    merged_xml = sharding_supervisor.AppendToXML(None, shard_xml_path, 0)
    merged_xml = sharding_supervisor.AppendToXML(merged_xml, shard_xml_path, 1)

    with open(expected_xml_path) as expected_xml_file:
      expected_xml = minidom.parse(expected_xml_file)

    # Serialize XML to a list of strings that is consistently formatted
    # (ignoring whitespace between elements) so that it may be compared.
    def _serialize_xml(xml):
      def _remove_whitespace_and_comments(xml):
        children_to_remove = []
        for child in xml.childNodes:
          if (child.nodeType == minidom.Node.TEXT_NODE and
              not child.data.strip()):
            children_to_remove.append(child)
          elif child.nodeType == minidom.Node.COMMENT_NODE:
            children_to_remove.append(child)
          elif child.nodeType == minidom.Node.ELEMENT_NODE:
            _remove_whitespace_and_comments(child)

        for child in children_to_remove:
          xml.removeChild(child)

      _remove_whitespace_and_comments(xml)
      return xml.toprettyxml(indent='  ').splitlines()

    diff = list(difflib.unified_diff(
        _serialize_xml(expected_xml),
        _serialize_xml(merged_xml),
        fromfile='gtest_results_expected.xml',
        tofile='gtest_results_actual.xml'))
    if diff:
      self.fail('Did not merge results XML correctly:\n' + '\n'.join(diff))


if __name__ == '__main__':
  unittest.main()
