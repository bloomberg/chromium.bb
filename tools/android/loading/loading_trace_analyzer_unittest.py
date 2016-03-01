# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gzip
import os
import re
import shutil
import subprocess
import tempfile
import unittest

import loading_trace_analyzer

LOADING_DIR = os.path.dirname(__file__)
TEST_DATA_DIR = os.path.join(LOADING_DIR, 'testdata')


class LoadingTraceAnalyzerTest(unittest.TestCase):
  _ROLLING_STONE = os.path.join(TEST_DATA_DIR, 'rollingstone.trace.gz')

  def setUp(self):
    self._temp_dir = tempfile.mkdtemp()
    self.trace_path = self._TmpPath('trace.json')
    with gzip.GzipFile(self._ROLLING_STONE) as f:
      with open(self.trace_path, 'w') as g:
        g.write(f.read())

  def tearDown(self):
    shutil.rmtree(self._temp_dir)

  def _TmpPath(self, name):
    return os.path.join(self._temp_dir, name)

  def testRequestsCmd(self):
    lines = [r for r in loading_trace_analyzer.ListRequests(self.trace_path)]
    self.assertNotEqual(0, len(lines))

    lines = [r for r in loading_trace_analyzer.ListRequests(self.trace_path,
        output_format='hello {protocol} world {url}')]
    self.assertNotEqual(0, len(lines))
    for line in lines:
      self.assertTrue(re.match(r'^hello \S+ world \S+$', line))

    lines = [r for r in loading_trace_analyzer.ListRequests(self.trace_path,
        where_format='{url}', where_statement=r'^http://.*$')]
    self.assertNotEqual(0, len(lines))
    for line in lines:
      self.assertTrue(line.startswith('http://'))

    lines = [r for r in loading_trace_analyzer.ListRequests(self.trace_path,
        where_format='{url}', where_statement=r'^https://.*$')]
    self.assertNotEqual(0, len(lines))
    for line in lines:
      self.assertTrue(line.startswith('https://'))


if __name__ == '__main__':
  unittest.main()
