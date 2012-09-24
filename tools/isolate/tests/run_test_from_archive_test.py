#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import sys
import time
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import run_test_from_archive


class RemoteTest(run_test_from_archive.Remote):
  @staticmethod
  def get_file_handler(_):
    def upload_file(item, _dest):
      if type(item) == type(Exception) and issubclass(item, Exception):
        raise item()
      elif isinstance(item, int):
        time.sleep(int(item) / 100)
    return upload_file


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

  def test_remote_no_errors(self):
    files_to_handle = 50
    remote = RemoteTest('')

    for i in range(files_to_handle):
      remote.add_item(run_test_from_archive.Remote.MED, i, i)

    for i in range(files_to_handle):
      self.assertNotEqual(-1, remote.get_result())
    self.assertEqual(None, remote.next_exception())
    remote.join()

  def test_remote_with_errors(self):
    remote = RemoteTest('')

    remote.add_item(run_test_from_archive.Remote.MED, IOError, '')
    remote.add_item(run_test_from_archive.Remote.MED, Exception, '')
    remote.join()

    self.assertNotEqual(None, remote.next_exception())
    self.assertNotEqual(None, remote.next_exception())
    self.assertEqual(None, remote.next_exception())


if __name__ == '__main__':
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.ERROR))
  unittest.main()
