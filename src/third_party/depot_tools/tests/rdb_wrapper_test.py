#!/usr/bin/env vpython3
# Copyright (c) 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for rdb_wrapper.py"""

from __future__ import print_function

import json
import logging
import os
import requests
import sys
import time
import unittest

if sys.version_info.major == 2:
  import mock
  BUILTIN_OPEN = '__builtin__.open'
else:
  from unittest import mock
  BUILTIN_OPEN = 'builtins.open'

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import rdb_wrapper

class TestSetupRDB(unittest.TestCase):

  def setUp(self):
    super(TestSetupRDB, self).setUp()
    mock.patch(BUILTIN_OPEN, mock.mock_open(read_data =
      '''{"result_sink":{"address": "fakeAddr","auth_token" : "p@$$w0rD"}}''')
      ).start()
    mock.patch('os.environ', {'LUCI_CONTEXT': 'dummy_file.txt'}).start()
    mock.patch('requests.post').start()
    mock.patch('time.time', side_effect=[1.0, 2.0, 3.0, 4.0, 5.0]).start()

  def test_setup_rdb(self):
    with rdb_wrapper.setup_rdb("_foobar", './my/folder/') as my_status_obj:
      self.assertEqual(my_status_obj.status, rdb_wrapper.STATUS_PASS)
      my_status_obj.status = rdb_wrapper.STATUS_FAIL

    expectedTr = {
        'testId'  : './my/folder/:_foobar',
        'status'  : rdb_wrapper.STATUS_FAIL,
        'expected': False,
        'duration': '1.000000000s'
    }

    requests.post.assert_called_once_with(
        url='http://fakeAddr/prpc/luci.resultsink.v1.Sink/ReportTestResults',
        headers={
            'Content-Type': 'application/json',
            'Accept': 'application/json',
            'Authorization': 'ResultSink p@$$w0rD'
        },
        data=json.dumps({'testResults': [expectedTr]})
    )

  def test_setup_rdb_exception(self):
    with self.assertRaises(Exception):
      with rdb_wrapper.setup_rdb("_foobar", './my/folder/'):
        raise Exception("Generic Error")

    expectedTr = {
        'testId': './my/folder/:_foobar',
        'status': rdb_wrapper.STATUS_FAIL,
        'expected': False,
        'duration': '1.000000000s'
    }

    requests.post.assert_called_once_with(
        url='http://fakeAddr/prpc/luci.resultsink.v1.Sink/ReportTestResults',
        headers={
            'Content-Type': 'application/json',
            'Accept': 'application/json',
            'Authorization': 'ResultSink p@$$w0rD'
        },
        data=json.dumps({'testResults': [expectedTr]})
    )

if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()
