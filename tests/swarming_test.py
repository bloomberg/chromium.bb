#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import datetime
import hashlib
import json
import logging
import os
import StringIO
import subprocess
import sys
import tempfile
import threading
import time
import unittest

# net_utils adjusts sys.path.
import net_utils

from depot_tools import auto_stub

import auth
import isolateserver
import swarming
import test_utils

from utils import file_path
from utils import tools

import isolateserver_mock


FILE_HASH = u'1' * 40
TEST_NAME = u'unit_tests'


OUTPUT = 'Ran stuff\n'

SHARD_OUTPUT_1 = 'Shard 1 of 3.'
SHARD_OUTPUT_2 = 'Shard 2 of 3.'
SHARD_OUTPUT_3 = 'Shard 3 of 3.'


def gen_yielded_data(index, **kwargs):
  """Returns an entry as it would be yielded by yield_results()."""
  return index, gen_result_response(**kwargs)


def get_results(keys, output_collector=None):
  """Simplifies the call to yield_results().

  The timeout is hard-coded to 10 seconds.
  """
  return list(
      swarming.yield_results(
          'https://host:9001', keys, 10., None, True, output_collector))


def collect(url, task_name, task_ids):
  """Simplifies the call to swarming.collect()."""
  return swarming.collect(
    swarming=url,
    task_name=task_name,
    task_ids=task_ids,
    timeout=10,
    decorate=True,
    print_status_updates=True,
    task_summary_json=None,
    task_output_dir=None)


def main(args):
  """Bypasses swarming.main()'s exception handling.

  It gets in the way when debugging test failures.
  """
  dispatcher = swarming.subcommand.CommandDispatcher('swarming')
  return dispatcher.execute(swarming.OptionParserSwarming(), args)


def gen_request_data(isolated_hash=FILE_HASH, properties=None, **kwargs):
  out = {
    'name': u'unit_tests',
    'parent_task_id': '',
    'priority': 101,
    'properties': {
      'commands': [
        [
          'python',
          'run_isolated.zip',
          '--isolated', isolated_hash,
          '--isolate-server', 'https://localhost:2',
          '--namespace', 'default-gzip',
          '--',
          '--some-arg',
          '123',
        ],
      ],
      'data': [('https://localhost:1/fetch_url', 'swarm_data.zip')],
      'dimensions': {
        'foo': 'bar',
        'os': 'Mac',
      },
      'env': {},
      'execution_timeout_secs': 60,
      'idempotent': False,
      'io_timeout_secs': 60,
    },
    'scheduling_expiration_secs': 3600,
    'tags': ['taga', 'tagb'],
    'user': 'joe@localhost',
  }
  out.update(kwargs)
  out['properties'].update(properties or {})
  return out


def gen_request_response(request, **kwargs):
  # As seen in services/swarming/handlers_api.py.
  out = {
    'request': request.copy(),
    'task_id': '12300',
  }
  out.update(kwargs)
  return out


def gen_result_response(**kwargs):
  out = {
    "abandoned_ts": None,
    "bot_id": "swarm6",
    "completed_ts": "2014-09-24 13:49:16",
    "created_ts": "2014-09-24 13:49:03",
    "durations": [0.9636809825897217, 0.8754310607910156],
    "exit_codes": [0, 0],
    "failure": False,
    "id": "10100",
    "internal_failure": False,
    "modified_ts": "2014-09-24 13:49:17",
    "name": "heartbeat-canary-2014-09-24_13:49:01-os=Linux",
    "started_ts": "2014-09-24 13:49:09",
    "state": 112,
    "try_number": 1,
    "user": "unknown",
  }
  out.update(kwargs)
  return out


def gen_run_isolated_out_hack_log(isolate_server, namespace, isolated_hash):
  data = {
    'hash': isolated_hash,
    'namespace': namespace,
    'storage': isolate_server,
  }
  return (OUTPUT +
      '[run_isolated_out_hack]%s[/run_isolated_out_hack]\n' % (
          json.dumps(data, sort_keys=True, separators=(',',':'))))


# Silence pylint 'Access to a protected member _Event of a client class'.
class NonBlockingEvent(threading._Event):  # pylint: disable=W0212
  """Just like threading.Event, but a class and ignores timeout in 'wait'.

  Intended to be used as a mock for threading.Event in tests.
  """

  def wait(self, timeout=None):
    return super(NonBlockingEvent, self).wait(0)


class NetTestCase(net_utils.TestCase):
  """Base class that defines the url_open mock."""
  def setUp(self):
    super(NetTestCase, self).setUp()
    self._tempdir = None
    self.mock(auth, 'ensure_logged_in', lambda _: None)
    self.mock(time, 'sleep', lambda _: None)
    self.mock(subprocess, 'call', lambda *_: self.fail())
    self.mock(threading, 'Event', NonBlockingEvent)
    self.mock(sys, 'stdout', StringIO.StringIO())
    self.mock(sys, 'stderr', StringIO.StringIO())

  def tearDown(self):
    try:
      if self._tempdir:
        file_path.rmtree(self._tempdir)
      if not self.has_failed():
        self._check_output('', '')
    finally:
      super(NetTestCase, self).tearDown()

  @property
  def tempdir(self):
    """Creates the directory on first reference."""
    if not self._tempdir:
      self._tempdir = tempfile.mkdtemp(prefix='swarming_test')
    return self._tempdir

  def _check_output(self, out, err):
    self.assertEqual(
        out.splitlines(True), sys.stdout.getvalue().splitlines(True))
    self.assertEqual(
        err.splitlines(True), sys.stderr.getvalue().splitlines(True))

    # Flush their content by mocking them again.
    self.mock(sys, 'stdout', StringIO.StringIO())
    self.mock(sys, 'stderr', StringIO.StringIO())


class TestIsolated(auto_stub.TestCase):
  """Test functions with isolated_ prefix."""
  def setUp(self):
    super(TestIsolated, self).setUp()
    self._server = None

  def tearDown(self):
    try:
      if self._server:
        self._server.close_start()
      if self._server:
        self._server.close_end()
    finally:
      super(TestIsolated, self).tearDown()

  @property
  def server(self):
    """Creates the Isolate Server mock on first reference."""
    if not self._server:
      self._server = isolateserver_mock.MockIsolateServer()
    return self._server

  # Test isolated related code.
  def test_isolated_get_data(self):
    data = swarming.isolated_get_data(self.server.url)
    self.assertEqual(['default'], self.server.contents.keys())
    self.assertEqual(1, len(self.server.contents['default']))
    h = self.server.contents['default'].popitem()[0]
    expected = [
      (
        '%s/content-gs/retrieve/default/%s' % (self.server.url, h),
        'swarm_data.zip',
      ),
    ]
    self.assertEqual(expected, data)

  def test_isolated_get_run_commands(self):
    actual = swarming.isolated_get_run_commands(
        'http://foo.invalid', 'default', '1'*40, ['fo', 'ba'], True)
    expected = [
      'python',
      'run_isolated.zip',
      '--isolated', '1111111111111111111111111111111111111111',
      '--isolate-server', 'http://foo.invalid',
      '--namespace', 'default',
      '--verbose',
      '--',
      'fo',
      'ba',
    ]
    self.assertEqual(expected, actual)


class TestSwarmingTrigger(NetTestCase):
  def test_trigger_task_shards_2_shards(self):
    task_request = swarming.TaskRequest(
        command=['a', 'b'],
        data=[],
        dimensions={'foo': 'bar', 'os': 'Mac'},
        env={},
        expiration=60*60,
        hard_timeout=60,
        idempotent=False,
        io_timeout=60,
        name=TEST_NAME,
        priority=101,
        tags=['taga', 'tagb'],
        user='joe@localhost',
        verbose=False)

    request_1 = swarming.task_request_to_raw_request(task_request)
    request_1['name'] = u'unit_tests:0:2'
    request_1['properties']['env'] = {
      'GTEST_SHARD_INDEX': '0', 'GTEST_TOTAL_SHARDS': '2',
    }
    result_1 = gen_request_response(request_1)

    request_2 = swarming.task_request_to_raw_request(task_request)
    request_2['name'] = u'unit_tests:1:2'
    request_2['properties']['env'] = {
      'GTEST_SHARD_INDEX': '1', 'GTEST_TOTAL_SHARDS': '2',
    }
    result_2 = gen_request_response(request_2, task_id='12400')
    self.expected_requests(
        [
          (
            'https://localhost:1/swarming/api/v1/client/handshake',
            {'data': {}, 'headers': {'X-XSRF-Token-Request': '1'}},
            {'server_version': 'v1', 'xsrf_token': 'Token'},
          ),
          (
            'https://localhost:1/swarming/api/v1/client/request',
            {'data': request_1, 'headers': {'X-XSRF-Token': 'Token'}},
            result_1,
          ),
          (
            'https://localhost:1/swarming/api/v1/client/request',
            {'data': request_2, 'headers': {'X-XSRF-Token': 'Token'}},
            result_2,
          ),
        ])

    tasks = swarming.trigger_task_shards(
        swarming='https://localhost:1',
        task_request=task_request,
        shards=2)
    expected = {
      u'unit_tests:0:2': {
        'shard_index': 0,
        'task_id': '12300',
        'view_url': 'https://localhost:1/user/task/12300',
      },
      u'unit_tests:1:2': {
        'shard_index': 1,
        'task_id': '12400',
        'view_url': 'https://localhost:1/user/task/12400',
      },
    }
    self.assertEqual(expected, tasks)

  def test_trigger_task_shards_priority_override(self):
    task_request = swarming.TaskRequest(
        command=['a', 'b'],
        data=[['https://foo.invalid/bar', 'bar.zip']],
        dimensions={'foo': 'bar', 'os': 'Mac'},
        env={},
        expiration=60*60,
        hard_timeout=60,
        idempotent=False,
        io_timeout=60,
        name=TEST_NAME,
        priority=101,
        tags=['taga', 'tagb'],
        user='joe@localhost',
        verbose=False)

    os.environ['SWARMING_TASK_ID'] = '123'
    try:
      request = swarming.task_request_to_raw_request(task_request)
    finally:
      os.environ.pop('SWARMING_TASK_ID')
    self.assertEqual('123', request['parent_task_id'])

    result = gen_request_response(request)
    result['request']['priority'] = 200
    self.expected_requests(
        [
          (
            'https://localhost:1/swarming/api/v1/client/handshake',
            {'data': {}, 'headers': {'X-XSRF-Token-Request': '1'}},
            {'server_version': 'v1', 'xsrf_token': 'Token'},
          ),
          (
            'https://localhost:1/swarming/api/v1/client/request',
            {'data': request, 'headers': {'X-XSRF-Token': 'Token'}},
            result,
          ),
        ])

    os.environ['SWARMING_TASK_ID'] = '123'
    try:
      tasks = swarming.trigger_task_shards(
          swarming='https://localhost:1',
          shards=1,
          task_request=task_request)
    finally:
      os.environ.pop('SWARMING_TASK_ID')
    expected = {
      u'unit_tests': {
        'shard_index': 0,
        'task_id': '12300',
        'view_url': 'https://localhost:1/user/task/12300',
      }
    }
    self.assertEqual(expected, tasks)
    self._check_output('', 'Priority was reset to 200\n')

  def test_isolated_to_hash(self):
    calls = []
    self.mock(subprocess, 'call', lambda *c: calls.append(c))
    content = '{}'
    expected_hash = hashlib.sha1(content).hexdigest()
    handle, isolated = tempfile.mkstemp(
        prefix='swarming_test_', suffix='.isolated')
    os.close(handle)
    try:
      with open(isolated, 'w') as f:
        f.write(content)
      hash_value, is_file = swarming.isolated_to_hash(
          'https://localhost:2', 'default-gzip', isolated, hashlib.sha1, False)
    finally:
      os.remove(isolated)
    self.assertEqual(expected_hash, hash_value)
    self.assertEqual(True, is_file)
    expected_calls = [
        (
          [
            sys.executable,
            os.path.join(swarming.ROOT_DIR, 'isolate.py'),
            'archive',
            '--isolate-server', 'https://localhost:2',
            '--namespace', 'default-gzip',
            '--isolated',
            isolated,
          ],
          False,
        ),
    ]
    self.assertEqual(expected_calls, calls)
    self._check_output('Archiving: %s\n' % isolated, '')


class TestSwarmingCollection(NetTestCase):
  def test_success(self):
    self.expected_requests(
        [
          (
            'https://host:9001/swarming/api/v1/client/task/10100',
            {'retry_50x': False},
            gen_result_response(),
          ),
          (
            'https://host:9001/swarming/api/v1/client/task/10100/output/all',
            {},
            {'outputs': [OUTPUT]},
          ),
        ])
    expected = [gen_yielded_data(0, outputs=[OUTPUT])]
    actual = get_results(['10100'])
    self.assertEqual(expected, actual)

  def test_failure(self):
    self.expected_requests(
        [
          (
            'https://host:9001/swarming/api/v1/client/task/10100',
            {'retry_50x': False},
            gen_result_response(exit_codes=[0, 1]),
          ),
          (
            'https://host:9001/swarming/api/v1/client/task/10100/output/all',
            {},
            {'outputs': [OUTPUT]},
          ),
        ])
    expected = [gen_yielded_data(0, outputs=[OUTPUT], exit_codes=[0, 1])]
    actual = get_results(['10100'])
    self.assertEqual(expected, actual)

  def test_no_ids(self):
    actual = get_results([])
    self.assertEqual([], actual)

  def test_url_errors(self):
    self.mock(logging, 'error', lambda *_, **__: None)
    # NOTE: get_results() hardcodes timeout=10.
    now = {}
    lock = threading.Lock()
    def get_now():
      t = threading.current_thread()
      with lock:
        return now.setdefault(t, range(10)).pop(0)
    self.mock(swarming.net, 'sleep_before_retry', lambda _x, _y: None)
    self.mock(swarming, 'now', get_now)
    # The actual number of requests here depends on 'now' progressing to 10
    # seconds. It's called once per loop. Loop makes 9 iterations.
    self.expected_requests(
        9 * [
          (
            'https://host:9001/swarming/api/v1/client/task/10100',
            {'retry_50x': False},
            None,
          )
        ])
    actual = get_results(['10100'])
    self.assertEqual([], actual)
    self.assertTrue(all(not v for v in now.itervalues()), now)

  def test_many_shards(self):
    self.expected_requests(
        [
          (
            'https://host:9001/swarming/api/v1/client/task/10100',
            {'retry_50x': False},
            gen_result_response(),
          ),
          (
            'https://host:9001/swarming/api/v1/client/task/10100/output/all',
            {},
            {'outputs': [SHARD_OUTPUT_1]},
          ),
          (
            'https://host:9001/swarming/api/v1/client/task/10200',
            {'retry_50x': False},
            gen_result_response(),
          ),
          (
            'https://host:9001/swarming/api/v1/client/task/10200/output/all',
            {},
            {'outputs': [SHARD_OUTPUT_2]},
          ),
          (
            'https://host:9001/swarming/api/v1/client/task/10300',
            {'retry_50x': False},
            gen_result_response(),
          ),
          (
            'https://host:9001/swarming/api/v1/client/task/10300/output/all',
            {},
            {'outputs': [SHARD_OUTPUT_3]},
          ),
        ])
    expected = [
      gen_yielded_data(0, outputs=[SHARD_OUTPUT_1]),
      gen_yielded_data(1, outputs=[SHARD_OUTPUT_2]),
      gen_yielded_data(2, outputs=[SHARD_OUTPUT_3]),
    ]
    actual = get_results(['10100', '10200', '10300'])
    self.assertEqual(expected, sorted(actual))

  def test_output_collector_called(self):
    # Three shards, one failed. All results are passed to output collector.
    self.expected_requests(
        [
          (
            'https://host:9001/swarming/api/v1/client/task/10100',
            {'retry_50x': False},
            gen_result_response(),
          ),
          (
            'https://host:9001/swarming/api/v1/client/task/10100/output/all',
            {},
            {'outputs': [SHARD_OUTPUT_1]},
          ),
          (
            'https://host:9001/swarming/api/v1/client/task/10200',
            {'retry_50x': False},
            gen_result_response(),
          ),
          (
            'https://host:9001/swarming/api/v1/client/task/10200/output/all',
            {},
            {'outputs': [SHARD_OUTPUT_2]},
          ),
          (
            'https://host:9001/swarming/api/v1/client/task/10300',
            {'retry_50x': False},
            gen_result_response(exit_codes=[0, 1]),
          ),
          (
            'https://host:9001/swarming/api/v1/client/task/10300/output/all',
            {},
            {'outputs': [SHARD_OUTPUT_3]},
          ),
        ])

    class FakeOutputCollector(object):
      def __init__(self):
        self.results = []
        self._lock = threading.Lock()

      def process_shard_result(self, index, result):
        with self._lock:
          self.results.append((index, result))

    output_collector = FakeOutputCollector()
    get_results(['10100', '10200', '10300'], output_collector)

    expected = [
      gen_yielded_data(0, outputs=[SHARD_OUTPUT_1]),
      gen_yielded_data(1, outputs=[SHARD_OUTPUT_2]),
      gen_yielded_data(2, outputs=[SHARD_OUTPUT_3], exit_codes=[0, 1]),
    ]
    self.assertEqual(sorted(expected), sorted(output_collector.results))

  def test_collect_nothing(self):
    self.mock(swarming, 'yield_results', lambda *_: [])
    self.assertEqual(
        1, collect('https://localhost:1', 'name', ['10100', '10200']))
    self._check_output('', 'Results from some shards are missing: 0, 1\n')

  def test_collect_success(self):
    data = gen_result_response(outputs=['Foo'])
    self.mock(swarming, 'yield_results', lambda *_: [(0, data)])
    self.assertEqual(0, collect('https://localhost:1', 'name', ['10100']))
    self._check_output(
        '+----------------------------------------------------------+\n'
        '| Shard 0  https://localhost:1/user/task/10100             |\n'
        '+----------------------------------------------------------+\n'
        'Foo\n'
        '+----------------------------------------------------------+\n'
        '| End of shard 0  Duration: 1.8s  Bot: swarm6  Exit code 0 |\n'
        '+----------------------------------------------------------+\n'
        'Total duration: 1.8s\n',
        '')

  def test_collect_fail(self):
    data = gen_result_response(outputs=['Foo'], exit_codes=[-9])
    data['outputs'] = ['Foo']
    self.mock(swarming, 'yield_results', lambda *_: [(0, data)])
    self.assertEqual(-9, collect('https://localhost:1', 'name', ['10100']))
    self._check_output(
        '+-----------------------------------------------------------+\n'
        '| Shard 0  https://localhost:1/user/task/10100              |\n'
        '+-----------------------------------------------------------+\n'
        'Foo\n'
        '+-----------------------------------------------------------+\n'
        '| End of shard 0  Duration: 1.8s  Bot: swarm6  Exit code -9 |\n'
        '+-----------------------------------------------------------+\n'
        'Total duration: 1.8s\n',
        '')

  def test_collect_one_missing(self):
    data = gen_result_response(outputs=['Foo'])
    data['outputs'] = ['Foo']
    self.mock(swarming, 'yield_results', lambda *_: [(0, data)])
    self.assertEqual(
        1, collect('https://localhost:1', 'name', ['10100', '10200']))
    self._check_output(
        '+----------------------------------------------------------+\n'
        '| Shard 0  https://localhost:1/user/task/10100             |\n'
        '+----------------------------------------------------------+\n'
        'Foo\n'
        '+----------------------------------------------------------+\n'
        '| End of shard 0  Duration: 1.8s  Bot: swarm6  Exit code 0 |\n'
        '+----------------------------------------------------------+\n'
        '\n'
        'Total duration: 1.8s\n',
        'Results from some shards are missing: 1\n')

  def test_collect_multi(self):
    actual_calls = []
    self.mock(
        isolateserver, 'fetch_isolated',
        lambda *args: actual_calls.append(args))
    shards_output = [
      gen_run_isolated_out_hack_log('https://server', 'namespace', 'hash1'),
      gen_run_isolated_out_hack_log('https://server', 'namespace', 'hash2'),
      OUTPUT,
    ]

    collector = swarming.TaskOutputCollector(
        self.tempdir, 'name', len(shards_output))
    for index, shard_output in enumerate(shards_output):
      collector.process_shard_result(
          index, gen_result_response(outputs=[shard_output]))
    summary = collector.finalize()

    expected_calls = [
      ('hash1', None, None, os.path.join(self.tempdir, '0'), False),
      ('hash2', None, None, os.path.join(self.tempdir, '1'), False),
    ]
    self.assertEqual(len(expected_calls), len(actual_calls))
    storage_instances = set()
    for expected, used in zip(expected_calls, actual_calls):
      isolated_hash, storage, cache, outdir, require_command = used
      storage_instances.add(storage)
      # Compare everything but |storage| and |cache| (use None in their place).
      self.assertEqual(
          expected, (isolated_hash, None, None, outdir, require_command))
      # Ensure cache is set.
      self.assertTrue(cache)

    # Only one instance of Storage should be used.
    self.assertEqual(1, len(storage_instances))

    # Ensure storage is pointing to required location.
    storage = storage_instances.pop()
    self.assertEqual('https://server', storage.location)
    self.assertEqual('namespace', storage.namespace)

    # Ensure collected summary is correct.
    isolated_outs = [
      {
        'hash': 'hash1',
        'namespace': 'namespace',
        'server': 'https://server',
        'view_url': 'https://server/browse?namespace=namespace&hash=hash1',
      },
      {
        'hash': 'hash2',
        'namespace': 'namespace',
        'server': 'https://server',
        'view_url': 'https://server/browse?namespace=namespace&hash=hash2',
      },
      None,
    ]
    expected = {
      'shards': [
        gen_result_response(isolated_out=isolated_out, outputs=[shard_output])
        for index, (isolated_out, shard_output) in
            enumerate(zip(isolated_outs, shards_output))
      ],
    }
    self.assertEqual(expected, summary)

    # Ensure summary dumped to a file is correct as well.
    with open(os.path.join(self.tempdir, 'summary.json'), 'r') as f:
      summary_dump = json.load(f)
    self.assertEqual(expected, summary_dump)

  def test_ensures_same_server(self):
    self.mock(logging, 'error', lambda *_: None)
    # Two shard results, attempt to use different servers.
    actual_calls = []
    self.mock(
        isolateserver, 'fetch_isolated',
        lambda *args: actual_calls.append(args))
    data = [
      gen_result_response(
        outputs=[
          gen_run_isolated_out_hack_log('https://server1', 'namespace', 'hash1')
        ]),
      gen_result_response(
        outputs=[
          gen_run_isolated_out_hack_log('https://server2', 'namespace', 'hash2')
        ]),
    ]

    # Feed them to collector.
    collector = swarming.TaskOutputCollector(self.tempdir, 'task/name', 2)
    for index, result in enumerate(data):
      collector.process_shard_result(index, result)
    collector.finalize()

    # Only first fetch is made, second one is ignored.
    self.assertEqual(1, len(actual_calls))
    isolated_hash, storage, _, outdir, _ = actual_calls[0]
    self.assertEqual(
        ('hash1', os.path.join(self.tempdir, '0')),
        (isolated_hash, outdir))
    self.assertEqual('https://server1', storage.location)

  def test_extract_output_files_location_ok(self):
    task_log = '\n'.join((
      'some log',
      'some more log',
      gen_run_isolated_out_hack_log('https://fake', 'default', '12345'),
      'more log',
    ))
    self.assertEqual(
        {'hash': '12345',
         'namespace': 'default',
         'server': 'https://fake',
         'view_url': 'https://fake/browse?namespace=default&hash=12345'},
        swarming.extract_output_files_location(task_log))

  def test_extract_output_files_location_empty(self):
    task_log = '\n'.join((
      'some log',
      'some more log',
      '[run_isolated_out_hack]',
      '[/run_isolated_out_hack]',
    ))
    self.assertEqual(
        None,
        swarming.extract_output_files_location(task_log))

  def test_extract_output_files_location_missing(self):
    task_log = '\n'.join((
      'some log',
      'some more log',
      'more log',
    ))
    self.assertEqual(
        None,
        swarming.extract_output_files_location(task_log))

  def test_extract_output_files_location_corrupt(self):
    task_log = '\n'.join((
      'some log',
      'some more log',
      '[run_isolated_out_hack]',
      '{"hash": "12345","namespace":}',
      '[/run_isolated_out_hack]',
      'more log',
    ))
    self.assertEqual(
        None,
        swarming.extract_output_files_location(task_log))

  def test_extract_output_files_location_not_url(self):
    task_log = '\n'.join((
      'some log',
      'some more log',
      gen_run_isolated_out_hack_log('/local/path', 'default', '12345'),
      'more log',
    ))
    self.assertEqual(
        None,
        swarming.extract_output_files_location(task_log))


class TestMain(NetTestCase):
  # Tests calling main().
  def test_run_raw_cmd(self):
    # Minimalist use.
    request = {
      'name': 'None/foo=bar',
      'parent_task_id': '',
      'priority': 100,
      'properties': {
        'commands': [['python', '-c', 'print(\'hi\')']],
        'data': [],
        'dimensions': {'foo': 'bar'},
        'env': {},
        'execution_timeout_secs': 3600,
        'idempotent': False,
        'io_timeout_secs': 1200,
      },
      'scheduling_expiration_secs': 21600,
      'tags': [],
      'user': None,
    }
    result = gen_request_response(request)
    self.expected_requests(
        [
          (
            'https://localhost:1/swarming/api/v1/client/handshake',
            {'data': {}, 'headers': {'X-XSRF-Token-Request': '1'}},
            {'server_version': 'v1', 'xsrf_token': 'Token'},
          ),
          (
            'https://localhost:1/swarming/api/v1/client/request',
            {'data': request, 'headers': {'X-XSRF-Token': 'Token'}},
            result,
          ),
        ])
    ret = main([
        'trigger',
        '--swarming', 'https://localhost:1',
        '--dimension', 'foo', 'bar',
        '--raw-cmd',
        '--',
        'python',
        '-c',
        'print(\'hi\')',
      ])
    actual = sys.stdout.getvalue()
    self.assertEqual(0, ret, (actual, sys.stderr.getvalue()))
    self._check_output(
        'Triggered task: None/foo=bar\n'
        'To collect results, use:\n'
        '  swarming.py collect -S https://localhost:1 12300\n'
        'Or visit:\n'
        '  https://localhost:1/user/task/12300\n',
        '')

  def test_run_isolated_hash(self):
    # pylint: disable=unused-argument
    def isolated_upload_zip_bundle(isolate_server, bundle):
      return 'https://localhost:1/fetch_url'
    self.mock(
        swarming, 'isolated_upload_zip_bundle', isolated_upload_zip_bundle)
    self.mock(swarming, 'now', lambda: 123456)

    request = gen_request_data()
    result = gen_request_response(request)
    self.expected_requests(
        [
          (
            'https://localhost:1/swarming/api/v1/client/handshake',
            {'data': {}, 'headers': {'X-XSRF-Token-Request': '1'}},
            {'server_version': 'v1', 'xsrf_token': 'Token'},
          ),
          (
            'https://localhost:1/swarming/api/v1/client/request',
            {'data': request, 'headers': {'X-XSRF-Token': 'Token'}},
            result,
          ),
        ])
    ret = main([
        'trigger',
        '--swarming', 'https://localhost:1',
        '--isolate-server', 'https://localhost:2',
        '--shards', '1',
        '--priority', '101',
        '--dimension', 'foo', 'bar',
        '--dimension', 'os', 'Mac',
        '--expiration', '3600',
        '--user', 'joe@localhost',
        '--tags', 'taga',
        '--tags', 'tagb',
        '--hard-timeout', '60',
        '--io-timeout', '60',
        '--task-name', 'unit_tests',
        '--isolated', FILE_HASH,
        '--',
        '--some-arg',
        '123',
      ])
    actual = sys.stdout.getvalue()
    self.assertEqual(0, ret, (actual, sys.stderr.getvalue()))
    self._check_output(
        'Triggered task: unit_tests\n'
        'To collect results, use:\n'
        '  swarming.py collect -S https://localhost:1 12300\n'
        'Or visit:\n'
        '  https://localhost:1/user/task/12300\n',
        '')

  def test_run_isolated_upload_and_json(self):
    # pylint: disable=unused-argument
    write_json_calls = []
    self.mock(tools, 'write_json', lambda *args: write_json_calls.append(args))
    def isolated_upload_zip_bundle(isolate_server, bundle):
      return 'https://localhost:1/fetch_url'
    self.mock(
        swarming, 'isolated_upload_zip_bundle', isolated_upload_zip_bundle)
    subprocess_calls = []
    self.mock(subprocess, 'call', lambda *c: subprocess_calls.append(c))
    self.mock(swarming, 'now', lambda: 123456)

    isolated = os.path.join(self.tempdir, 'zaz.isolated')
    content = '{}'
    with open(isolated, 'wb') as f:
      f.write(content)

    isolated_hash = isolateserver_mock.hash_content(content)
    request = gen_request_data(
        isolated_hash=isolated_hash, properties=dict(idempotent=True))
    result = gen_request_response(request)
    self.expected_requests(
        [
          (
            'https://localhost:1/swarming/api/v1/client/handshake',
            {'data': {}, 'headers': {'X-XSRF-Token-Request': '1'}},
            {'server_version': 'v1', 'xsrf_token': 'Token'},
          ),
          (
            'https://localhost:1/swarming/api/v1/client/request',
            {'data': request, 'headers': {'X-XSRF-Token': 'Token'}},
            result,
          ),
        ])
    ret = main([
        'trigger',
        '--swarming', 'https://localhost:1',
        '--isolate-server', 'https://localhost:2',
        '--shards', '1',
        '--priority', '101',
        '--dimension', 'foo', 'bar',
        '--dimension', 'os', 'Mac',
        '--expiration', '3600',
        '--user', 'joe@localhost',
        '--tags', 'taga',
        '--tags', 'tagb',
        '--hard-timeout', '60',
        '--io-timeout', '60',
        '--idempotent',
        '--task-name', 'unit_tests',
        '--dump-json', 'foo.json',
        isolated,
        '--',
        '--some-arg',
        '123',
      ])
    actual = sys.stdout.getvalue()
    self.assertEqual(0, ret, (actual, sys.stderr.getvalue()))
    expected = [
      (
        [
          sys.executable,
          os.path.join(swarming.ROOT_DIR, 'isolate.py'), 'archive',
          '--isolate-server', 'https://localhost:2',
          '--namespace' ,'default-gzip',
          '--isolated', isolated,
        ],
      0),
    ]
    self.assertEqual(expected, subprocess_calls)
    self._check_output(
        'Archiving: %s\n'
        'Triggered task: unit_tests\n'
        'To collect results, use:\n'
        '  swarming.py collect -S https://localhost:1 --json foo.json\n'
        'Or visit:\n'
        '  https://localhost:1/user/task/12300\n' % isolated,
        '')
    expected = [
      (
        'foo.json',
        {
          'base_task_name': 'unit_tests',
          'tasks': {
            'unit_tests': {
              'shard_index': 0,
              'task_id': '12300',
              'view_url': 'https://localhost:1/user/task/12300',
            }
          },
        },
        True,
      ),
    ]
    self.assertEqual(expected, write_json_calls)

  def test_trigger_no_request(self):
    with self.assertRaises(SystemExit):
      main([
            'trigger', '--swarming', 'https://host',
            '--isolate-server', 'https://host', '-T', 'foo',
            '-d', 'os', 'amgia',
          ])
    self._check_output(
        '',
        'Usage: swarming.py trigger [options] (hash|isolated) '
          '[-- extra_args|raw command]\n'
        '\n'
        'swarming.py: error: Use --isolated, --raw-cmd or \'--\' to pass '
          'arguments to the called process.\n')

  def test_trigger_no_env_vars(self):
    with self.assertRaises(SystemExit):
      main(['trigger'])
    self._check_output(
        '',
        'Usage: swarming.py trigger [options] (hash|isolated) '
          '[-- extra_args|raw command]'
        '\n\n'
        'swarming.py: error: --swarming is required.'
        '\n')

  def test_trigger_no_swarming_env_var(self):
    with self.assertRaises(SystemExit):
      with test_utils.EnvVars({'ISOLATE_SERVER': 'https://host'}):
        main(['trigger', '-T' 'foo', 'foo.isolated'])
    self._check_output(
        '',
        'Usage: swarming.py trigger [options] (hash|isolated) '
          '[-- extra_args|raw command]'
        '\n\n'
        'swarming.py: error: --swarming is required.'
        '\n')

  def test_trigger_no_isolate_server(self):
    with self.assertRaises(SystemExit):
      with test_utils.EnvVars({'SWARMING_SERVER': 'https://host'}):
        main(['trigger', 'foo.isolated', '-d', 'os', 'amiga'])
    self._check_output(
        '',
        'Usage: swarming.py trigger [options] (hash|isolated) '
          '[-- extra_args|raw command]'
        '\n\n'
        'swarming.py: error: --isolate-server is required.'
        '\n')

  def test_trigger_no_dimension(self):
    with self.assertRaises(SystemExit):
      main([
            'trigger', '--swarming', 'https://host', '--raw-cmd', '--', 'foo',
          ])
    self._check_output(
        '',
        'Usage: swarming.py trigger [options] (hash|isolated) '
          '[-- extra_args|raw command]'
        '\n\n'
        'swarming.py: error: Please at least specify one --dimension\n')

  def test_query_base(self):
    self.expected_requests(
        [
          (
            'https://localhost:1/swarming/api/v1/client/bots/botid/tasks?'
                'limit=200',
            {},
            {'yo': 'dawg'},
          ),
        ])
    ret = main(
        [
          'query', '--swarming', 'https://localhost:1', 'bots/botid/tasks',
        ])
    self._check_output('{\n  "yo": "dawg"\n}\n', '')
    self.assertEqual(0, ret)

  def test_query_cursor(self):
    self.expected_requests(
        [
          (
            'https://localhost:1/swarming/api/v1/client/bots/botid/tasks?'
                'limit=2',
            {},
            {
              'cursor': '%',
              'extra': False,
              'items': ['A'],
            },
          ),
          (
            'https://localhost:1/swarming/api/v1/client/bots/botid/tasks?'
                'cursor=%25&limit=1',
            {},
            {
              'cursor': None,
              'items': ['B'],
              'ignored': True,
            },
          ),
        ])
    ret = main(
        [
          'query', '--swarming', 'https://localhost:1', 'bots/botid/tasks',
          '--limit', '2',
        ])
    expected = (
        '{\n'
        '  "extra": false, \n'
        '  "items": [\n'
        '    "A", \n'
        '    "B"\n'
        '  ]\n'
        '}\n')
    self._check_output(expected, '')
    self.assertEqual(0, ret)

  def test_reproduce(self):
    old_cwd = os.getcwd()
    try:
      os.chdir(self.tempdir)

      def call(cmd, env, cwd):
        self.assertEqual(['foo'], cmd)
        expected = os.environ.copy()
        expected['aa'] = 'bb'
        self.assertEqual(expected, env)
        self.assertEqual('work', cwd)
        return 0

      self.mock(subprocess, 'call', call)

      self.expected_requests(
          [
            (
              'https://localhost:1/swarming/api/v1/client/task/123/request',
              {},
              {
                'properties': {
                  'commands': [['foo']],
                  'data': [],
                  'env': {'aa': 'bb'},
                },
              },
            ),
          ])
      ret = main(
          [
            'reproduce', '--swarming', 'https://localhost:1', '123',
          ])
      self._check_output('', '')
      self.assertEqual(0, ret)
    finally:
      os.chdir(old_cwd)


class TestCommandBot(NetTestCase):
  # Specialized test fixture for command 'bot'.
  def setUp(self):
    super(TestCommandBot, self).setUp()
    # Expected requests are always the same, independent of the test case.
    self.expected_requests(
        [
          (
            'https://localhost:1/swarming/api/v1/client/bots?limit=250',
            {},
            self.mock_swarming_api_v1_bots_page_1(),
          ),
          (
            'https://localhost:1/swarming/api/v1/client/bots?limit=250&'
              'cursor=opaque_cursor',
            {},
            self.mock_swarming_api_v1_bots_page_2(),
          ),
        ])

  @staticmethod
  def mock_swarming_api_v1_bots_page_1():
    """Returns fake /swarming/api/v1/client/bots data."""
    # Sample data retrieved from actual server.
    now = unicode(datetime.datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S'))
    return {
      u'items': [
        {
          u'created_ts': now,
          u'dimensions': {
            u'cores': u'4',
            u'cpu': [u'x86', u'x86-64'],
            u'gpu': [u'15ad', u'15ad:0405'],
            u'hostname': u'swarm3.example.com',
            u'id': u'swarm3',
            u'os': [u'Mac', u'Mac-10.9'],
          },
          u'external_ip': u'1.1.1.3',
          u'hostname': u'swarm3.example.com',
          u'id': u'swarm3',
          u'internal_ip': u'192.168.0.3',
          u'is_dead': False,
          u'last_seen_ts': now,
          u'quarantined': False,
          u'task_id': u'148569b73a89501',
          u'task_name': u'browser_tests',
          u'version': u'56918a2ea28a6f51751ad14cc086f118b8727905',
        },
        {
          u'created_ts': now,
          u'dimensions': {
            u'cores': u'8',
            u'cpu': [u'x86', u'x86-64'],
            u'gpu': [],
            u'hostname': u'swarm1.example.com',
            u'id': u'swarm1',
            u'os': [u'Linux', u'Linux-12.04'],
          },
          u'external_ip': u'1.1.1.1',
          u'hostname': u'swarm1.example.com',
          u'id': u'swarm1',
          u'internal_ip': u'192.168.0.1',
          u'is_dead': True,
          u'last_seen_ts': 'A long time ago',
          u'quarantined': False,
          u'task_id': u'',
          u'task_name': None,
          u'version': u'56918a2ea28a6f51751ad14cc086f118b8727905',
        },
        {
          u'created_ts': now,
          u'dimensions': {
            u'cores': u'8',
            u'cpu': [u'x86', u'x86-64'],
            u'cygwin': u'0',
            u'gpu': [
              u'15ad',
              u'15ad:0405',
              u'VMware Virtual SVGA 3D Graphics Adapter',
            ],
            u'hostname': u'swarm2.example.com',
            u'id': u'swarm2',
            u'integrity': u'high',
            u'os': [u'Windows', u'Windows-6.1'],
          },
          u'external_ip': u'1.1.1.2',
          u'hostname': u'swarm2.example.com',
          u'id': u'swarm2',
          u'internal_ip': u'192.168.0.2',
          u'is_dead': False,
          u'last_seen_ts': now,
          u'quarantined': False,
          u'task_id': u'',
          u'task_name': None,
          u'version': u'56918a2ea28a6f51751ad14cc086f118b8727905',
        },
      ],
      u'cursor': u'opaque_cursor',
      u'death_timeout': 1800.0,
      u'limit': 4,
      u'now': unicode(now),
    }

  @staticmethod
  def mock_swarming_api_v1_bots_page_2():
    """Returns fake /swarming/api/v1/client/bots data."""
    # Sample data retrieved from actual server.
    now = unicode(datetime.datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S'))
    return {
      u'items': [
        {
          u'created_ts': now,
          u'dimensions': {
            u'cores': u'8',
            u'cpu': [u'x86', u'x86-64'],
            u'gpu': [],
            u'hostname': u'swarm4.example.com',
            u'id': u'swarm4',
            u'os': [u'Linux', u'Linux-12.04'],
          },
          u'external_ip': u'1.1.1.4',
          u'hostname': u'swarm4.example.com',
          u'id': u'swarm4',
          u'internal_ip': u'192.168.0.4',
          u'is_dead': False,
          u'last_seen_ts': now,
          u'quarantined': False,
          u'task_id': u'14856971a64c601',
          u'task_name': u'base_unittests',
          u'version': u'56918a2ea28a6f51751ad14cc086f118b8727905',
        }
      ],
      u'cursor': None,
      u'death_timeout': 1800.0,
      u'limit': 4,
      u'now': unicode(now),
    }

  def test_bots(self):
    ret = main(['bots', '--swarming', 'https://localhost:1'])
    expected = (
        u'swarm2\n'
        u'  {"cores": "8", "cpu": ["x86", "x86-64"], "cygwin": "0", "gpu": '
          '["15ad", "15ad:0405", "VMware Virtual SVGA 3D Graphics Adapter"], '
          '"hostname": "swarm2.example.com", "id": "swarm2", "integrity": '
          '"high", "os": ["Windows", "Windows-6.1"]}\n'
        'swarm3\n'
        '  {"cores": "4", "cpu": ["x86", "x86-64"], "gpu": ["15ad", '
          '"15ad:0405"], "hostname": "swarm3.example.com", "id": "swarm3", '
          '"os": ["Mac", "Mac-10.9"]}\n'
        u'  task: 148569b73a89501\n'
        u'swarm4\n'
        u'  {"cores": "8", "cpu": ["x86", "x86-64"], "gpu": [], "hostname": '
          '"swarm4.example.com", "id": "swarm4", "os": ["Linux", '
          '"Linux-12.04"]}\n'
        u'  task: 14856971a64c601\n')
    self._check_output(expected, '')
    self.assertEqual(0, ret)

  def test_bots_bare(self):
    ret = main(['bots', '--swarming', 'https://localhost:1', '--bare'])
    self._check_output("swarm2\nswarm3\nswarm4\n", '')
    self.assertEqual(0, ret)

  def test_bots_filter(self):
    ret = main(
        [
          'bots', '--swarming', 'https://localhost:1',
          '--dimension', 'os', 'Windows',
        ])
    expected = (
        u'swarm2\n  {"cores": "8", "cpu": ["x86", "x86-64"], "cygwin": "0", '
          '"gpu": ["15ad", "15ad:0405", "VMware Virtual SVGA 3D Graphics '
          'Adapter"], "hostname": "swarm2.example.com", "id": "swarm2", '
          '"integrity": "high", "os": ["Windows", "Windows-6.1"]}\n')
    self._check_output(expected, '')
    self.assertEqual(0, ret)

  def test_bots_filter_keep_dead(self):
    ret = main(
        [
          'bots', '--swarming', 'https://localhost:1',
          '--dimension', 'os', 'Linux', '--keep-dead',
        ])
    expected = (
        u'swarm1\n  {"cores": "8", "cpu": ["x86", "x86-64"], "gpu": [], '
          '"hostname": "swarm1.example.com", "id": "swarm1", "os": ["Linux", '
          '"Linux-12.04"]}\n'
        u'swarm4\n'
        u'  {"cores": "8", "cpu": ["x86", "x86-64"], "gpu": [], "hostname": '
          '"swarm4.example.com", "id": "swarm4", "os": ["Linux", '
          '"Linux-12.04"]}\n'
        u'  task: 14856971a64c601\n')
    self._check_output(expected, '')
    self.assertEqual(0, ret)

  def test_bots_filter_dead_only(self):
    ret = main(
        [
          'bots', '--swarming', 'https://localhost:1',
          '--dimension', 'os', 'Linux', '--dead-only',
        ])
    expected = (
        u'swarm1\n  {"cores": "8", "cpu": ["x86", "x86-64"], "gpu": [], '
          '"hostname": "swarm1.example.com", "id": "swarm1", "os": ["Linux", '
          '"Linux-12.04"]}\n')
    self._check_output(expected, '')
    self.assertEqual(0, ret)


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.CRITICAL)
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  for e in ('ISOLATE_SERVER', 'SWARMING_TASK_ID', 'SWARMING_SERVER'):
    os.environ.pop(e, None)
  unittest.main()
