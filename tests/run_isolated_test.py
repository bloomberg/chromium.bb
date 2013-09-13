#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import shutil
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import auto_stub
import isolateserver
import run_isolated


class RunIsolatedTest(auto_stub.TestCase):
  def setUp(self):
    super(RunIsolatedTest, self).setUp()
    self.tempdir = tempfile.mkdtemp(prefix='run_isolated')
    os.chdir(self.tempdir)
    # Mock out file_write() so nothing it written to disk.
    self.mock(isolateserver, 'file_write', lambda y, x: list(x))

  def tearDown(self):
    os.chdir(ROOT_DIR)
    shutil.rmtree(self.tempdir)
    super(RunIsolatedTest, self).tearDown()

  def test_load_isolated_empty(self):
    m = run_isolated.load_isolated('{}')
    self.assertEqual({}, m)

  def test_load_isolated_good(self):
    data = {
      u'command': [u'foo', u'bar'],
      u'files': {
        u'a': {
          u'l': u'somewhere',
          u'm': 123,
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
    self.assertEqual(data, m)

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
    self.assertEqual(data, m)

  def test_load_isolated_os_bad(self):
    data = {
      u'os': 'foo',
    }
    try:
      run_isolated.load_isolated(json.dumps(data))
      self.fail()
    except run_isolated.ConfigError:
      pass

  def test_zip_header_error(self):
    self.mock(
        isolateserver.net, 'url_open',
        lambda url, **_: isolateserver.net.HttpResponse.get_fake_response(
            '111', url))
    self.mock(run_isolated.time, 'sleep', lambda _x: None)

    retriever = isolateserver.get_storage_api(
        'https://fake-CAD.com/', 'namespace')
    remote = isolateserver.RemoteOperation(retriever.retrieve)

    # Both files will fail to be unzipped due to incorrect headers,
    # ensure that we don't accept the files (even if the size is unknown)}.
    remote.add_item(isolateserver.RemoteOperation.MED, 'zipped_A', 'A',
                    isolateserver.UNKNOWN_FILE_SIZE)
    remote.add_item(isolateserver.RemoteOperation.MED, 'zipped_B', 'B', 5)
    self.assertRaises(IOError, remote.get_one_result)
    self.assertRaises(IOError, remote.get_one_result)
    # Need to use join here, since get_one_result will hang.
    self.assertEqual([], remote.join())


if __name__ == '__main__':
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.FATAL))
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
