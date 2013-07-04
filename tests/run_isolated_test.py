#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import json
import logging
import math
import os
import shutil
import signal
import StringIO
import sys
import tempfile
import threading
import time
import unittest
import urllib2

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import run_isolated

# Number of times self._now() is called per loop in HttpService._retry_loop().
NOW_CALLS_PER_OPEN = 2


class RemoteTest(run_isolated.Remote):
  def get_file_handler(self, _):  # pylint: disable=R0201
    def upload_file(item, _dest):
      if type(item) == type(Exception) and issubclass(item, Exception):
        raise item()
      elif isinstance(item, int):
        time.sleep(int(item) / 100)
    return upload_file


def timeout(max_running_time):
  """Test method decorator that fails the test if it executes longer
  than |max_running_time| seconds.

  It exists to terminate tests in case of deadlocks. There's a high chance that
  process is broken after such timeout (due to hanging deadlocked threads that
  can own some shared resources). But failing early (maybe not in a cleanest
  way) due to timeout is generally better than hanging indefinitely.

  |max_running_time| should be an order of magnitude (or even two orders) larger
  than the expected run time of the test to compensate for slow machine, high
  CPU utilization by some other processes, etc.

  Can not be nested.

  Noop on windows (since win32 doesn't support signal.setitimer).
  """
  if sys.platform == 'win32':
    return lambda method: method

  def decorator(method):
    @functools.wraps(method)
    def wrapper(self, *args, **kwargs):
      signal.signal(signal.SIGALRM, lambda *_args: self.fail('Timeout'))
      signal.setitimer(signal.ITIMER_REAL, max_running_time)
      try:
        return method(self, *args, **kwargs)
      finally:
        signal.signal(signal.SIGALRM, signal.SIG_DFL)
        signal.setitimer(signal.ITIMER_REAL, 0)
    return wrapper

  return decorator


class ThreadPoolTest(unittest.TestCase):
  MIN_THREADS = 0
  MAX_THREADS = 32

  @staticmethod
  def sleep_task(duration):
    """Returns function that sleeps |duration| sec and returns its argument."""
    def task(arg):
      time.sleep(duration)
      return arg
    return task

  def setUp(self):
    super(ThreadPoolTest, self).setUp()
    self.thread_pool = run_isolated.ThreadPool(
        self.MIN_THREADS, self.MAX_THREADS, 0)

  @timeout(1)
  def tearDown(self):
    super(ThreadPoolTest, self).tearDown()
    self.thread_pool.close()

  @timeout(1)
  def test_get_one_result_ok(self):
    self.thread_pool.add_task(0, lambda: 'OK')
    self.assertEqual(self.thread_pool.get_one_result(), 'OK')

  @timeout(1)
  def test_get_one_result_fail(self):
    # No tasks added -> get_one_result raises an exception.
    with self.assertRaises(run_isolated.ThreadPoolEmpty):
      self.thread_pool.get_one_result()

  @timeout(1)
  def test_get_many_results(self):
    args = range(0, 100)
    for i in args:
      self.thread_pool.add_task(0, self.sleep_task(0.01), i)
    results = [self.thread_pool.get_one_result() for _ in args]
    # Check that got all results back (exact same set, no duplicates).
    self.assertEqual(set(args), set(results))
    self.assertEqual(len(args), len(results))
    # Queue is empty, result request should fail.
    with self.assertRaises(run_isolated.ThreadPoolEmpty):
      self.thread_pool.get_one_result()

  @timeout(5)
  def test_iter_results(self):
    args = range(0, 100)
    for i in args:
      self.thread_pool.add_task(0, self.sleep_task(0.01), i)
    results = list(self.thread_pool.iter_results())
    # Check that got all results back (exact same set, no duplicates).
    self.assertEqual(set(args), set(results))
    self.assertEqual(len(args), len(results))
    # Queue is empty, result request should fail.
    with self.assertRaises(run_isolated.ThreadPoolEmpty):
      self.thread_pool.get_one_result()

  @timeout(5)
  def test_concurrent_iter_results(self):
    def poller_proc(result):
      result.extend(self.thread_pool.iter_results())
    args = range(0, 100)
    for i in args:
      self.thread_pool.add_task(0, self.sleep_task(0.01), i)
    # Start a bunch of threads, all calling iter_results in parallel.
    pollers = []
    for _ in xrange(0, 4):
      result = []
      poller = threading.Thread(target=poller_proc, args=(result,))
      poller.start()
      pollers.append((poller, result))
    # Collects results from all polling threads.
    all_results = []
    for poller, results in pollers:
      poller.join()
      all_results.extend(results)
    # Check that got all results back (exact same set, no duplicates).
    self.assertEqual(set(args), set(all_results))
    self.assertEqual(len(args), len(all_results))

  @timeout(5)
  def test_join(self):
    args = range(0, 100)
    for i in args:
      self.thread_pool.add_task(0, self.sleep_task(0.01), i)
    results = self.thread_pool.join()
    # Check that got all results back (exact same set, no duplicates).
    self.assertEqual(set(args), set(results))
    self.assertEqual(len(args), len(results))
    # Queue is empty, result request should fail.
    with self.assertRaises(run_isolated.ThreadPoolEmpty):
      self.thread_pool.get_one_result()

  @timeout(1)
  def test_adding_tasks_after_close(self):
    pool = run_isolated.ThreadPool(1, 1, 0)
    pool.add_task(0, lambda: None)
    pool.close()
    with self.assertRaises(run_isolated.ThreadPoolClosed):
      pool.add_task(0, lambda: None)

  @timeout(1)
  def test_double_close(self):
    pool = run_isolated.ThreadPool(1, 1, 0)
    pool.close()
    with self.assertRaises(run_isolated.ThreadPoolClosed):
      pool.close()


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

  def test_remote_no_errors(self):
    files_to_handle = 50
    remote = RemoteTest('')

    for i in range(files_to_handle):
      remote.add_item(
          run_isolated.Remote.MED,
          'obj%d' % i,
          'dest%d' % i,
          run_isolated.UNKNOWN_FILE_SIZE)

    items = sorted(remote.join())
    expected = sorted('obj%d' % i for i in range(files_to_handle))
    self.assertEqual(expected, items)

  def test_remote_with_errors(self):
    remote = RemoteTest('')

    def RaiseIOError(*_):
      raise IOError()
    remote._do_item = RaiseIOError
    remote.add_item(run_isolated.Remote.MED, 'ignored', '',
                    run_isolated.UNKNOWN_FILE_SIZE)
    self.assertRaises(IOError, remote.join)
    self.assertEqual([], remote.join())

  def test_zip_header_error(self):
    old_urlopen = run_isolated.url_open
    old_sleep = run_isolated.time.sleep
    try:
      run_isolated.url_open = lambda _url, **_kwargs: StringIO.StringIO('111')
      run_isolated.time.sleep = lambda _x: None
      remote = run_isolated.Remote('https://fake-CAD.com/')

      # Both files will fail to be unzipped due to incorrect headers,
      # ensure that we don't accept the files (even if the size is unknown)}.
      remote.add_item(run_isolated.Remote.MED, 'zipped_A', 'A',
                      run_isolated.UNKNOWN_FILE_SIZE)
      remote.add_item(run_isolated.Remote.MED, 'zipped_B', 'B', 5)
      self.assertRaises(IOError, remote.get_one_result)
      self.assertRaises(IOError, remote.get_one_result)
      # Need to use join here, since get_one_result will hang.
      self.assertEqual([], remote.join())
    finally:
      run_isolated.url_open = old_urlopen
      run_isolated.time.sleep = old_sleep


class HttpServiceTest(unittest.TestCase):

  # HttpService that doesn't sleep in retries and doesn't write cookie files.
  class HttpServiceNoSideEffects(run_isolated.HttpService):
    def __init__(self, *args, **kwargs):
      super(HttpServiceTest.HttpServiceNoSideEffects, self).__init__(
          *args, **kwargs)
      self.sleeps = []
      self._count = 0.

    def _now(self):  # pylint: disable=W0221
      x = self._count
      self._count += 1
      return x

    def sleep_before_retry(self, attempt, max_wait):
      self.sleeps.append((attempt, max_wait))

    def load_cookie_jar(self):  # pylint: disable=W0221
      pass

    def save_cookie_jar(self):  # pylint: disable=W0221
      pass

  def mocked_http_service(self, url='http://example.com', **kwargs):
    service = HttpServiceTest.HttpServiceNoSideEffects(url)
    for name, fn in kwargs.iteritems():
      self.assertTrue(getattr(service, name))
      setattr(service, name, fn)
    return service

  def test_request_GET_success(self):
    service_url = 'http://example.com'
    request_url = '/some_request'
    response = 'True'

    def mock_url_open(request):
      self.assertTrue(request.get_full_url().startswith(service_url +
                                                        request_url))
      return StringIO.StringIO(response)

    service = self.mocked_http_service(url=service_url, _url_open=mock_url_open)
    self.assertEqual(service.request(request_url).read(), response)
    self.assertEqual([], service.sleeps)

  def test_request_POST_success(self):
    service_url = 'http://example.com'
    request_url = '/some_request'
    response = 'True'

    def mock_url_open(request):
      self.assertTrue(request.get_full_url().startswith(service_url +
                                                        request_url))
      self.assertEqual('', request.get_data())
      return StringIO.StringIO(response)

    service = self.mocked_http_service(url=service_url, _url_open=mock_url_open)
    self.assertEqual(service.request(request_url, data={}).read(), response)
    self.assertEqual([], service.sleeps)

  def test_request_success_after_failure(self):
    response = 'True'

    def mock_url_open(request):
      if run_isolated.COUNT_KEY + '=1' not in request.get_data():
        raise urllib2.URLError('url')
      return StringIO.StringIO(response)

    service = self.mocked_http_service(_url_open=mock_url_open)
    self.assertEqual(service.request('/', data={}).read(), response)
    self.assertEqual(
        [(0, run_isolated.URL_OPEN_TIMEOUT - NOW_CALLS_PER_OPEN)],
        service.sleeps)

  def test_request_failure_max_attempts_default(self):
    def mock_url_open(_request):
      raise urllib2.URLError('url')
    service = self.mocked_http_service(_url_open=mock_url_open)
    self.assertEqual(service.request('/'), None)
    retries = [
      (i, run_isolated.URL_OPEN_TIMEOUT - NOW_CALLS_PER_OPEN * (i + 1))
      for i in xrange(run_isolated.URL_OPEN_MAX_ATTEMPTS)
    ]
    self.assertEqual(retries, service.sleeps)

  def test_request_failure_max_attempts(self):
    def mock_url_open(_request):
      raise urllib2.URLError('url')
    service = self.mocked_http_service(_url_open=mock_url_open)
    self.assertEqual(service.request('/', max_attempts=23), None)
    retries = [
      (i, run_isolated.URL_OPEN_TIMEOUT - NOW_CALLS_PER_OPEN * (i + 1))
      for i in xrange(23)
    ]
    self.assertEqual(retries, service.sleeps)

  def test_request_failure_timeout(self):
    def mock_url_open(_request):
      raise urllib2.URLError('url')
    service = self.mocked_http_service(_url_open=mock_url_open)
    self.assertEqual(service.request('/', max_attempts=10000), None)
    retries = [
      (i, run_isolated.URL_OPEN_TIMEOUT - NOW_CALLS_PER_OPEN * (i + 1))
      # Currently 179 for timeout == 360.
      for i in xrange(
          int(run_isolated.URL_OPEN_TIMEOUT) / NOW_CALLS_PER_OPEN - 1)
    ]
    self.assertEqual(retries, service.sleeps)

  def test_request_failure_timeout_default(self):
    def mock_url_open(_request):
      raise urllib2.URLError('url')
    service = self.mocked_http_service(_url_open=mock_url_open)
    self.assertEqual(service.request('/', timeout=10.), None)
    retries = [(i, 8. - (NOW_CALLS_PER_OPEN * i)) for i in xrange(4)]
    self.assertEqual(retries, service.sleeps)

  def test_request_HTTP_error_no_retry(self):
    count = []
    def mock_url_open(request):
      count.append(request)
      raise urllib2.HTTPError(
          'url', 400, 'error message', None, StringIO.StringIO())

    service = self.mocked_http_service(_url_open=mock_url_open)
    self.assertEqual(service.request('/', data={}), None)
    self.assertEqual(1, len(count))
    self.assertEqual([], service.sleeps)

  def test_request_HTTP_error_retry_404(self):
    response = 'data'
    def mock_url_open(request):
      if run_isolated.COUNT_KEY + '=1' in request.get_data():
        return StringIO.StringIO(response)
      raise urllib2.HTTPError(
          'url', 404, 'error message', None, StringIO.StringIO())

    service = self.mocked_http_service(_url_open=mock_url_open)
    result = service.request('/', data={}, retry_404=True)
    self.assertEqual(result.read(), response)
    self.assertEqual(
        [(0, run_isolated.URL_OPEN_TIMEOUT - NOW_CALLS_PER_OPEN)],
        service.sleeps)

  def test_request_HTTP_error_with_retry(self):
    response = 'response'

    def mock_url_open(request):
      if run_isolated.COUNT_KEY + '=1' not in request.get_data():
        raise urllib2.HTTPError(
            'url', 500, 'error message', None, StringIO.StringIO())
      return StringIO.StringIO(response)

    service = self.mocked_http_service(_url_open=mock_url_open)
    self.assertTrue(service.request('/', data={}).read(), response)
    self.assertEqual(
        [(0, run_isolated.URL_OPEN_TIMEOUT - NOW_CALLS_PER_OPEN)],
        service.sleeps)

  def test_count_key_in_data_failure(self):
    data = {run_isolated.COUNT_KEY: 1}
    service = self.mocked_http_service()
    self.assertEqual(service.request('/', data=data), None)
    self.assertEqual([], service.sleeps)

  def test_auth_success(self):
    count = []
    response = 'response'
    def mock_url_open(_request):
      if not count:
        raise urllib2.HTTPError(
          'url', 403, 'error message', None, StringIO.StringIO())
      return StringIO.StringIO(response)
    def mock_authenticate():
      self.assertEqual(len(count), 0)
      count.append(1)
      return True
    service = self.mocked_http_service(_url_open=mock_url_open,
                                       authenticate=mock_authenticate)
    self.assertEqual(service.request('/').read(), response)
    self.assertEqual(len(count), 1)
    self.assertEqual([], service.sleeps)

  def test_auth_failure(self):
    count = []
    def mock_url_open(_request):
      raise urllib2.HTTPError(
          'url', 403, 'error message', None, StringIO.StringIO())
    def mock_authenticate():
      count.append(1)
      return False
    service = self.mocked_http_service(_url_open=mock_url_open,
                                       authenticate=mock_authenticate)
    self.assertEqual(service.request('/'), None)
    self.assertEqual(len(count), 1) # single attempt only
    self.assertEqual([], service.sleeps)

  def test_request_attempted_before_auth(self):
    calls = []
    def mock_url_open(_request):
      calls.append('url_open')
      raise urllib2.HTTPError(
          'url', 403, 'error message', None, StringIO.StringIO())
    def mock_authenticate():
      calls.append('authenticate')
      return False
    service = self.mocked_http_service(_url_open=mock_url_open,
                                       authenticate=mock_authenticate)
    self.assertEqual(service.request('/'), None)
    self.assertEqual(calls, ['url_open', 'authenticate'])
    self.assertEqual([], service.sleeps)

  def test_sleep_before_retry(self):
    # Verifies bounds. Because it's using a pseudo-random number generator and
    # not a read random source, it's basically guaranteed to never return the
    # same value twice consecutively.
    a = run_isolated.HttpService.calculate_sleep_before_retry(0, 0)
    b = run_isolated.HttpService.calculate_sleep_before_retry(0, 0)
    self.assertTrue(a >= math.pow(1.5, -1), a)
    self.assertTrue(b >= math.pow(1.5, -1), b)
    self.assertTrue(a < 1.5 + math.pow(1.5, -1), a)
    self.assertTrue(b < 1.5 + math.pow(1.5, -1), b)
    self.assertNotEqual(a, b)


if __name__ == '__main__':
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.FATAL))
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
