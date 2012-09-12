#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import run_test_from_archive


class RunTestFromArchiveTest(unittest.TestCase):
  def test_load_manifest_empty(self):
    m = run_test_from_archive.load_manifest('{}')
    self.assertEquals({}, m)

  def test_load_manifest_good(self):
    data = {
      u'command': [u'foo', u'bar'],
      u'files': {
        u'a': {
          u'link': u'somewhere',
          u'mode': 123,
          u'timestamp': 456,
        },
        u'b': {
          u'mode': 123,
          u'sha-1': u'0123456789abcdef0123456789abcdef01234567'
        }
      },
      u'includes': [u'0123456789abcdef0123456789abcdef01234567'],
      u'os': run_test_from_archive.get_flavor(),
      u'read_only': False,
      u'relative_cwd': u'somewhere_else'
    }
    m = run_test_from_archive.load_manifest(json.dumps(data))
    self.assertEquals(data, m)

  def test_load_manifest_bad(self):
    data = {
      u'files': {
        u'a': {
          u'link': u'somewhere',
          u'sha-1': u'0123456789abcdef0123456789abcdef01234567'
        }
      },
    }
    try:
      run_test_from_archive.load_manifest(json.dumps(data))
      self.fail()
    except run_test_from_archive.ConfigError:
      pass

  def test_load_manifest_os_only(self):
    data = {
      u'os': run_test_from_archive.get_flavor(),
    }
    m = run_test_from_archive.load_manifest(json.dumps(data))
    self.assertEquals(data, m)

  def test_load_manifest_os_bad(self):
    data = {
      u'os': 'foo',
    }
    try:
      run_test_from_archive.load_manifest(json.dumps(data))
      self.fail()
    except run_test_from_archive.ConfigError:
      pass


if __name__ == '__main__':
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.ERROR))
  unittest.main()
