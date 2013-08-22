#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import logging
import os
import signal
import sys
import threading
import time
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

from utils import threading_utils


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

  # Append custom assert messages to default ones (works with python >= 2.7).
  longMessage = True

  @staticmethod
  def sleep_task(duration=0.01):
    """Returns function that sleeps |duration| sec and returns its argument."""
    def task(arg):
      time.sleep(duration)
      return arg
    return task

  def retrying_sleep_task(self, duration=0.01):
    """Returns function that adds sleep_task to the thread pool."""
    def task(arg):
      self.thread_pool.add_task(0, self.sleep_task(duration), arg)
    return task

  @staticmethod
  def none_task():
    """Returns function that returns None."""
    return lambda _arg: None

  def setUp(self):
    super(ThreadPoolTest, self).setUp()
    self.thread_pool = threading_utils.ThreadPool(
        self.MIN_THREADS, self.MAX_THREADS, 0)

  @timeout(1)
  def tearDown(self):
    super(ThreadPoolTest, self).tearDown()
    self.thread_pool.close()

  def get_results_via_join(self, _expected):
    return self.thread_pool.join()

  def get_results_via_get_one_result(self, expected):
    return [self.thread_pool.get_one_result() for _ in expected]

  def get_results_via_iter_results(self, _expected):
    return list(self.thread_pool.iter_results())

  def run_results_test(self, task, results_getter, args=None, expected=None):
    """Template function for tests checking that pool returns all results.

    Will add multiple instances of |task| to the thread pool, then call
    |results_getter| to get back all results and compare them to expected ones.
    """
    args = range(0, 100) if args is None else args
    expected = args if expected is None else expected
    msg = 'Using \'%s\' to get results.' % (results_getter.__name__,)

    for i in args:
      self.thread_pool.add_task(0, task, i)
    results = results_getter(expected)

    # Check that got all results back (exact same set, no duplicates).
    self.assertEqual(set(expected), set(results), msg)
    self.assertEqual(len(expected), len(results), msg)

    # Queue is empty, result request should fail.
    with self.assertRaises(threading_utils.ThreadPoolEmpty):
      self.thread_pool.get_one_result()

  @timeout(1)
  def test_get_one_result_ok(self):
    self.thread_pool.add_task(0, lambda: 'OK')
    self.assertEqual(self.thread_pool.get_one_result(), 'OK')

  @timeout(1)
  def test_get_one_result_fail(self):
    # No tasks added -> get_one_result raises an exception.
    with self.assertRaises(threading_utils.ThreadPoolEmpty):
      self.thread_pool.get_one_result()

  @timeout(5)
  def test_join(self):
    self.run_results_test(self.sleep_task(),
                          self.get_results_via_join)

  @timeout(5)
  def test_get_one_result(self):
    self.run_results_test(self.sleep_task(),
                          self.get_results_via_get_one_result)

  @timeout(5)
  def test_iter_results(self):
    self.run_results_test(self.sleep_task(),
                          self.get_results_via_iter_results)

  @timeout(5)
  def test_retry_and_join(self):
    self.run_results_test(self.retrying_sleep_task(),
                          self.get_results_via_join)

  @timeout(5)
  def test_retry_and_get_one_result(self):
    self.run_results_test(self.retrying_sleep_task(),
                          self.get_results_via_get_one_result)

  @timeout(5)
  def test_retry_and_iter_results(self):
    self.run_results_test(self.retrying_sleep_task(),
                          self.get_results_via_iter_results)

  @timeout(5)
  def test_none_task_and_join(self):
    self.run_results_test(self.none_task(),
                          self.get_results_via_join,
                          expected=[])

  @timeout(5)
  def test_none_task_and_get_one_result(self):
    self.thread_pool.add_task(0, self.none_task(), 0)
    with self.assertRaises(threading_utils.ThreadPoolEmpty):
      self.thread_pool.get_one_result()

  @timeout(5)
  def test_none_task_and_and_iter_results(self):
    self.run_results_test(self.none_task(),
                          self.get_results_via_iter_results,
                          expected=[])

  @timeout(5)
  def test_generator_task(self):
    MULTIPLIER = 1000
    COUNT = 10

    # Generator that yields [i * MULTIPLIER, i * MULTIPLIER + COUNT).
    def generator_task(i):
      for j in xrange(COUNT):
        time.sleep(0.001)
        yield i * MULTIPLIER + j

    # Arguments for tasks and expected results.
    args = range(0, 10)
    expected = [i * MULTIPLIER + j for i in args for j in xrange(COUNT)]

    # Test all possible ways to pull results from the thread pool.
    getters = (self.get_results_via_join,
               self.get_results_via_iter_results,
               self.get_results_via_get_one_result,)
    for results_getter in getters:
      self.run_results_test(generator_task, results_getter, args, expected)

  @timeout(5)
  def test_concurrent_iter_results(self):
    def poller_proc(result):
      result.extend(self.thread_pool.iter_results())

    args = range(0, 100)
    for i in args:
      self.thread_pool.add_task(0, self.sleep_task(), i)

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

  @timeout(1)
  def test_adding_tasks_after_close(self):
    pool = threading_utils.ThreadPool(1, 1, 0)
    pool.add_task(0, lambda: None)
    pool.close()
    with self.assertRaises(threading_utils.ThreadPoolClosed):
      pool.add_task(0, lambda: None)

  @timeout(1)
  def test_double_close(self):
    pool = threading_utils.ThreadPool(1, 1, 0)
    pool.close()
    with self.assertRaises(threading_utils.ThreadPoolClosed):
      pool.close()


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
