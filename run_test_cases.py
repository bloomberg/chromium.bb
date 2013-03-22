#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs each test cases as a single shard, single process execution.

Similar to sharding_supervisor.py but finer grained. It runs each test case
individually instead of running per shard. Runs multiple instances in parallel.
"""

import datetime
import fnmatch
import json
import logging
import optparse
import os
import Queue
import random
import re
import subprocess
import sys
import threading
import time
from xml.dom import minidom
import xml.parsers.expat

import run_isolated

# Scripts using run_test_cases as a library expect this function.
from run_isolated import fix_python_path


# These are known to influence the way the output is generated.
KNOWN_GTEST_ENV_VARS = [
  'GTEST_ALSO_RUN_DISABLED_TESTS',
  'GTEST_BREAK_ON_FAILURE',
  'GTEST_CATCH_EXCEPTIONS',
  'GTEST_COLOR',
  'GTEST_FILTER',
  'GTEST_OUTPUT',
  'GTEST_PRINT_TIME',
  'GTEST_RANDOM_SEED',
  'GTEST_REPEAT',
  'GTEST_SHARD_INDEX',
  'GTEST_SHARD_STATUS_FILE',
  'GTEST_SHUFFLE',
  'GTEST_THROW_ON_FAILURE',
  'GTEST_TOTAL_SHARDS',
]

# These needs to be poped out before running a test.
GTEST_ENV_VARS_TO_REMOVE = [
  'GTEST_ALSO_RUN_DISABLED_TESTS',
  'GTEST_FILTER',
  'GTEST_OUTPUT',
  'GTEST_RANDOM_SEED',
  # TODO(maruel): Handle.
  'GTEST_REPEAT',
  'GTEST_SHARD_INDEX',
  # TODO(maruel): Handle.
  'GTEST_SHUFFLE',
  'GTEST_TOTAL_SHARDS',
]


RUN_PREFIX = '[ RUN      ] '
OK_PREFIX = '[       OK ] '
FAILED_PREFIX = '[  FAILED  ] '


def num_processors():
  """Returns the number of processors.

  Python on OSX 10.6 raises a NotImplementedError exception.
  """
  try:
    # Multiprocessing
    import multiprocessing
    return multiprocessing.cpu_count()
  except:  # pylint: disable=W0702
    try:
      # Mac OS 10.6
      return int(os.sysconf('SC_NPROCESSORS_ONLN'))  # pylint: disable=E1101
    except:
      # Some of the windows builders seem to get here.
      return 4


if subprocess.mswindows:
  import msvcrt  # pylint: disable=F0401
  from ctypes import wintypes
  from ctypes import windll

  def ReadFile(handle, desired_bytes):
    """Calls kernel32.ReadFile()."""
    c_read = wintypes.DWORD()
    buff = wintypes.create_string_buffer(desired_bytes+1)
    windll.kernel32.ReadFile(
        handle, buff, desired_bytes, wintypes.byref(c_read), None)
    # NULL terminate it.
    buff[c_read.value] = '\x00'
    return wintypes.GetLastError(), buff.value

  def PeekNamedPipe(handle):
    """Calls kernel32.PeekNamedPipe(). Simplified version."""
    c_avail = wintypes.DWORD()
    c_message = wintypes.DWORD()
    success = windll.kernel32.PeekNamedPipe(
        handle, None, 0, None, wintypes.byref(c_avail),
        wintypes.byref(c_message))
    if not success:
      raise OSError(wintypes.GetLastError())
    return c_avail.value

  def recv_impl(conn, maxsize, timeout):
    """Reads from a pipe without blocking."""
    return recv_multi_impl([conn], maxsize, timeout)[1]

  def recv_multi_impl(conns, maxsize, timeout):
    if timeout:
      start = time.time()
    handles = [msvcrt.get_osfhandle(conn.fileno()) for conn in conns]
    try:
      while True:
        for i, handle in enumerate(handles):
          avail = min(PeekNamedPipe(handle), maxsize)
          if avail:
            return i, ReadFile(handle, avail)[1]
        if not timeout or (time.time() - start) >= timeout:
          return None, None
        # Polling rocks.
        time.sleep(0.001)
    except OSError:
      # Not classy but fits our needs.
      return None, None

else:
  import fcntl  # pylint: disable=F0401
  import select

  def recv_impl(conn, maxsize, timeout):
    """Reads from a pipe without blocking."""
    if not select.select([conn], [], [], timeout)[0]:
      return None

    # Temporarily make it non-blocking.
    flags = fcntl.fcntl(conn, fcntl.F_GETFL)
    if not conn.closed:
      # pylint: disable=E1101
      fcntl.fcntl(conn, fcntl.F_SETFL, flags | os.O_NONBLOCK)
    try:
      return conn.read(maxsize)
    finally:
      if not conn.closed:
        fcntl.fcntl(conn, fcntl.F_SETFL, flags)

  def recv_multi_impl(conns, maxsize, timeout):
    """Reads from the first available pipe without blocking."""
    if timeout:
      start = time.time()
    r, _, _ = select.select(conns, [], [], timeout)
    if not r:
      return None, None
    if timeout:
      timeout = min(0, timeout - (time.time() - start))
    return conns.index(r[0]), recv_impl(r[0], maxsize, timeout)


class Failure(Exception):
  pass


class Popen(subprocess.Popen):
  """Adds timeout support on stdout and stderr.

  Inspired by
  http://code.activestate.com/recipes/440554-module-to-allow-asynchronous-subprocess-use-on-win/
  """
  def __init__(self, *args, **kwargs):
    self.start = time.time()
    self.end = None
    super(Popen, self).__init__(*args, **kwargs)

  def duration(self):
    """Duration of the child process.

    It is greater or equal to the actual time the child process ran. It can be
    significantly higher than the real value if neither .wait() nor .poll() was
    used.
    """
    return (self.end or time.time()) - self.start

  def wait(self):
    ret = super(Popen, self).wait()
    if not self.end:
      # communicate() uses wait() internally.
      self.end = time.time()
    return ret

  def poll(self):
    ret = super(Popen, self).poll()
    if ret is not None and not self.end:
      self.end = time.time()
    return ret

  def yield_any(self, timeout=None):
    """Yields output until the process terminates or is killed by a timeout.

    Yielded values are in the form (pipename, data).
    """
    remaining = 0
    while self.poll() is None:
      if timeout:
        remaining = max(timeout - self.duration(), 0.001)
      t, data = self.recv_any(timeout=remaining)
      if data:
        yield (t, data)
      if timeout and self.duration() >= timeout:
        break
    if self.poll() is None and timeout and self.duration() >= timeout:
      logging.debug('Kill %s %s', self.duration(), timeout)
      self.kill()
    self.wait()
    # Read all remaining output in the pipes.
    while True:
      t, data = self.recv_any()
      if not data:
        break
      yield (t, data)

  def recv_any(self, maxsize=None, timeout=None):
    """Reads from stderr and if empty, from stdout."""
    pipes = [
      x for x in ((self.stderr, 'stderr'), (self.stdout, 'stdout')) if x[0]
    ]
    if len(pipes) == 2 and self.stderr.fileno() == self.stdout.fileno():
      pipes.pop(0)
    if not pipes:
      return None, None
    conns, names = zip(*pipes)
    index, data = recv_multi_impl(conns, max(maxsize or 1024, 1), timeout or 0)
    if index is None:
      return index, data
    if not data:
      self._close(names[index])
      return None, None
    if self.universal_newlines:
      data = self._translate_newlines(data)
    return names[index], data

  def recv_out(self, maxsize=None, timeout=None):
    """Reads from stdout asynchronously."""
    return self._recv('stdout', maxsize, timeout)

  def recv_err(self, maxsize=None, timeout=None):
    """Reads from stderr asynchronously."""
    return self._recv('stderr', maxsize, timeout)

  def _close(self, which):
    getattr(self, which).close()
    setattr(self, which, None)

  def _recv(self, which, maxsize, timeout):
    conn = getattr(self, which)
    if conn is None:
      return None
    data = recv_impl(conn, max(maxsize or 1024, 1), timeout or 0)
    if not data:
      return self._close(which)
    if self.universal_newlines:
      data = self._translate_newlines(data)
    return data


def call_with_timeout(cmd, timeout, **kwargs):
  """Runs an executable with an optional timeout."""
  proc = Popen(
      cmd,
      stdin=subprocess.PIPE,
      stdout=subprocess.PIPE,
      **kwargs)
  if timeout:
    out = ''
    err = ''
    for t, data in proc.yield_any(timeout):
      if t == 'stdout':
        out += data
      else:
        err += data
  else:
    # This code path is much faster.
    out, err = proc.communicate()
  return out, err, proc.returncode, proc.duration()


class QueueWithProgress(Queue.PriorityQueue):
  """Implements progress support in join()."""
  def __init__(self, maxsize, *args, **kwargs):
    Queue.PriorityQueue.__init__(self, *args, **kwargs)
    self.progress = Progress(maxsize)

  def set_progress(self, progress):
    """Replace the current progress, mainly used when a progress should be
    shared between queues."""
    self.progress = progress

  def task_done(self):
    """Contrary to Queue.task_done(), it wakes self.all_tasks_done at each task
    done.
    """
    self.all_tasks_done.acquire()
    try:
      unfinished = self.unfinished_tasks - 1
      if unfinished < 0:
        raise ValueError('task_done() called too many times')
      self.unfinished_tasks = unfinished
      # This is less efficient, because we want the Progress to be updated.
      logging.debug('all_tasks_done.notify_all() at %s', time.time())
      logging.debug('%s unfinished tasks', unfinished)
      self.all_tasks_done.notify_all()
    except Exception as e:
      logging.exception('task_done threw an exception.\n%s', e)
    finally:
      self.all_tasks_done.release()

  def join(self):
    """Calls print_update() whenever possible."""
    self.progress.print_update()
    self.all_tasks_done.acquire()
    try:
      while self.unfinished_tasks:
        logging.debug('Looping in join() with %s unfinished tasks',
                      self.unfinished_tasks)
        self.progress.print_update()
        logging.debug('Calling all_tasks_done.wait() at %s', time.time())
        self.all_tasks_done.wait(60.)
      self.progress.print_update()
    finally:
      self.all_tasks_done.release()


class ThreadPool(run_isolated.ThreadPool):
  QUEUE_CLASS = QueueWithProgress

  def __init__(self, progress, *args, **kwargs):
    super(ThreadPool, self).__init__(*args, **kwargs)
    self.tasks.set_progress(progress)


class Progress(object):
  """Prints progress and accepts updates thread-safely."""
  def __init__(self, size):
    # To be used in the primary thread
    self.last_printed_line = ''
    self.index = 0
    self.start = time.time()
    self.size = size
    self.use_cr_only = True
    self.unfinished_commands = set()

    # To be used in all threads.
    self.queued_lines = Queue.Queue()

  def update_item(self, name, index=True, size=False):
    """Queue information to print out.

    |index| notes that the index should be incremented.
    |size| note that the total size should be incremented.
    """
    # This code doesn't need lock because it's only using self.queued_lines.
    self.queued_lines.put((name, index, size))

  def print_update(self):
    """Prints the current status."""
    # Flush all the logging output so it doesn't appear within this output.
    for handler in logging.Logger.manager.loggerDict:
      handler.flush()

    while True:
      try:
        name, index, size = self.queued_lines.get_nowait()
      except Queue.Empty:
        break

      # Keep track of all the running commands, so we can list which ones
      # are unfinished.
      if 'Starting command' in name:
        match = re.match('^.*?(\[.*\]).*?$', name)
        if match:
          self.unfinished_commands.add(match.group(1))
      if 'finished after' in name:
        match = re.match('^.*?(\[.*\]).*?$', name)
        if match:
          self.unfinished_commands.remove(match.group(1))

      if size:
        self.size += 1
      if index:
        self.index += 1
        alignment = str(len(str(self.size)))
        next_line = ('[%' + alignment + 'd/%d] %6.2fs %s') % (
            self.index,
            self.size,
            time.time() - self.start,
            name)
        # Fill it with whitespace only if self.use_cr_only is set.
        prefix = ''
        if self.use_cr_only:
          if self.last_printed_line:
            prefix = '\r'
        if self.use_cr_only:
          suffix = ' ' * max(0, len(self.last_printed_line) - len(next_line))
        else:
          suffix = '\n'
        line = '%s%s%s' % (prefix, next_line, suffix)
        self.last_printed_line = next_line
      else:
        line = '\n%s\n' % name.strip('\n')
        self.last_printed_line = ''

      sys.stdout.write(line)

    # Ensure that all the output is flush to prevent it from getting mixed with
    # other output streams (like the logging streams).
    sys.stdout.flush()

    if self.unfinished_commands:
      logging.debug('Waiting for the following commands to finish:\n%s',
                    '\n'.join(self.unfinished_commands))


def setup_gtest_env():
  """Copy the enviroment variables and setup for running a gtest."""
  env = os.environ.copy()
  for name in GTEST_ENV_VARS_TO_REMOVE:
    env.pop(name, None)

  # Forcibly enable color by default, if not already disabled.
  env.setdefault('GTEST_COLOR', 'on')

  return env


def gtest_list_tests(cmd, cwd):
  """List all the test cases for a google test.

  See more info at http://code.google.com/p/googletest/.
  """
  cmd = cmd[:]
  cmd.append('--gtest_list_tests')
  env = setup_gtest_env()
  timeout = 60.
  try:
    out, err, returncode, _ = call_with_timeout(
        cmd,
        timeout,
        stderr=subprocess.PIPE,
        env=env,
        cwd=cwd)
  except OSError, e:
    raise Failure('Failed to run %s\ncwd=%s\n%s' % (' '.join(cmd), cwd, str(e)))
  if returncode:
    raise Failure(
        'Failed to run %s\nstdout:\n%s\nstderr:\n%s' %
          (' '.join(cmd), out, err), returncode)
  # pylint: disable=E1103
  if err and not err.startswith('Xlib:  extension "RANDR" missing on display '):
    logging.error('Unexpected spew in gtest_list_tests:\n%s\n%s', err, cmd)
  return out


def filter_shards(tests, index, shards):
  """Filters the shards.

  Watch out about integer based arithmetics.
  """
  # The following code could be made more terse but I liked the extra clarity.
  assert 0 <= index < shards
  total = len(tests)
  quotient, remainder = divmod(total, shards)
  # 1 item of each remainder is distributed over the first 0:remainder shards.
  # For example, with total == 5, index == 1, shards == 3
  # min_bound == 2, max_bound == 4.
  min_bound = quotient * index + min(index, remainder)
  max_bound = quotient * (index + 1) + min(index + 1, remainder)
  return tests[min_bound:max_bound]


def filter_bad_tests(tests, disabled, fails, flaky):
  """Filters out DISABLED_, FAILS_ or FLAKY_ test cases."""
  def starts_with(a, b, prefix):
    return a.startswith(prefix) or b.startswith(prefix)

  def valid(test):
    fixture, case = test.split('.', 1)
    if not disabled and starts_with(fixture, case, 'DISABLED_'):
      return False
    if not fails and starts_with(fixture, case, 'FAILS_'):
      return False
    if not flaky and starts_with(fixture, case, 'FLAKY_'):
      return False
    return True

  return [test for test in tests if valid(test)]


def chromium_valid(test, pre, manual):
  """Returns True if the test case is valid to be selected."""
  def starts_with(a, b, prefix):
    return a.startswith(prefix) or b.startswith(prefix)

  fixture, case = test.split('.', 1)
  if not pre and starts_with(fixture, case, 'PRE_'):
    return False
  if not manual and starts_with(fixture, case, 'MANUAL_'):
    return False
  if test == 'InProcessBrowserTest.Empty':
    return False
  return True


def chromium_filter_bad_tests(tests, disabled, fails, flaky, pre, manual):
  """Filters out PRE_, MANUAL_, and other weird Chromium-specific test cases."""
  tests = filter_bad_tests(tests, disabled, fails, flaky)
  return [test for test in tests if chromium_valid(test, pre, manual)]


def parse_gtest_cases(out, seed):
  """Returns the flattened list of test cases in the executable.

  The returned list is sorted so it is not dependent on the order of the linked
  objects. Then |seed| is applied to deterministically shuffle the list if
  |seed| is a positive value. The rationale is that the probability of two test
  cases stomping on each other when run simultaneously is high for test cases in
  the same fixture. By shuffling the tests, the probability of these badly
  written tests running simultaneously, let alone being in the same shard, is
  lower.

  Expected format is a concatenation of this:
  TestFixture1
     TestCase1
     TestCase2
  """
  tests = []
  fixture = None
  lines = out.splitlines()
  while lines:
    line = lines.pop(0)
    if not line:
      break
    if not line.startswith('  '):
      fixture = line
    else:
      case = line[2:]
      if case.startswith('YOU HAVE'):
        # It's a 'YOU HAVE foo bar' line. We're done.
        break
      assert ' ' not in case
      tests.append(fixture + case)
  tests = sorted(tests)
  if seed:
    # Sadly, python's random module doesn't permit local seeds.
    state = random.getstate()
    try:
      # This is totally deterministic.
      random.seed(seed)
      random.shuffle(tests)
    finally:
      random.setstate(state)
  return tests


def list_test_cases(
    cmd, cwd, index, shards, disabled, fails, flaky, pre, manual, seed):
  """Returns the list of test cases according to the specified criterias."""
  tests = parse_gtest_cases(gtest_list_tests(cmd, cwd), seed)

  # TODO(maruel): Splitting shards before filtering bad test cases could result
  # in inbalanced shards.
  if shards:
    tests = filter_shards(tests, index, shards)
  return chromium_filter_bad_tests(tests, disabled, fails, flaky, pre, manual)


class RunSome(object):
  """Thread-safe object deciding if testing should continue."""
  def __init__(
      self, expected_count, retries, min_failures, max_failure_ratio,
      max_failures):
    """Determines if it is better to give up testing after an amount of failures
    and successes.

    Arguments:
    - expected_count is the expected number of elements to run.
    - retries is how many time a failing element can be retried. retries should
      be set to the maximum number of retries per failure. This permits
      dampening the curve to determine threshold where to stop.
    - min_failures is the minimal number of failures to tolerate, to put a lower
      limit when expected_count is small. This value is multiplied by the number
      of retries.
    - max_failure_ratio is the ratio of permitted failures, e.g. 0.1 to stop
      after 10% of failed test cases.
    - max_failures is the absolute maximum number of tolerated failures or None.

    For large values of expected_count, the number of tolerated failures will be
    at maximum "(expected_count * retries) * max_failure_ratio".

    For small values of expected_count, the number of tolerated failures will be
    at least "min_failures * retries".
    """
    assert 0 < expected_count
    assert 0 <= retries < 100
    assert 0 <= min_failures
    assert 0. < max_failure_ratio < 1.
    # Constants.
    self._expected_count = expected_count
    self._retries = retries
    self._min_failures = min_failures
    self._max_failure_ratio = max_failure_ratio

    self._min_failures_tolerated = self._min_failures * (self._retries + 1)
    # Pre-calculate the maximum number of allowable failures. Note that
    # _max_failures can be lower than _min_failures.
    self._max_failures_tolerated = round(
        (expected_count * (retries + 1)) * max_failure_ratio)
    if max_failures is not None:
      # Override the ratio if necessary.
      self._max_failures_tolerated = min(
          self._max_failures_tolerated, max_failures)
      self._min_failures_tolerated = min(
          self._min_failures_tolerated, max_failures)

    # Variables.
    self._lock = threading.Lock()
    self._passed = 0
    self._failures = 0
    self.stopped = False

  def should_stop(self):
    """Stops once a threshold was reached. This includes retries."""
    with self._lock:
      if self.stopped:
        return True
      # Accept at least the minimum number of failures.
      if self._failures <= self._min_failures_tolerated:
        return False
      if self._failures >= self._max_failures_tolerated:
        self.stopped = True
      return self.stopped

  def got_result(self, passed):
    with self._lock:
      if passed:
        self._passed += 1
      else:
        self._failures += 1

  def __str__(self):
    return '%s(%d, %d, %d, %.3f)' % (
        self.__class__.__name__,
        self._expected_count,
        self._retries,
        self._min_failures,
        self._max_failure_ratio)


class RunAll(object):
  """Never fails."""
  stopped = False

  @staticmethod
  def should_stop():
    return False

  @staticmethod
  def got_result(_):
    pass


def process_output(lines, test_cases):
  """Yield the data of each test cases.

  Expects the test cases to be run in the order of the list.

  Handles the following google-test behavior:
    - Test case crash causing a partial number of test cases to be run.
    - Invalid test case name so the test case wasn't run at all.

  This function automatically distribute the startup cost across each test case.
  """
  test_cases = test_cases[:]
  test_case = None
  test_case_data = None
  terminated_early = False
  for line in lines:
    if not (test_case or test_cases):
      terminated_early = True
      break
    i = line.find(RUN_PREFIX)
    if i > 0 and test_case_data:
      # This may occur specifically in browser_tests, because the test case is
      # run in a child process. If the child process doesn't terminate its
      # output with a LF, it may cause the "[ RUN    ]" line to be improperly
      # printed out in the middle of a line.
      test_case_data['output'] += line[:i]
      line = line[i:]
      i = 0

    if i >= 0:
      if test_case:
        # The previous test case had crashed. No idea about its duration
        test_case_data['returncode'] = 1
        test_case_data['duration'] = 0
        test_case_data['crashed'] = True
        yield test_case_data

      test_case = line[len(RUN_PREFIX):].strip().split(' ', 1)[0]
      # Accept the test case even if it was unexpected.
      if test_case in test_cases:
        test_cases.remove(test_case)
      else:
        logging.warning('Unexpected test case: %s', test_case)
      test_case_data = {
        'test_case': test_case,
        'returncode': None,
        'duration': None,
        'output': line,
      }
    elif test_case:
      test_case_data['output'] += line
      if line.startswith((OK_PREFIX, FAILED_PREFIX)):
        # The test completed.
        match = re.search(r' \((\d+) ms\)', line)
        if match:
          test_case_data['duration'] = float(match.group(1)) / 1000.
        test_case_data['returncode'] = int(line.startswith(FAILED_PREFIX))
        yield test_case_data
        test_case = None
        test_case_data = None

  if test_case_data:
    assert not terminated_early
    # This means the last one likely crashed.
    test_case_data['crashed'] = True
    test_case_data['duration'] = 0
    test_case_data['returncode'] = 1
    yield test_case_data
  elif terminated_early:
    for _ in lines:
      # Exhaust the generator. It is necessary so that the input generator is
      # in a consistent state. More practically in our specific case, it is
      # necessary to ensure that yield_any() was called back again so that it
      # calls poll() and detects the process termination and/or wait for it. We
      # don't want this generator to quit before the process has terminated.
      pass

  # If test_cases is not empty, these test cases were not run.
  for t in test_cases:
    yield {
      'test_case': t,
      'returncode': None,
      'duration': None,
      'output': None,
    }


def normalize_testing_time(data, duration, returncode):
  """Normalizes the test case duration by spreading the total runtime.

  If a test crashed, he takes the whole startup cost.
  """
  # Alias the list 'data' into 'data_ran' for the results of test cases that
  # ran.
  data = [i.copy() for i in data]
  data_ran = [i for i in data if i['duration'] is not None]
  startup_duration = duration
  if data_ran:
    startup_duration -= sum(i['duration'] for i in data_ran)

  for i in reversed(data):
    if i['output'] is not None:
      if i.get('crashed'):
        i['duration'] = startup_duration
        i['returncode'] = returncode or i['returncode']
      else:
        # Distribute the one-time process startup cost across the test cases
        # that ran if the startup cost was above 10ms.
        if startup_duration > 0.01 and data_ran:
          distributed_duration = startup_duration / len(data_ran)
          for i in data_ran:
            i['duration'] += distributed_duration
        i['returncode'] = returncode or i['returncode']
      break
  return data


def convert_to_lines(generator):
  """Turn input coming from a generator into lines.

  It is Windows-friendly.
  """
  accumulator = ''
  for data in generator:
    items = (accumulator + data).splitlines(True)
    for item in items[:-1]:
      yield item
    if items[-1].endswith(('\r', '\n')):
      yield items[-1]
      accumulator = ''
    else:
      accumulator = items[-1]
  if accumulator:
    yield accumulator


def chromium_filter_tests(data):
  """Returns a generator that removes funky PRE_ chromium-specific tests."""
  return (d for d in data if chromium_valid(d['test_case'], False, True))


class Runner(object):
  """Immutable settings to run many test cases in a loop."""
  def __init__(
      self, cmd, cwd_dir, timeout, progress, retries, decider, verbose,
      add_task, add_serial_task):
    self.cmd = cmd[:]
    self.cwd_dir = cwd_dir
    self.timeout = timeout
    self.progress = progress
    # The number of retries. For example if 2, the test case will be tried 3
    # times in total.
    self.retries = retries
    self.decider = decider
    self.verbose = verbose
    self.add_task = add_task
    self.add_serial_task = add_serial_task
    # It is important to remove the shard environment variables since it could
    # conflict with --gtest_filter.
    self.env = setup_gtest_env()

  def map(self, priority, test_cases, try_count):
    """Traces a single test case and returns its output.

    try_count is 0 based, the original try is 0.
    """
    if self.decider.should_stop():
      return []

    cmd = self.cmd + ['--gtest_filter=%s' % ':'.join(test_cases)]
    if '--gtest_print_time' not in cmd:
      cmd.append('--gtest_print_time')

    # TODO(maruel): Use a distribution model.
    timeout = self.timeout * len(test_cases)

    if self.verbose > 1:
      self.progress.update_item('Starting command %s with a timeout of %ss' %
                                (cmd, timeout), False, False)

    # TODO(maruel): Differentiate between soft and hard timeouts.
    proc = Popen(
        cmd,
        cwd=self.cwd_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=self.env)
    # Create a pipeline of generators.
    gen_lines = convert_to_lines(data for _, data in proc.yield_any(timeout))
    # It needs to be valid utf-8 otherwise it can't be stored.
    # TODO(maruel): Be more intelligent than decoding to ascii.
    gen_lines_utf8 = (
      line.decode('ascii', 'ignore').encode('utf-8') for line in gen_lines)
    gen_test_cases = process_output(gen_lines_utf8, test_cases)
    gen_test_cases_filtered = chromium_filter_tests(gen_test_cases)

    data = []
    for i in gen_test_cases_filtered:
      if i['duration'] is None:
        continue
      # A new test_case completed.
      data.append(i)
      self.decider.got_result(i['returncode'] == 0)
      need_to_retry = i['returncode'] != 0 and try_count < self.retries
      if try_count:
        line = '%s (%.2fs) - retry #%d' % (
            i['test_case'], i['duration'] or 0, try_count)
      else:
        line = '%s (%.2fs)' % (i['test_case'], i['duration'] or 0)
      if self.verbose or i['returncode'] != 0 or try_count > 0:
        # Print output in one of three cases:
        #   --verbose was specified.
        #   The test failed.
        #   The wasn't the first attempt (this is needed so the test parser can
        #       detect that a test has been successfully retried).
        if i['output']:
          line += '\n' + i['output']
      self.progress.update_item(line, True, need_to_retry)

      if need_to_retry:
        if try_count + 1 < self.retries:
          # The test failed and needs to be retried normally.
          # Leave a buffer of ~40 test cases before retrying.
          priority += 40
          self.add_task(
              priority, self.map, priority, [i['test_case']], try_count + 1)
        else:
          # This test only has one retry left, so the final retry should be
          # done serially.
          self.add_serial_task(
              priority, self.map, priority, [i['test_case']], try_count + 1)

    if self.verbose > 1:
      self.progress.update_item(
          'Command %s finished after %ss' % (cmd, proc.duration()),
          False, False)

    return normalize_testing_time(data, proc.duration(), proc.returncode)


def get_test_cases(
    cmd, cwd, whitelist, blacklist, index, shards, seed, disabled, fails, flaky,
    manual):
  """Returns the filtered list of test cases.

  This is done synchronously.
  """
  try:
    # List all the test cases if a whitelist is used.
    tests = list_test_cases(
        cmd,
        cwd,
        index=index,
        shards=shards,
        disabled=disabled,
        fails=fails,
        flaky=flaky,
        pre=False,
        manual=manual,
        seed=seed)
  except Failure, e:
    print('Failed to list test cases')
    print(e.args[0])
    return None

  if shards:
    # This is necessary for Swarm log parsing.
    print('Note: This is test shard %d of %d.' % (index+1, shards))

  # Filters the test cases with the two lists.
  if blacklist:
    tests = [
      t for t in tests if not any(fnmatch.fnmatch(t, s) for s in blacklist)
    ]
  if whitelist:
    tests = [
      t for t in tests if any(fnmatch.fnmatch(t, s) for s in whitelist)
    ]
  logging.info('Found %d test cases in %s' % (len(tests), ' '.join(cmd)))
  return tests


def dump_results_as_json(result_file, results):
  """Write the results out to a json file."""
  base_path = os.path.dirname(result_file)
  if base_path and not os.path.isdir(base_path):
    os.makedirs(base_path)
  with open(result_file, 'wb') as f:
    json.dump(results, f, sort_keys=True, indent=2)


def dump_results_as_xml(gtest_output, results, now):
  """Write the results out to a xml file in google-test compatible format."""
  # TODO(maruel): Print all the test cases, including the ones that weren't run
  # and the retries. For now, ditch the failures from the retries, which is a
  # bit sad.
  test_suites = {}
  for test_case, result in results['test_cases'].iteritems():
    suite, case = test_case.split('.', 1)
    test_suites.setdefault(suite, {})[case] = result[-1]

  with open(gtest_output, 'wb') as f:
    # Sanity warning: hand-rolling XML. What could possibly go wrong?
    f.write('<?xml version="1.0" ?>\n')
    # TODO(maruel): File the fields nobody reads anyway.
    # disabled="%d" errors="%d" failures="%d"
    f.write(
        ('<testsuites name="AllTests" tests="%d" time="%f" timestamp="%s">\n')
        % (results['expected'], results['duration'], now))
    for suite_name, suite in test_suites.iteritems():
      # TODO(maruel): disabled="0" errors="0" failures="0" time="0"
      f.write('<testsuite name="%s" tests="%d">\n' % (suite_name, len(suite)))
      for case_name, case in suite.iteritems():
        if case['returncode'] == 0:
          f.write(
            '  <testcase classname="%s" name="%s" status="run" time="%f"/>\n' %
            (suite_name, case_name, case['duration']))
        else:
          f.write(
            '  <testcase classname="%s" name="%s" status="run" time="%f">\n' %
            (suite_name, case_name, case['duration']))
          # While at it, hand-roll CDATA escaping too.
          output = ']]><![CDATA['.join(case['output'].split(']]>'))
          # TODO(maruel): message="" type=""
          f.write('<failure><![CDATA[%s]]></failure></testcase>\n' % output)
      f.write('</testsuite>\n')
    f.write('</testsuites>')


def append_gtest_output_to_xml(final_xml, filepath):
  """Combines the shard xml file with the final xml file."""
  try:
    with open(filepath) as shard_xml_file:
      shard_xml = minidom.parse(shard_xml_file)
  except xml.parsers.expat.ExpatError as e:
    logging.error('Failed to parse %s: %s', filepath, e)
    return final_xml
  except IOError as e:
    logging.error('Failed to load %s: %s', filepath, e)
    # If the shard crashed, gtest will not have generated an xml file.
    return final_xml

  if not final_xml:
    # Out final xml is empty, let's prepopulate it with the first one we see.
    return shard_xml

  final_testsuites_by_name = dict(
      (suite.getAttribute('name'), suite)
      for suite in final_xml.documentElement.getElementsByTagName('testsuite'))

  for testcase in shard_xml.documentElement.getElementsByTagName('testcase'):
    # Don't bother updating the final xml if there is no data.
    status = testcase.getAttribute('status')
    if status == 'notrun':
      continue

    name = testcase.getAttribute('name')
    # Look in our final xml to see if it's there.
    to_remove = []
    final_testsuite = final_testsuites_by_name[
        testcase.getAttribute('classname')]
    for final_testcase in final_testsuite.getElementsByTagName('testcase'):
      # Trim all the notrun testcase instances to add the new instance there.
      # This is to make sure it works properly in case of a testcase being run
      # multiple times.
      if (final_testcase.getAttribute('name') == name and
          final_testcase.getAttribute('status') == 'notrun'):
        to_remove.append(final_testcase)

    for item in to_remove:
      final_testsuite.removeChild(item)
    # Reparent the XML node.
    final_testsuite.appendChild(testcase)

  return final_xml


def running_serial_warning():
  return ['*****************************************************',
          '*****************************************************',
          '*****************************************************',
          'WARNING: The remaining tests are going to be retried',
          'serially. All tests should be isolated and be able to pass',
          'regardless of what else is running.',
          'If you see a test that can only pass serially, that test is',
          'probably broken and should be fixed.',
          '*****************************************************',
          '*****************************************************',
          '*****************************************************']


def gen_gtest_output_dir(cwd, gtest_output):
  """Converts gtest_output to an actual path that can be used in parallel.

  Returns a 'corrected' gtest_output value.
  """
  if not gtest_output.startswith('xml'):
    raise Failure('Can\'t parse --gtest_output=%s' % gtest_output)
  # Figure out the result filepath in case we can't parse it, it'd be
  # annoying to error out *after* running the tests.
  if gtest_output == 'xml':
    gtest_output = os.path.join(cwd, 'test_detail.xml')
  else:
    match = re.match(r'xml\:(.+)', gtest_output)
    if not match:
      raise Failure('Can\'t parse --gtest_output=%s' % gtest_output)
    # If match.group(1) is an absolute path, os.path.join() will do the right
    # thing.
    if match.group(1).endswith((os.path.sep, '/')):
      gtest_output = os.path.join(cwd, match.group(1), 'test_detail.xml')
    else:
      gtest_output = os.path.join(cwd, match.group(1))

  base_path = os.path.dirname(gtest_output)
  if base_path and not os.path.isdir(base_path):
    os.makedirs(base_path)

  # Emulate google-test' automatic increasing index number.
  while True:
    try:
      # Creates a file exclusively.
      os.close(os.open(gtest_output, os.O_CREAT|os.O_EXCL|os.O_RDWR, 0666))
      # It worked, we are done.
      return gtest_output
    except OSError:
      pass
    logging.debug('%s existed', gtest_output)
    base, ext = os.path.splitext(gtest_output)
    match = re.match(r'^(.+?_)(\d+)$', base)
    if match:
      base = match.group(1) + str(int(match.group(2)) + 1)
    else:
      base = base + '_0'
    gtest_output = base + ext


def calc_cluster_default(num_test_cases, jobs):
  """Calculates a desired number for clusters depending on the number of test
  cases and parallel jobs.
  """
  if not num_test_cases:
    return 0
  chunks = 6 * jobs
  if chunks >= num_test_cases:
    # Too many chunks, use 1~5 test case per thread. Not enough to start
    # chunking.
    value = num_test_cases / jobs
  else:
    # Use chunks that are spread across threads.
    value = (num_test_cases + chunks - 1) / chunks
  # Limit to 10 test cases per cluster.
  return min(10, max(1, value))


def run_test_cases(
    cmd, cwd, test_cases, jobs, timeout, clusters, retries, run_all,
    max_failures, no_cr, gtest_output, result_file, verbose):
  """Runs test cases in parallel.

  Arguments:
    - cmd: command to run.
    - cwd: working directory.
    - test_cases: list of preprocessed test cases to run.
    - jobs: number of parallel execution threads to do.
    - timeout: individual test case timeout. Modulated when used with
      clustering.
    - clusters: number of test cases to lump together in a single execution. 0
      means the default automatic value which depends on len(test_cases) and
      jobs. Capped to len(test_cases) / jobs.
    - retries: number of times a test case can be retried.
    - run_all: If true, do not early return even if all test cases fail.
    - max_failures is the absolute maximum number of tolerated failures or None.
    - no_cr: makes output friendly to piped logs.
    - gtest_output: saves results as xml.
    - result_file: saves results as json.
    - verbose: print more details.

  It may run a subset of the test cases if too many test cases failed, as
  determined with max_failures, retries and run_all.
  """
  assert 0 <= retries <= 100000
  if not test_cases:
    return 0
  if run_all:
    decider = RunAll()
  else:
    # If 10% of test cases fail, just too bad.
    decider = RunSome(len(test_cases), retries, 2, 0.1, max_failures)

  if not clusters:
    clusters = calc_cluster_default(len(test_cases), jobs)
  else:
    # Limit the value.
    clusters = min(clusters, len(test_cases) / jobs)

  logging.debug('%d test cases with clusters of %d', len(test_cases), clusters)

  if gtest_output:
    gtest_output = gen_gtest_output_dir(cwd, gtest_output)
  progress = Progress(len(test_cases))
  serial_tasks = QueueWithProgress(0)
  serial_tasks.set_progress(progress)

  def add_serial_task(priority, func, *args, **kwargs):
    """Adds a serial task, to be executed later."""
    assert isinstance(priority, int)
    assert callable(func)
    serial_tasks.put((priority, func, args, kwargs))

  with ThreadPool(progress, jobs, jobs, len(test_cases)) as pool:
    runner = Runner(
        cmd, cwd, timeout, progress, retries, decider, verbose,
        pool.add_task, add_serial_task)
    function = runner.map
    progress.use_cr_only = not no_cr
    # Cluster the test cases right away.
    for i in xrange((len(test_cases) + clusters - 1) / clusters):
      cluster = test_cases[i*clusters : (i+1)*clusters]
      pool.add_task(i, function, i, cluster, 0)
    results = pool.join()

    # Retry any failed tests serially.
    if not serial_tasks.empty():
      progress.update_item('\n'.join(running_serial_warning()), index=False,
                           size=False)
      progress.print_update()

      while not serial_tasks.empty():
        _priority, func, args, kwargs = serial_tasks.get()
        results.append(func(*args, **kwargs))
        serial_tasks.task_done()
        progress.print_update()

      # Call join since that is a standard call once a queue has been emptied.
      serial_tasks.join()

    duration = time.time() - pool.tasks.progress.start

  cleaned = {}
  for map_run in results:
    for test_case_result in map_run:
      cleaned.setdefault(test_case_result['test_case'], []).append(
          test_case_result)
  results = cleaned

  # Total time taken to run each test case.
  test_case_duration = dict(
      (test_case, sum((i.get('duration') or 0) for i in item))
      for test_case, item in results.iteritems())

  # Classify the results
  success = []
  flaky = []
  fail = []
  nb_runs = 0
  for test_case in sorted(results):
    items = results[test_case]
    nb_runs += len(items)
    if not any(i['returncode'] == 0 for i in items):
      fail.append(test_case)
    elif len(items) > 1 and any(i['returncode'] == 0 for i in items):
      flaky.append(test_case)
    elif len(items) == 1 and items[0]['returncode'] == 0:
      success.append(test_case)
    else:
      # The test never ran.
      assert False, items
  missing = list(set(test_cases) - set(success) - set(flaky) - set(fail))

  saved = {
    'test_cases': results,
    'expected': len(test_cases),
    'success': success,
    'flaky': flaky,
    'fail': fail,
    'missing': missing,
    'duration': duration,
  }
  if result_file:
    dump_results_as_json(result_file, saved)
  if gtest_output:
    dump_results_as_xml(gtest_output, saved, datetime.datetime.now())
  sys.stdout.write('\n')
  if not results:
    return 1

  if flaky:
    print('Flaky tests:')
    for test_case in sorted(flaky):
      items = results[test_case]
      print('  %s (tried %d times)' % (test_case, len(items)))

  if fail:
    print('Failed tests:')
    for test_case in sorted(fail):
      print('  %s' % test_case)

  print('Summary:')
  if decider.should_stop():
    print('  ** STOPPED EARLY due to high failure rate **')
  output = [
    ('Success', success),
    ('Flaky', flaky),
    ('Fail', fail),
  ]
  if missing:
    output.append(('Missing', missing))
  total_expected = len(test_cases)
  for name, items in output:
    number = len(items)
    print(
        '  %7s: %4d %6.2f%% %7.2fs' % (
          name,
          number,
          number * 100. / total_expected,
          sum(test_case_duration.get(item, 0) for item in items)))
  print('  %.2fs Done running %d tests with %d executions. %.2f test/s' % (
      duration,
      len(results),
      nb_runs,
      nb_runs / duration if duration else 0))
  return int(bool(fail)) or decider.stopped


class OptionParserWithLogging(optparse.OptionParser):
  """Adds --verbose option."""
  def __init__(self, verbose=0, **kwargs):
    kwargs.setdefault('description', sys.modules['__main__'].__doc__)
    optparse.OptionParser.__init__(self, **kwargs)
    self.add_option(
        '-v', '--verbose',
        action='count',
        default=verbose,
        help='Use multiple times to increase verbosity')
    self.add_option(
        '-l', '--log_file',
        default=os.environ.get('RUN_TEST_CASES_LOG_FILE', ''),
        help='The name of the file to store rotating log details.')

  def parse_args(self, *args, **kwargs):
    options, args = optparse.OptionParser.parse_args(self, *args, **kwargs)
    level = ([logging.ERROR, logging.INFO, logging.DEBUG]
             [min(2, options.verbose)])

    logging_console = logging.StreamHandler()
    logging_console.setFormatter(logging.Formatter(
        '%(levelname)5s %(module)15s(%(lineno)3d): %(message)s'))
    logging_console.setLevel(level)
    logging.getLogger().setLevel(level)
    logging.getLogger().addHandler(logging_console)

    if options.log_file:
      logging_rotating_file = logging.handlers.RotatingFileHandler(
          options.log_file, maxBytes=10 * 1024 * 1024, backupCount=5)
      logging_rotating_file.setLevel(logging.DEBUG)
      logging.getLogger().setLevel(logging.DEBUG)
      logging_rotating_file.setFormatter(logging.Formatter(
          '%(asctime)s %(levelname)-8s %(module)15s(%(lineno)3d): %(message)s'))
      logging.getLogger().addHandler(logging_rotating_file)

    return options, args


class OptionParserWithTestSharding(OptionParserWithLogging):
  """Adds automatic handling of test sharding"""
  def __init__(self, **kwargs):
    OptionParserWithLogging.__init__(self, **kwargs)

    def as_digit(variable, default):
      return int(variable) if variable.isdigit() else default

    group = optparse.OptionGroup(self, 'Which shard to select')
    group.add_option(
        '-I', '--index',
        type='int',
        default=as_digit(os.environ.get('GTEST_SHARD_INDEX', ''), None),
        help='Shard index to select')
    group.add_option(
        '-S', '--shards',
        type='int',
        default=as_digit(os.environ.get('GTEST_TOTAL_SHARDS', ''), None),
        help='Total number of shards to calculate from the --index to select')
    self.add_option_group(group)

  def parse_args(self, *args, **kwargs):
    options, args = OptionParserWithLogging.parse_args(self, *args, **kwargs)
    if bool(options.shards) != bool(options.index is not None):
      self.error('Use both --index X --shards Y or none of them')
    return options, args


class OptionParserWithTestShardingAndFiltering(OptionParserWithTestSharding):
  """Adds automatic handling of test sharding and filtering."""
  def __init__(self, *args, **kwargs):
    OptionParserWithTestSharding.__init__(self, *args, **kwargs)

    group = optparse.OptionGroup(self, 'Which test cases to select')
    group.add_option(
        '-w', '--whitelist',
        default=[],
        action='append',
        help='filter to apply to test cases to run, wildcard-style, defaults '
        'to all test')
    group.add_option(
        '-b', '--blacklist',
        default=[],
        action='append',
        help='filter to apply to test cases to skip, wildcard-style, defaults '
        'to no test')
    group.add_option(
        '-T', '--test-case-file',
        help='File containing the exact list of test cases to run')
    group.add_option(
        '--gtest_filter',
        default=os.environ.get('GTEST_FILTER', ''),
        help='Select test cases like google-test does, separated with ":"')
    group.add_option(
        '--seed',
        type='int',
        default=os.environ.get('GTEST_RANDOM_SEED', '1'),
        help='Deterministically shuffle the test list if non-0. default: '
             '%default')
    group.add_option(
        '-d', '--disabled',
        action='store_true',
        default=int(os.environ.get('GTEST_ALSO_RUN_DISABLED_TESTS', '0')),
        help='Include DISABLED_ tests')
    group.add_option(
        '--gtest_also_run_disabled_tests',
        action='store_true',
        dest='disabled',
        help='same as --disabled')
    self.add_option_group(group)

    group = optparse.OptionGroup(
        self, 'Which test cases to select; chromium-specific')
    group.add_option(
        '-f', '--fails',
        action='store_true',
        help='Include FAILS_ tests')
    group.add_option(
        '-F', '--flaky',
        action='store_true',
        help='Include FLAKY_ tests')
    group.add_option(
        '-m', '--manual',
        action='store_true',
        help='Include MANUAL_ tests')
    group.add_option(
        '--run-manual',
        action='store_true',
        dest='manual',
        help='same as --manual')
    self.add_option_group(group)

  def parse_args(self, *args, **kwargs):
    options, args = OptionParserWithTestSharding.parse_args(
        self, *args, **kwargs)

    if options.gtest_filter:
      # Override any other option.
      # Based on UnitTestOptions::FilterMatchesTest() in
      # http://code.google.com/p/googletest/source/browse/#svn%2Ftrunk%2Fsrc
      if '-' in options.gtest_filter:
        options.whitelist, options.blacklist = options.gtest_filter.split('-',
                                                                          1)
      else:
        options.whitelist = options.gtest_filter
        options.blacklist = ''
      options.whitelist = [i for i in options.whitelist.split(':') if i]
      options.blacklist = [i for i in options.blacklist.split(':') if i]

    return options, args

  @staticmethod
  def process_gtest_options(cmd, cwd, options):
    """Grabs the test cases."""
    if options.test_case_file:
      with open(options.test_case_file, 'r') as f:
        # Do not shuffle or alter the file in any way in that case except to
        # strip whitespaces.
        return [l for l in (l.strip() for l in f) if l]
    else:
      return get_test_cases(
          cmd,
          cwd,
          options.whitelist,
          options.blacklist,
          options.index,
          options.shards,
          options.seed,
          options.disabled,
          options.fails,
          options.flaky,
          options.manual)


class OptionParserTestCases(OptionParserWithTestShardingAndFiltering):
  def __init__(self, *args, **kwargs):
    OptionParserWithTestShardingAndFiltering.__init__(self, *args, **kwargs)
    self.add_option(
        '-j', '--jobs',
        type='int',
        default=num_processors(),
        help='Number of parallel jobs; default=%default')
    self.add_option(
        '--use-less-jobs',
        action='store_const',
        const=num_processors() - 1,
        dest='jobs',
        help='Starts less parallel jobs than the default, used to help reduce'
             'contention between threads if all the tests are very CPU heavy.')
    self.add_option(
        '-t', '--timeout',
        type='int',
        default=75,
        help='Timeout for a single test case, in seconds default:%default')
    self.add_option(
        '--clusters',
        type='int',
        help='Number of test cases to cluster together, clamped to '
             'len(test_cases) / jobs; the default is automatic')


def process_args(argv):
  parser = OptionParserTestCases(
      usage='%prog <options> [gtest]',
      verbose=int(os.environ.get('ISOLATE_DEBUG', 0)))
  parser.add_option(
      '--run-all',
      action='store_true',
      help='Do not fail early when a large number of test cases fail')
  parser.add_option(
      '--max-failures', type='int',
      help='Limit the number of failures before aborting')
  parser.add_option(
      '--retries', type='int', default=2,
      help='Number of times each test case should be retried in case of '
           'failure.')
  parser.add_option(
      '--no-dump',
      action='store_true',
      help='do not generate a .run_test_cases file')
  parser.add_option(
      '--no-cr',
      action='store_true',
      help='Use LF instead of CR for status progress')
  parser.add_option(
      '--result',
      help='Override the default name of the generated .run_test_cases file')

  group = optparse.OptionGroup(parser, 'google-test compability flags')
  group.add_option(
      '--gtest_list_tests',
      action='store_true',
      help='List all the test cases unformatted. Keeps compatibility with the '
           'executable itself.')
  group.add_option(
      '--gtest_output',
      default=os.environ.get('GTEST_OUTPUT', ''),
      help='XML output to generate')
  parser.add_option_group(group)

  options, args = parser.parse_args(argv)

  if not args:
    parser.error(
        'Please provide the executable line to run, if you need fancy things '
        'like xvfb, start this script from *inside* xvfb, it\'ll be much faster'
        '.')

  if options.run_all and options.max_failures is not None:
    parser.error('Use only one of --run-all or --max-failures')
  return parser, options, fix_python_path(args)


def main(argv):
  """CLI frontend to validate arguments."""
  parser, options, cmd = process_args(argv)

  if options.gtest_list_tests:
    # Special case, return the output of the target unmodified.
    return subprocess.call(cmd + ['--gtest_list_tests'])

  cwd = os.getcwd()
  test_cases = parser.process_gtest_options(cmd, cwd, options)
  if not test_cases:
    # If test_cases is None then there was a problem generating the tests to
    # run, so this should be considered a failure.
    return int(test_cases is None)

  if options.no_dump:
    result_file = None
  else:
    result_file = options.result
    if not result_file:
      if cmd[0] == sys.executable:
        result_file = '%s.run_test_cases' % cmd[1]
      else:
        result_file = '%s.run_test_cases' % cmd[0]

  if options.disabled:
    cmd.append('--gtest_also_run_disabled_tests')
  if options.manual:
    cmd.append('--run-manual')

  try:
    return run_test_cases(
        cmd,
        cwd,
        test_cases,
        options.jobs,
        options.timeout,
        options.clusters,
        options.retries,
        options.run_all,
        options.max_failures,
        options.no_cr,
        options.gtest_output,
        result_file,
        options.verbose)
  except Failure as e:
    print >> sys.stderr, e.args[0]
    return 1


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
