#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import json
import logging
import os
import StringIO
import sys
import threading
import unittest

import auto_stub

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import swarming
from utils import net


ALGO = hashlib.sha1
FILE_NAME = u'test.isolated'
FILE_HASH = u'1' * 40
TEST_NAME = u'unit_tests'
STDOUT_FOR_TRIGGER_LEN = 180


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
  return net.HttpResponse.get_fake_response(
      json.dumps(gen_data(index, shard_output, exit_codes)), 'mocked_url')


def get_swarm_results(keys):
  """Simplifies the call to yield_results().

  The timeout is hard-coded to 10 seconds.
  """
  return list(swarming.yield_results('http://host:9001', keys, 10., None))


class TestCase(auto_stub.TestCase):
  """Base class that defines the url_open mock."""
  def setUp(self):
    super(TestCase, self).setUp()
    self._lock = threading.Lock()
    self.requests = []
    self.mock(swarming.net, 'url_open', self._url_open)

  def tearDown(self):
    try:
      if not self.has_failed():
        self.assertEqual([], self.requests)
    finally:
      super(TestCase, self).tearDown()

  def _url_open(self, url, **kwargs):
    logging.info('url_open(%s)', url)
    with self._lock:
      # Ignore 'stream' argument, it's not important for these tests.
      kwargs.pop('stream')
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
    self.mock(swarming.time, 'sleep', lambda x: x)
    self.requests = [
      (
        'http://host:9001/get_matching_test_cases?name=my_test',
        {'retry_404': True},
        StringIO.StringIO('No matching Test Cases'),
      ) for _ in range(net.URL_OPEN_MAX_ATTEMPTS)
    ]
    try:
      swarming.get_test_keys('http://host:9001', 'my_test')
      self.fail()
    except swarming.Failure as e:
      msg = (
          'Error: Unable to find any tests with the name, my_test, on swarm '
          'server')
      self.assertEqual(msg, e.args[0])

  def test_no_keys_on_first_attempt(self):
    self.mock(swarming.time, 'sleep', lambda x: x)
    keys = ['key_1', 'key_2']
    self.requests = [
      (
        'http://host:9001/get_matching_test_cases?name=my_test',
        {'retry_404': True},
        StringIO.StringIO('No matching Test Cases'),
      ),
      (
        'http://host:9001/get_matching_test_cases?name=my_test',
        {'retry_404': True},
        StringIO.StringIO(json.dumps(keys)),
      ),
    ]
    actual = swarming.get_test_keys('http://host:9001', 'my_test')
    self.assertEqual(keys, actual)

  def test_find_keys(self):
    keys = ['key_1', 'key_2']
    self.requests = [
      (
        'http://host:9001/get_matching_test_cases?name=my_test',
        {'retry_404': True},
        StringIO.StringIO(json.dumps(keys)),
      ),
    ]
    actual = swarming.get_test_keys('http://host:9001', 'my_test')
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
    self.mock(logging, 'error', lambda *_: None)
    # NOTE: get_swarm_results() hardcodes timeout=10. range(12) is because of an
    # additional time.time() call deep in net.url_open().
    now = {}
    lock = threading.Lock()
    def get_now():
      t = threading.current_thread()
      with lock:
        return now.setdefault(t, range(12)).pop(0)
    self.mock(swarming.net, 'sleep_before_retry', lambda _x, _y: None)
    self.mock(swarming, 'now', get_now)
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

  def test_collect_nothing(self):
    self.mock(swarming, 'get_test_keys', lambda *_: [1, 2])
    self.mock(swarming, 'yield_results', lambda *_: [])
    self.assertEquals(
        1, swarming.collect('url', 'test_name', 'timeout', 'decorate'))

  def test_collect_success(self):
    self.mock(swarming, 'get_test_keys', lambda *_: [1, 2])
    self.mock(sys, 'stdout', StringIO.StringIO())
    data = {
      'config_instance_index': 0,
      'exit_codes': '0',
      'machine_id': 0,
      'output': 'Foo',
    }
    self.mock(swarming, 'yield_results', lambda *_: [(0, data)])
    self.assertEquals(
        0, swarming.collect('url', 'test_name', 'timeout', 'decorate'))

  def test_collect_fail(self):
    self.mock(swarming, 'get_test_keys', lambda *_: [1, 2])
    self.mock(sys, 'stdout', StringIO.StringIO())
    data = {
      'config_instance_index': 0,
      'exit_codes': '0,8',
      'machine_id': 0,
      'output': 'Foo',
    }
    self.mock(swarming, 'yield_results', lambda *_: [(0, data)])
    self.assertEquals(
        8, swarming.collect('url', 'test_name', 'timeout', 'decorate'))


def chromium_tasks(retrieval_url):
  return [
    {
      u'action': [
        u'python', u'run_isolated.zip',
        u'--hash', FILE_HASH,
        u'--isolate-server', retrieval_url,
      ],
      u'decorate_output': False,
      u'test_name': u'Run Test',
      u'time_out': 600,
    },
    {
      u'action' : [
          u'python', u'swarm_cleanup.py',
      ],
      u'decorate_output': False,
      u'test_name': u'Clean Up',
      u'time_out': 600,
    }
  ]


def generate_expected_json(
    shards,
    slave_os,
    working_dir,
    isolate_server,
    profile):
  os_value = unicode(swarming.PLATFORM_MAPPING_SWARMING[slave_os])
  expected = {
    u'cleanup': u'root',
    u'configurations': [
      {
        u'config_name': os_value,
        u'dimensions': {
          u'os': os_value,
        },
        u'min_instances': shards,
        u'priority': 101,
      },
    ],
    u'data': [],
    u'env_vars': {},
    u'restart_on_failure': True,
    u'test_case_name': TEST_NAME,
    u'tests': chromium_tasks(isolate_server),
    u'working_dir': unicode(working_dir),
  }
  if shards > 1:
    expected[u'env_vars'][u'GTEST_SHARD_INDEX'] = u'%(instance_index)s'
    expected[u'env_vars'][u'GTEST_TOTAL_SHARDS'] = u'%(num_instances)s'
  if profile:
    expected[u'tests'][0][u'action'].append(u'--verbose')
  return expected


class MockedStorage(object):
  def __init__(self, warm_cache):
    self._warm_cache = warm_cache

  def upload_items(self, items):
    return [] if self._warm_cache else items

  def get_fetch_url(self, _digest):  # pylint: disable=R0201
    return 'http://localhost:8081/fetch_url'


class ManifestTest(auto_stub.TestCase):
  def setUp(self):
    self.mock(swarming.time, 'sleep', lambda x: None)
    self.mock(sys, 'stdout', StringIO.StringIO())
    self.mock(sys, 'stderr', StringIO.StringIO())

  def tearDown(self):
    if not self.has_failed():
      self._check_output('', '')
    super(ManifestTest, self).tearDown()

  def _check_output(self, out, err):
    self.assertEqual(out, sys.stdout.getvalue())
    self.assertEqual(err, sys.stderr.getvalue())

    # Flush their content by mocking them again.
    self.mock(sys, 'stdout', StringIO.StringIO())
    self.mock(sys, 'stderr', StringIO.StringIO())

  def test_basic_manifest(self):
    manifest = swarming.Manifest(
        isolated_hash=FILE_HASH,
        test_name=TEST_NAME,
        shards=2,
        test_filter='*',
        slave_os='Windows',
        working_dir='swarm_tests',
        isolate_server='http://localhost:8081',
        verbose=False,
        profile=False,
        priority=101,
        algo=ALGO)

    swarming.chromium_setup(manifest)
    manifest_json = json.loads(manifest.to_json())

    expected = generate_expected_json(
        shards=2,
        slave_os='win32',
        working_dir='swarm_tests',
        isolate_server=u'http://localhost:8081',
        profile=False)
    self.assertEqual(expected, manifest_json)

  def test_basic_linux(self):
    """A basic linux manifest test to ensure that windows specific values
       aren't used.
    """
    manifest = swarming.Manifest(
        isolated_hash=FILE_HASH,
        test_name=TEST_NAME,
        shards=1,
        test_filter='*',
        slave_os='Linux',
        working_dir='swarm_tests',
        isolate_server='http://localhost:8081',
        verbose=False,
        profile=False,
        priority=101,
        algo=ALGO)

    swarming.chromium_setup(manifest)
    manifest_json = json.loads(manifest.to_json())

    expected = generate_expected_json(
        shards=1,
        slave_os='linux2',
        working_dir='swarm_tests',
        isolate_server=u'http://localhost:8081',
        profile=False)
    self.assertEqual(expected, manifest_json)

  def test_basic_linux_profile(self):
    manifest = swarming.Manifest(
        isolated_hash=FILE_HASH,
        test_name=TEST_NAME,
        shards=1,
        test_filter='*',
        slave_os='Linux',
        working_dir='swarm_tests',
        isolate_server='http://localhost:8081',
        verbose=False,
        profile=True,
        priority=101,
        algo=ALGO)

    swarming.chromium_setup(manifest)
    manifest_json = json.loads(manifest.to_json())

    expected = generate_expected_json(
        shards=1,
        slave_os='linux2',
        working_dir='swarm_tests',
        isolate_server=u'http://localhost:8081',
        profile=True)
    self.assertEqual(expected, manifest_json)

  def test_process_manifest_success(self):
    self.mock(swarming.net, 'url_read', lambda url, data=None: '{}')
    self.mock(swarming.isolateserver, 'get_storage',
        lambda *_: MockedStorage(warm_cache=False))

    result = swarming.process_manifest(
        file_hash_or_isolated=FILE_HASH,
        test_name=TEST_NAME,
        shards=1,
        test_filter='*',
        slave_os='linux2',
        working_dir='swarm_tests',
        isolate_server='http://localhost:8081',
        swarming='http://localhost:8082',
        verbose=False,
        profile=False,
        priority=101,
        algo=ALGO)
    self.assertEqual(0, result)

    # Just assert it printed enough, since it contains variable output.
    out = sys.stdout.getvalue()
    self.assertTrue(
        len(out) > STDOUT_FOR_TRIGGER_LEN,
        (out, sys.stderr.getvalue()))
    self.assertTrue('Upload complete' in out)
    self.mock(sys, 'stdout', StringIO.StringIO())

  def test_process_manifest_success_zip_already_uploaded(self):
    self.mock(swarming.net, 'url_read', lambda url, data=None: '{}')
    self.mock(swarming.isolateserver, 'get_storage',
        lambda *_: MockedStorage(warm_cache=True))

    result = swarming.process_manifest(
        file_hash_or_isolated=FILE_HASH,
        test_name=TEST_NAME,
        shards=1,
        test_filter='*',
        slave_os='linux2',
        working_dir='swarm_tests',
        isolate_server='http://localhost:8081',
        swarming='http://localhost:8082',
        verbose=False,
        profile=False,
        priority=101,
        algo=ALGO)
    self.assertEqual(0, result)

    # Just assert it printed enough, since it contains variable output.
    out = sys.stdout.getvalue()
    self.assertTrue(len(out) > STDOUT_FOR_TRIGGER_LEN)
    self.assertTrue('Zip file already on server' in out)
    self.mock(sys, 'stdout', StringIO.StringIO())

  def test_no_request(self):
    try:
      swarming.main([
          'trigger', '--swarming', 'https://example.com',
          '--isolate-server', 'https://example.com'])
      self.fail()
    except SystemExit as e:
      self.assertEqual(2, e.code)
      self._check_output(
          '',
          'Usage: swarming.py trigger [options]\n\n'
          'swarming.py: error: At least one --task is required.\n')


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
