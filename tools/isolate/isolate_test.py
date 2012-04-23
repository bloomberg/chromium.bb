#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import tempfile
import unittest

import isolate

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


class Isolate(unittest.TestCase):
  def setUp(self):
    # Everything should work even from another directory.
    os.chdir(os.path.dirname(ROOT_DIR))

  def _run_process_options(self, values, variables, more_expected_data):
    """Runs isolate.process_options() and verify the results."""
    fd, temp_path = tempfile.mkstemp()
    try:
      # Reuse the file descriptor. On windows, it needs to be closed before
      # process_options() opens it, because mkstemp() opens it without file
      # sharing enabled.
      with os.fdopen(fd, 'w') as f:
        json.dump(values, f)
      root_dir, infiles, data = isolate.process_options(
          variables,
          temp_path,
          os.path.join('isolate', 'data', 'isolate', 'touch_root.isolate'),
          self.fail)
    finally:
      os.remove(temp_path)

    expected_data = {
      u'command': ['python', 'touch_root.py'],
      u'read_only': None,
      u'relative_cwd': 'data/isolate',
      u'resultfile': temp_path,
      u'resultdir': tempfile.gettempdir(),
      u'variables': {},
    }
    expected_data.update(more_expected_data)
    expected_files = sorted(
        ('isolate.py', os.path.join('data', 'isolate', 'touch_root.py')))
    self.assertEquals(ROOT_DIR, root_dir)
    self.assertEquals(expected_files, sorted(infiles))
    self.assertEquals(expected_data, data)

  def test_load_empty(self):
    content = "{}"
    variables = {}
    command, infiles, read_only = isolate.load_isolate(
        content, variables, self.fail)
    self.assertEquals([], command)
    self.assertEquals([], infiles)
    self.assertEquals(None, read_only)

  def test_process_options_empty(self):
    # Passing nothing generates nothing unexpected.
    self._run_process_options({}, {}, {})

  def test_process_options(self):
    # The previous unexpected variables are kept, the 'variables' dictionary is
    # updated.
    values = {
      'command': 'maybe',
      'foo': 'bar',
      'read_only': 2,
      'relative_cwd': None,
      'resultdir': '2',
      'resultfile': [],
      'variables': {
        'unexpected': 'seriously',
      },
    }

    expected_data = {
      u'foo': u'bar',
      u'variables': {
        'expected': 'very',
        u'unexpected': u'seriously',
      },
    }
    self._run_process_options(values, {'expected': 'very'}, expected_data)


if __name__ == '__main__':
  unittest.main()
