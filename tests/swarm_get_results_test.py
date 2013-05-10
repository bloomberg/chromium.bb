#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import StringIO
import sys
import threading
import unittest
import urllib2

import auto_stub

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import run_isolated
import swarm_get_results


TEST_CASE_SUCCESS = (
  '[----------] 2 tests from StaticCookiePolicyTest\n'
  '[ RUN      ] StaticCookiePolicyTest.AllowAllCookiesTest\n'
  '[       OK ] StaticCookiePolicyTest.AllowAllCookiesTest (0 ms)\n'
  '[ RUN      ] StaticCookiePolicyTest.BlockAllCookiesTest\n'
  '[       OK ] StaticCookiePolicyTest.BlockAllCookiesTest (0 ms)\n'
  '[----------] 2 tests from StaticCookiePolicyTest (0 ms total)\n'
  '\n'
  '[----------] 1 test from TCPListenSocketTest\n'
  '[ RUN      ] TCPListenSocketTest.ServerSend\n'
  '[       OK ] TCPListenSocketTest.ServerSend (1 ms)\n'
  '[----------] 1 test from TCPListenSocketTest (1 ms total)\n')


TEST_CASE_FAILURE = (
  '[----------] 2 tests from StaticCookiePolicyTest\n'
  '[ RUN      ] StaticCookiePolicyTest.AllowAllCookiesTest\n'
  '[       OK ] StaticCookiePolicyTest.AllowAllCookiesTest (0 ms)\n'
  '[ RUN      ] StaticCookiePolicyTest.BlockAllCookiesTest\n'
  'C:\\win\\build\\src\\chrome\\test.cc: error: Value of: result()\n'
  '  Actual: false\n'
  'Expected: true\n'
  '[  FAILED  ] StaticCookiePolicyTest.BlockAllCookiesTest (0 ms)\n'
  '[----------] 2 tests from StaticCookiePolicyTest (0 ms total)\n'
  '\n'
  '[----------] 1 test from TCPListenSocketTest\n'
  '[ RUN      ] TCPListenSocketTest.ServerSend\n'
  '[       OK ] TCPListenSocketTest.ServerSend (1 ms)\n'
  '[----------] 1 test from TCPListenSocketTest (1 ms total)\n')


SWARM_OUTPUT_SUCCESS = (
  '[ RUN      ] unittests.Run Test\n' +
  TEST_CASE_SUCCESS +
  '[       OK ] unittests.Run Test (2549 ms)\n'
  '[ RUN      ] unittests.Clean Up\n'
  'No output!\n'
  '[       OK ] unittests.Clean Up (6 ms)\n'
  '\n'
  '[----------] unittests summary\n'
  '[==========] 2 tests ran. (2556 ms total)\n')


SWARM_OUTPUT_FAILURE = (
  '[ RUN      ] unittests.Run Test\n' +
  TEST_CASE_FAILURE +
  '[       OK ] unittests.Run Test (2549 ms)\n'
  '[ RUN      ] unittests.Clean Up\n'
  'No output!\n'
  '[       OK ] unittests.Clean Up (6 ms)\n'
  '\n'
  '[----------] unittests summary\n'
  '[==========] 2 tests ran. (2556 ms total)\n')


SWARM_OUTPUT_WITH_NO_TEST_OUTPUT = (
  '\n'
  'Unable to connection to swarm machine.\n')


TEST_SHARD_1 = 'Note: This is test shard 1 of 3.'
TEST_SHARD_2 = 'Note: This is test shard 2 of 3.'
TEST_SHARD_3 = 'Note: This is test shard 3 of 3.'


SWARM_SHARD_OUTPUT = (
  '[ RUN      ] unittests.Run Test\n'
  '%s\n'
  '[       OK ] unittests.Run Test (2549 ms)\n'
  '[ RUN      ] unittests.Clean Up\n'
  'No output!\n'
  '[       OK ] unittests.Clean Up (6 ms)\n'
  '\n'
  '[----------] unittests summary\n'
  '[==========] 2 tests ran. (2556 ms total)\n')


TEST_SHARD_OUTPUT_1 = SWARM_SHARD_OUTPUT % TEST_SHARD_1
TEST_SHARD_OUTPUT_2 = SWARM_SHARD_OUTPUT % TEST_SHARD_2
TEST_SHARD_OUTPUT_3 = SWARM_SHARD_OUTPUT % TEST_SHARD_3


def gen_data(index, shard_output, exit_codes):
  return {
    u'config_instance_index': index,
    u'exit_codes': unicode(exit_codes),
    u'machine_id': u'host',
    u'machine_tag': u'localhost',
    u'output': unicode(shard_output),
  }


def gen_yielded_data(index, shard_output, exit_codes):
  """Returns an entry as it would be yielded by yield_results()."""
  return index, gen_data(index, shard_output, exit_codes)


def generate_url_response(index, shard_output, exit_codes):
  url_response = urllib2.addinfourl(
      StringIO.StringIO(json.dumps(gen_data(index, shard_output, exit_codes))),
      'mock message',
      'host')
  url_response.code = 200
  url_response.msg = 'OK'
  return url_response


def get_swarm_results(keys):
  """Simplifies the call to yield_results().

  The timeout is hard-coded to 10 seconds.
  """
  return list(
      swarm_get_results.yield_results(
          'http://host:9001', keys, 10., None))


class TestCase(auto_stub.TestCase):
  """Base class that defines the url_open mock."""
  def setUp(self):
    super(TestCase, self).setUp()
    self._lock = threading.Lock()
    self.requests = []
    self.mock(
        swarm_get_results.run_isolated, 'url_open',
        self._url_open)

  def tearDown(self):
    try:
      if not self.has_failed():
        self.assertEqual([], self.requests)
    finally:
      super(TestCase, self).tearDown()

  def _url_open(self, url, **kwargs):
    logging.info('url_open(%s)', url)
    with self._lock:
      # Since the client is multi-threaded, requests can be processed out of
      # order.
      for index, r in enumerate(self.requests):
        if r[0] == url and r[1] == kwargs:
          _, _, returned = self.requests.pop(index)
          break
      else:
        self.fail('Failed to find url %s' % url)
    return returned


class TestGetTestKeys(TestCase):
  def test_no_keys(self):
    old_sleep = swarm_get_results.time.sleep
    swarm_get_results.time.sleep = lambda x: x
    try:
      self.requests = [
        (
          'http://host:9001/get_matching_test_cases?name=my_test',
          {'retry_404': True, 'timeout': 0.0},
          StringIO.StringIO('No matching Test Cases'),
        ) for _ in range(run_isolated.URL_OPEN_MAX_ATTEMPTS)
      ]
      try:
        swarm_get_results.get_test_keys('http://host:9001', 'my_test', 0.)
        self.fail()
      except swarm_get_results.Failure as e:
        msg = (
            'Error: Unable to find any tests with the name, my_test, on swarm '
            'server')
        self.assertEqual(msg, e.args[0])
    finally:
      swarm_get_results.time.sleep = old_sleep

  def test_no_keys_on_first_attempt(self):
    old_sleep = swarm_get_results.time.sleep
    swarm_get_results.time.sleep = lambda x: x
    try:
      keys = ['key_1', 'key_2']
      self.requests = [
        (
          'http://host:9001/get_matching_test_cases?name=my_test',
          {'retry_404': True, 'timeout': 0.0},
          StringIO.StringIO('No matching Test Cases'),
        ),
        (
          'http://host:9001/get_matching_test_cases?name=my_test',
          {'retry_404': True, 'timeout': 0.0},
          StringIO.StringIO(json.dumps(keys)),
        ),
      ]
      actual = swarm_get_results.get_test_keys('http://host:9001', 'my_test',
                                               0.)
      self.assertEqual(keys, actual)
    finally:
      swarm_get_results.time.sleep = old_sleep

  def test_find_keys(self):
    keys = ['key_1', 'key_2']
    self.requests = [
      (
        'http://host:9001/get_matching_test_cases?name=my_test',
        {'retry_404': True, 'timeout': 0.0},
        StringIO.StringIO(json.dumps(keys)),
      ),
    ]
    actual = swarm_get_results.get_test_keys('http://host:9001', 'my_test', 0.)
    self.assertEqual(keys, actual)


class TestGetSwarmResults(TestCase):
  def test_success(self):
    self.requests = [
      (
        'http://host:9001/get_result?r=key1',
        {'retry_404': False, 'retry_50x': False},
        generate_url_response(0, SWARM_OUTPUT_SUCCESS, '0, 0'),
      ),
    ]
    expected = [gen_yielded_data(0, SWARM_OUTPUT_SUCCESS, '0, 0')]
    actual = get_swarm_results(['key1'])
    self.assertEqual(expected, actual)

  def test_failure(self):
    self.requests = [
      (
        'http://host:9001/get_result?r=key1',
        {'retry_404': False, 'retry_50x': False},
        generate_url_response(0, SWARM_OUTPUT_FAILURE, '0, 1'),
      ),
    ]
    expected = [gen_yielded_data(0, SWARM_OUTPUT_FAILURE, '0, 1')]
    actual = get_swarm_results(['key1'])
    self.assertEqual(expected, actual)

  def test_no_test_output(self):
    self.requests = [
      (
        'http://host:9001/get_result?r=key1',
        {'retry_404': False, 'retry_50x': False},
        generate_url_response(0, SWARM_OUTPUT_WITH_NO_TEST_OUTPUT, '0, 0'),
      ),
    ]
    expected = [gen_yielded_data(0, SWARM_OUTPUT_WITH_NO_TEST_OUTPUT, '0, 0')]
    actual = get_swarm_results(['key1'])
    self.assertEqual(expected, actual)

  def test_no_keys(self):
    actual = get_swarm_results([])
    self.assertEqual([], actual)

  def test_url_errors(self):
    # NOTE: get_swarm_results() hardcodes timeout=10. range(12) is because of an
    # additional time.time() call deep in run_isolated.url_open().
    now = {}
    lock = threading.Lock()
    def get_now():
      t = threading.current_thread()
      with lock:
        return now.setdefault(t, range(12)).pop(0)
    self.mock(
        swarm_get_results.run_isolated.HttpService,
        'sleep_before_retry',
        staticmethod(lambda _x, _y: None))
    self.mock(swarm_get_results, 'now', get_now)
    # The actual number of requests here depends on 'now' progressing to 10
    # seconds. It's called twice per loop.
    self.requests = [
      (
        'http://host:9001/get_result?r=key1',
        {'retry_404': False, 'retry_50x': False},
        None,
      ),
      (
        'http://host:9001/get_result?r=key1',
        {'retry_404': False, 'retry_50x': False},
        None,
      ),
      (
        'http://host:9001/get_result?r=key1',
        {'retry_404': False, 'retry_50x': False},
        None,
      ),
      (
        'http://host:9001/get_result?r=key1',
        {'retry_404': False, 'retry_50x': False},
        None,
      ),
      (
        'http://host:9001/get_result?r=key1',
        {'retry_404': False, 'retry_50x': False},
        None,
      ),
    ]
    actual = get_swarm_results(['key1'])
    self.assertEqual([], actual)
    self.assertTrue(all(not v for v in now.itervalues()), now)

  def test_shard_repeated(self):
    self.requests = [
      (
        'http://host:9001/get_result?r=key1',
        {'retry_404': False, 'retry_50x': False},
        generate_url_response(0, SWARM_OUTPUT_SUCCESS, '0, 0'),
      ),
      (
        'http://host:9001/get_result?r=key1-repeat',
        {'retry_404': False, 'retry_50x': False},
        generate_url_response(0, SWARM_OUTPUT_SUCCESS, '0, 0'),
      ),
    ]
    expected = [gen_yielded_data(0, SWARM_OUTPUT_SUCCESS, '0, 0')]
    actual = get_swarm_results(['key1', 'key1-repeat'])
    self.assertEqual(expected, actual)

  def test_one_shard_repeated(self):
    """Have shard 1 repeated twice, then shard 2 and 3."""
    self.requests = [
      (
        'http://host:9001/get_result?r=key1',
        {'retry_404': False, 'retry_50x': False},
        generate_url_response(0, TEST_SHARD_OUTPUT_1, '0, 0'),
      ),
      (
        'http://host:9001/get_result?r=key1-repeat',
        {'retry_404': False, 'retry_50x': False},
        generate_url_response(0, TEST_SHARD_OUTPUT_1, '0, 0'),
      ),
      (
        'http://host:9001/get_result?r=key2',
        {'retry_404': False, 'retry_50x': False},
        generate_url_response(1, TEST_SHARD_OUTPUT_2, '0, 0'),
      ),
      (
        'http://host:9001/get_result?r=key3',
        {'retry_404': False, 'retry_50x': False},
        generate_url_response(2, TEST_SHARD_OUTPUT_3, '0, 0'),
      ),
    ]
    expected = [
      gen_yielded_data(0, TEST_SHARD_OUTPUT_1, '0, 0'),
      gen_yielded_data(1, TEST_SHARD_OUTPUT_2, '0, 0'),
      gen_yielded_data(2, TEST_SHARD_OUTPUT_3, '0, 0'),
    ]
    actual = get_swarm_results(['key1', 'key1-repeat', 'key2', 'key3'])
    self.assertEqual(expected, sorted(actual))


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
