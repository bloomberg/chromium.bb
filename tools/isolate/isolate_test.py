#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import unittest

import isolate

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


class Isolate(unittest.TestCase):
  def setUp(self):
    # Everything should work even from another directory.
    os.chdir(os.path.dirname(ROOT_DIR))

  def test_load_isolate_empty(self):
    content = "{}"
    command, infiles, read_only = isolate.load_isolate(
        content, self.fail)
    self.assertEquals([], command)
    self.assertEquals([], infiles)
    self.assertEquals(None, read_only)

  def test_result_load_empty(self):
    values = {
    }
    expected = {
      'command': [],
      'files': {},
      'read_only': None,
      'relative_cwd': None,
    }
    self.assertEquals(expected, isolate.Result.load(values).flatten())

  def test_result_load(self):
    values = {
      'command': 'maybe',
      'files': {'foo': 42},
      'read_only': 2,
      'relative_cwd': None,
    }
    expected = {
      'command': 'maybe',
      'files': {'foo': 42},
      'read_only': 2,
      'relative_cwd': None,
    }
    self.assertEquals(expected, isolate.Result.load(values).flatten())

  def test_result_load_unexpected(self):
    values = {
      'foo': 'bar',
    }
    try:
      isolate.Result.load(values)
      self.fail()
    except AssertionError:
      pass

  def test_savedstate_load_empty(self):
    values = {
    }
    expected = {
      'isolate_file': None,
      'variables': {},
    }
    self.assertEquals(expected, isolate.SavedState.load(values).flatten())

  def test_savedstate_load(self):
    values = {
      'isolate_file': 'maybe',
      'variables': {'foo': 42},
    }
    expected = {
      'isolate_file': 'maybe',
      'variables': {'foo': 42},
    }
    self.assertEquals(expected, isolate.SavedState.load(values).flatten())


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR,
      format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  unittest.main()
