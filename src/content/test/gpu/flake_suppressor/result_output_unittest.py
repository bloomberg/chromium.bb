#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import tempfile
import unittest

from pyfakefs import fake_filesystem_unittest

from flake_suppressor import result_output


class GenerateHtmlOutputFileUntitest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self.output_file = tempfile.NamedTemporaryFile(mode='w', delete=False)
    self.output_file_name = self.output_file.name

  def testBasic(self):
    """Basic functionality test."""
    result_map = {
        'some_suite': {
            'some_test': {
                'some_tags': {
                    'typ_tags': ['some', 'tags'],
                    'build_url_list': ['url1', 'url2'],
                },
            },
        },
    }
    result_output.GenerateHtmlOutputFile(result_map, self.output_file)
    expected_output = """\
<html>
<body>
<h1>Grouped By Test</h1>
<ul>
<li>some_suite</li>
<ul>
<li>some_test</li>
<ul>
<li>some tags</li>
<ul>
<li><a href="url1">url1</a></li>
<li><a href="url2">url2</a></li>
</ul>
</ul>
</ul>
</ul>
<h1>Grouped By Config</h1>
<ul>
<li>some tags</li>
<ul>
<li>some_suite</li>
<ul>
<li>some_test</li>
<ul>
<li><a href="url1">url1</a></li>
<li><a href="url2">url2</a></li>
</ul>
</ul>
</ul>
</ul>
</body>
</html>
"""
    with open(self.output_file_name) as infile:
      self.assertEqual(infile.read(), expected_output)


class RecursiveHtmlToFileUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self.output_file = tempfile.NamedTemporaryFile(mode='w', delete=False)
    self.output_file_name = self.output_file.name

  def testBasic(self):
    """Basic functionality test."""
    string_map = {
        'some_suite': {
            'some_test': {
                'some tags': ['url1', 'url2'],
            },
        },
    }
    result_output._RecursiveHtmlToFile(string_map, self.output_file)
    self.output_file.close()
    expected_output = """\
<li>some_suite</li>
<ul>
<li>some_test</li>
<ul>
<li>some tags</li>
<ul>
<li><a href="url1">url1</a></li>
<li><a href="url2">url2</a></li>
</ul>
</ul>
</ul>
"""
    with open(self.output_file_name) as infile:
      self.assertEqual(infile.read(), expected_output)

  def testUnsupportedType(self):
    """Tests that providing an unsupported data type fails."""
    with self.assertRaises(RuntimeError):
      result_output._RecursiveHtmlToFile('a', self.output_file)


class ConvertAggregatedResultsToStringMapUnittest(unittest.TestCase):
  def testBasic(self):
    """Basic functionality test."""
    result_map = {
        'some_suite': {
            'some_test': {
                'some_tags': {
                    'typ_tags': ['some', 'tags'],
                    'build_url_list': ['url1', 'url2'],
                },
            },
        },
    }
    expected_map = {
        'some_suite': {
            'some_test': {
                'some tags': ['url1', 'url2'],
            },
        },
    }
    self.assertEqual(
        result_output._ConvertAggregatedResultsToStringMap(result_map),
        expected_map)


class ConvertFromTestGroupingToConfigGroupingUnittest(unittest.TestCase):
  def testBasic(self):
    """Basic functionality test."""
    string_map = {
        'some_suite': {
            'some_test': {
                'some tags': ['url1', 'url2'],
            },
        },
    }
    config_map = result_output._ConvertFromTestGroupingToConfigGrouping(
        string_map)
    expected_config_map = {
        'some tags': {
            'some_suite': {
                'some_test': ['url1', 'url2'],
            },
        },
    }
    self.assertEqual(config_map, expected_config_map)


if __name__ == '__main__':
  unittest.main(verbosity=2)
