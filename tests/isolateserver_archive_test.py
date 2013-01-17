#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import isolateserver_archive


class IsolateServerTest(unittest.TestCase):
  def test_process_items(self):
    old = isolateserver_archive.update_files_to_upload
    try:
      items = {
        'foo': {'size': 12},
        'bar': {},
        'blow': {'size': 0},
        'bizz': {'size': 1222},
        'buzz': {'size': 1223},
      }
      expected = [
        ('buzz', {'size': 1223}),
        ('bizz', {'size': 1222}),
        ('foo', {'size': 12}),
        ('blow', {'size': 0}),
      ]
      actual = []
      def process(url, items, upload_func):
        self.assertEquals('FakeUrl', url)
        self.assertEquals(self.fail, upload_func)
        actual.extend(items)

      isolateserver_archive.update_files_to_upload = process
      isolateserver_archive.process_items('FakeUrl', items, self.fail)
      self.assertEqual(expected, actual)
    finally:
      isolateserver_archive.update_files_to_upload = old


if __name__ == '__main__':
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.ERROR))
  unittest.main()
