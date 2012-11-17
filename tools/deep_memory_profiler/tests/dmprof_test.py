#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import logging
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import dmprof
from dmprof import FUNCTION_ADDRESS, TYPEINFO_ADDRESS


class MockSymbolCache(object):
  def __init__(self):
    self._symbol_caches = {FUNCTION_ADDRESS: {}, TYPEINFO_ADDRESS: {}}

  def add(self, address_type, address, symbol):
    self._symbol_caches[address_type][address] = symbol

  def lookup(self, address_type, address):
    symbol = self._symbol_caches[address_type].get(address)
    return symbol if symbol else '0x%016x' % address


class PolicyTest(unittest.TestCase):
  _TEST_POLICY = """{
  "components": [
    "second",
    "mmap-v8",
    "malloc-v8",
    "malloc-WebKit",
    "mmap-catch-all",
    "malloc-catch-all"
  ],
  "rules": [
    {
      "name": "second",
      "stacktrace": "optional",
      "allocator": "optional"
    },
    {
      "name": "mmap-v8",
      "stacktrace": ".*v8::.*",
      "allocator": "mmap"
    },
    {
      "name": "malloc-v8",
      "stacktrace": ".*v8::.*",
      "allocator": "malloc"
    },
    {
      "name": "malloc-WebKit",
      "stacktrace": ".*WebKit::.*",
      "allocator": "malloc"
    },
    {
      "name": "mmap-catch-all",
      "stacktrace": ".*",
      "allocator": "mmap"
    },
    {
      "name": "malloc-catch-all",
      "stacktrace": ".*",
      "allocator": "malloc"
    }
  ],
  "version": "POLICY_DEEP_3"
}
"""

  def test_load(self):
    policy = dmprof.Policy.parse(cStringIO.StringIO(self._TEST_POLICY), 'json')
    self.assertTrue(policy)
    self.assertEqual(policy.version, 'POLICY_DEEP_3')

  def test_find(self):
    policy = dmprof.Policy.parse(cStringIO.StringIO(self._TEST_POLICY), 'json')
    self.assertTrue(policy)

    symbol_cache = MockSymbolCache()
    symbol_cache.add(FUNCTION_ADDRESS, 0x1212, 'v8::create')
    symbol_cache.add(FUNCTION_ADDRESS, 0x1381, 'WebKit::create')

    bucket1 = dmprof.Bucket([0x1212, 0x013], False, 0x29492, '_Z')
    bucket1.symbolize(symbol_cache)
    bucket2 = dmprof.Bucket([0x18242, 0x1381], False, 0x9492, '_Z')
    bucket2.symbolize(symbol_cache)
    bucket3 = dmprof.Bucket([0x18242, 0x181], False, 0x949, '_Z')
    bucket3.symbolize(symbol_cache)

    self.assertEqual(policy.find(bucket1), 'malloc-v8')
    self.assertEqual(policy.find(bucket2), 'malloc-WebKit')
    self.assertEqual(policy.find(bucket3), 'malloc-catch-all')


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR,
      format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  unittest.main()
