#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import shutil
import StringIO
import sys
import tempfile
import time
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import run_isolated


class RemoteTest(run_isolated.Remote):
  def get_file_handler(self, _):  # pylint: disable=R0201
    def upload_file(item, _dest):
      if type(item) == type(Exception) and issubclass(item, Exception):
        raise item()
      elif isinstance(item, int):
        time.sleep(int(item) / 100)
    return upload_file


class RunIsolatedTest(unittest.TestCase):
  def setUp(self):
    super(RunIsolatedTest, self).setUp()
    self.tempdir = tempfile.mkdtemp(prefix='run_isolated')
    os.chdir(self.tempdir)

  def tearDown(self):
    os.chdir(ROOT_DIR)
    shutil.rmtree(self.tempdir)
    super(RunIsolatedTest, self).tearDown()

  def test_load_isolated_empty(self):
    m = run_isolated.load_isolated('{}')
    self.assertEquals({}, m)

  def test_load_isolated_good(self):
    data = {
      u'command': [u'foo', u'bar'],
      u'files': {
        u'a': {
          u'l': u'somewhere',
          u'm': 123,
          u't': 456,
        },
        u'b': {
          u'm': 123,
          u'h': u'0123456789abcdef0123456789abcdef01234567'
        }
      },
      u'includes': [u'0123456789abcdef0123456789abcdef01234567'],
      u'os': run_isolated.get_flavor(),
      u'read_only': False,
      u'relative_cwd': u'somewhere_else'
    }
    m = run_isolated.load_isolated(json.dumps(data))
    self.assertEquals(data, m)

  def test_load_isolated_bad(self):
    data = {
      u'files': {
        u'a': {
          u'l': u'somewhere',
          u'h': u'0123456789abcdef0123456789abcdef01234567'
        }
      },
    }
    try:
      run_isolated.load_isolated(json.dumps(data))
      self.fail()
    except run_isolated.ConfigError:
      pass

  def test_load_isolated_os_only(self):
    data = {
      u'os': run_isolated.get_flavor(),
    }
    m = run_isolated.load_isolated(json.dumps(data))
    self.assertEquals(data, m)

  def test_load_isolated_os_bad(self):
    data = {
      u'os': 'foo',
    }
    try:
      run_isolated.load_isolated(json.dumps(data))
      self.fail()
    except run_isolated.ConfigError:
      pass

  def test_remote_no_errors(self):
    files_to_handle = 50
    remote = RemoteTest('')

    for i in range(files_to_handle):
      remote.add_item(run_isolated.Remote.MED, i, i,
                      run_isolated.UNKNOWN_FILE_SIZE)

    for i in range(files_to_handle):
      self.assertNotEqual(-1, remote.get_result())
    self.assertEqual(None, remote.next_exception())
    remote.join()

  def test_remote_with_errors(self):
    remote = RemoteTest('')

    remote.add_item(run_isolated.Remote.MED, IOError, '',
                    run_isolated.UNKNOWN_FILE_SIZE)
    remote.add_item(run_isolated.Remote.MED, Exception, '',
                    run_isolated.UNKNOWN_FILE_SIZE)
    remote.join()

    self.assertNotEqual(None, remote.next_exception())
    self.assertNotEqual(None, remote.next_exception())
    self.assertEqual(None, remote.next_exception())

  def test_zip_header_error(self):
    old_urlopen = run_isolated.urllib2.urlopen
    try:
      run_isolated.urllib2.urlopen = lambda x : StringIO.StringIO('111')
      remote = run_isolated.Remote('https://fake-CAD.com/')

      # Both files will fail to be unzipped due to incorrect headers,
      # ensure that we don't accept the files (even if the size is unknown)}.
      remote.add_item(run_isolated.Remote.MED, 'zipped_A', 'A',
                      run_isolated.UNKNOWN_FILE_SIZE)
      remote.add_item(run_isolated.Remote.MED, 'zipped_B', 'B', 5)
      remote.join()

      self.assertNotEqual(None, remote.next_exception())
      self.assertNotEqual(None, remote.next_exception())
      self.assertEqual(None, remote.next_exception())
    finally:
      run_isolated.urllib2.urlopen = old_urlopen


if __name__ == '__main__':
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.ERROR))
  unittest.main()
