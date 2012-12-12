#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs each test cases as a single shard, single process execution.

Similar to sharding_supervisor.py but finer grained. It runs each test case
individually instead of running per shard. Runs multiple instances in parallel.
"""

import fnmatch
import json
import logging
import optparse
import os
import Queue
import random
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from xml.dom import minidom

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
  # TODO(maruel): Handle.
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


def num_processors():
  """Returns the number of processors.

  Python on OSX 10.6 raises a NotImplementedError exception.
  """
  try:
    # Multiprocessing
    import multiprocessing
    return multiprocessing.cpu_count()
  except:  # pylint: disable=W0702
    # Mac OS 10.6
    return int(os.sysconf('SC_NPROCESSORS_ONLN'))  # pylint: disable=E1101


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
    if timeout:
      start = time.time()
    x = msvcrt.get_osfhandle(conn.fileno())
    try:
      while True:
        avail = min(PeekNamedPipe(x), maxsize)
        if avail:
          return ReadFile(x, avail)[1]
        if not timeout or (time.time() - start) >= timeout:
          return
        # Polling rocks.
        time.sleep(0.001)
    except OSError:
      # Not classy but fits our needs.
      return None

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


class Failure(Exception):
  pass


class Popen(subprocess.Popen):
  """Adds timeout support on stdout and stderr.

  Inspired by
  http://code.activestate.com/recipes/440554-module-to-allow-asynchronous-subprocess-use-on-win/
  """
  def recv(self, maxsize=None, timeout=None):
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
    start = time.time()
    output = ''
    while proc.poll() is None:
      remaining = max(timeout - (time.time() - start), 0.001)
      data = proc.recv(timeout=remaining)
      if data:
        output += data
      if (time.time() - start) >= timeout:
        break
    if (time.time() - start) >= timeout and proc.poll() is None:
      logging.debug('Kill %s %s' % ((time.time() - start) , timeout))
      proc.kill()
    proc.wait()
    # Try reading a last time.
    while True:
      data = proc.recv()
      if not data:
        break
      output += data
  else:
    # This code path is much faster.
    output = proc.communicate()[0]
  return output, proc.returncode


class QueueWithProgress(Queue.Queue):
  """Implements progress support in join()."""
  def __init__(self, maxsize, *args, **kwargs):
    Queue.Queue.__init__(self, *args, **kwargs)
    self.progress = Progress(maxsize)

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
      self.all_tasks_done.notify_all()
    finally:
      self.all_tasks_done.release()

  def join(self):
    """Calls print_update() whenever possible."""
    self.progress.print_update()
    self.all_tasks_done.acquire()
    try:
      while self.unfinished_tasks:
        self.progress.print_update()
        self.all_tasks_done.wait()
      self.progress.print_update()
    finally:
      self.all_tasks_done.release()


class ThreadPool(run_isolated.ThreadPool):
  QUEUE_CLASS = QueueWithProgress


class Progress(object):
  """Prints progress and accepts updates thread-safely."""
  def __init__(self, size):
    # To be used in the primary thread
    self.last_printed_line = ''
    self.index = 0
    self.start = time.time()
    self.size = size
    self.use_cr_only = True

    # To be used in all threads.
    self.queued_lines = Queue.Queue()

  def update_item(self, name, index=True, size=False):
    """Queue information to print out.

    |index| notes that the index should be incremented.
    |size| note that the total size should be incremented.
    """
    self.queued_lines.put((name, index, size))

  def print_update(self):
    """Prints the current status."""
    while True:
      try:
        name, index, size = self.queued_lines.get_nowait()
      except Queue.Empty:
        break

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
  try:
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         env=env, cwd=cwd)
  except OSError, e:
    raise Failure('Failed to run %s\ncwd=%s\n%s' % (' '.join(cmd), cwd, str(e)))
  out, err = p.communicate()
  if p.returncode:
    raise Failure(
        'Failed to run %s\nstdout:\n%s\nstderr:\n%s' %
          (' '.join(cmd), out, err), p.returncode)
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


def chromium_filter_bad_tests(tests, disabled, fails, flaky, pre, manual):
  """Filters out PRE_, MANUAL_, and other weird Chromium-specific test cases."""
  def starts_with(a, b, prefix):
    return a.startswith(prefix) or b.startswith(prefix)

  def valid(test):
    fixture, case = test.split('.', 1)
    if not pre and starts_with(fixture, case, 'PRE_'):
      return False
    if not manual and starts_with(fixture, case, 'MANUAL_'):
      return False
    if test == 'InProcessBrowserTest.Empty':
      return False
    return True

  tests = filter_bad_tests(tests, disabled, fails, flaky)
  return [test for test in tests if valid(test)]


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


class Runner(object):
  """Immutable settings to run many test cases in a loop."""
  def __init__(
      self, cmd, cwd_dir, timeout, progress, retries, decider, verbose,
      add_task):
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
    # It is important to remove the shard environment variables since it could
    # conflict with --gtest_filter.
    self.env = setup_gtest_env()

  def map(self, test_case, try_count):
    """Traces a single test case and returns its output.

    try_count is 0 based, the original try is 0.
    """
    if self.decider.should_stop():
      return []

    start = time.time()
    output, returncode = call_with_timeout(
        self.cmd + ['--gtest_filter=%s' % test_case],
        self.timeout,
        cwd=self.cwd_dir,
        stderr=subprocess.STDOUT,
        env=self.env)
    duration = time.time() - start
    data = {
      'test_case': test_case,
      'returncode': returncode,
      'duration': duration,
      # It needs to be valid utf-8 otherwise it can't be store.
      'output': output.decode('ascii', 'ignore').encode('utf-8'),
    }
    if '[ RUN      ]' not in output:
      # Can't find gtest marker, mark it as invalid.
      returncode = returncode or 1
    self.decider.got_result(not bool(returncode))
    if sys.platform == 'win32':
      output = output.replace('\r\n', '\n')
    need_to_retry = returncode and try_count < self.retries

    if try_count:
      line = '%s (%.2fs) - retry #%d' % (test_case, duration, try_count)
    else:
      line = '%s (%.2fs)' % (test_case, duration)
    if self.verbose or returncode:
      # Print output in one of two cases:
      #   --verbose was specified.
      #   The test failed.
      line += '\n' + output
    self.progress.update_item(line, True, need_to_retry)

    if need_to_retry:
      # The test failed and needs to be retried..
      self.add_task(self.map, test_case, try_count + 1)
    return [data]


def get_test_cases(cmd, cwd, whitelist, blacklist, index, shards, seed):
  """Returns the filtered list of test cases.

  This is done synchronously.
  """
  try:
    tests = list_test_cases(
        cmd,
        cwd,
        index=index,
        shards=shards,
        disabled=False,
        fails=False,
        flaky=False,
        pre=False,
        manual=False,
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


def LogResults(result_file, results):
  """Write the results out to a file if one is given."""
  if not result_file:
    return
  base_path = os.path.dirname(result_file)
  if base_path and not os.path.isdir(base_path):
    os.makedirs(base_path)
  with open(result_file, 'wb') as f:
    json.dump(results, f, sort_keys=True, indent=2)


def append_gtest_output_to_xml(final_xml, filepath):
  """Combines the shard xml file with the final xml file."""
  try:
    with open(filepath) as shard_xml_file:
      shard_xml = minidom.parse(shard_xml_file)
  except IOError:
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


def run_test_cases(
    cmd, cwd, test_cases, jobs, timeout, retries, run_all, max_failures,
    no_cr, gtest_output, result_file, verbose):
  """Traces test cases one by one."""
  if not test_cases:
    return 0
  if run_all:
    decider = RunAll()
  else:
    # If 10% of test cases fail, just too bad.
    decider = RunSome(len(test_cases), retries, 2, 0.1, max_failures)

  tempdir = None
  try:
    if gtest_output:
      if gtest_output.startswith('xml'):
        # Have each shard write an XML file and them merge them all.
        tempdir = tempfile.mkdtemp(prefix='run_test_cases')
        cmd.append('--gtest_output=' + tempdir)
        # Figure out the result filepath in case we can't parse it, it'd be
        # annoying to error out after running the tests.
        if gtest_output == 'xml':
          gtest_output = os.path.join(cwd, 'test_detail.xml')
        else:
          match = re.match(r'xml\:(.+)', gtest_output)
          if not match:
            print >> sys.stderr, 'Can\'t parse --gtest_output=%s' % gtest_output
            return 1
          gtest_output = os.path.join(cwd, match.group(1))
      else:
        print >> sys.stderr, 'Can\'t parse --gtest_output=%s' % gtest_output
        return 1

    with ThreadPool(jobs, len(test_cases)) as pool:
      runner = Runner(
          cmd, cwd, timeout, pool.tasks.progress, retries, decider, verbose,
          pool.add_task)
      function = runner.map
      logging.debug('Adding tests to ThreadPool')
      pool.tasks.progress.use_cr_only = not no_cr
      for test_case in test_cases:
        pool.add_task(function, test_case, 0)
      logging.debug('All tests added to the ThreadPool')
      results = pool.join()
      duration = time.time() - pool.tasks.progress.start

    # Merges the XMLs into one before having the directory deleted.
    # TODO(maruel): Use two threads?
    if gtest_output:
      result = None
      for i in sorted(os.listdir(tempdir)):
        result = append_gtest_output_to_xml(result, os.path.join(tempdir, i))

      if result:
        base_path = os.path.dirname(gtest_output)
        if base_path and not os.path.isdir(base_path):
          os.makedirs(base_path)
        if os.path.isdir(gtest_output):
          # Includes compatibility with with google-test when a directory is
          # specified.
          # TODO(maruel): It would automatically add 0, 1, 2 when a previous
          # one exists.
          gtest_output = os.path.join(gtest_output, 'test_detail.xml')
        with open(gtest_output, 'w') as f:
          result.writexml(f)
      else:
        logging.error('Didn\'t find any XML file to write to %s' % gtest_output)
  finally:
    if tempdir:
      shutil.rmtree(tempdir)

  cleaned = {}
  for item in results:
    if item:
      cleaned.setdefault(item[0]['test_case'], []).extend(item)
  results = cleaned

  # Total time taken to run each test case.
  test_case_duration = dict(
      (test_case, sum(i.get('duration', 0) for i in item))
      for test_case, item in results.iteritems())

  # Classify the results
  success = []
  flaky = []
  fail = []
  nb_runs = 0
  for test_case in sorted(results):
    items = results[test_case]
    nb_runs += len(items)
    if not any(not i['returncode'] for i in items):
      fail.append(test_case)
    elif len(items) > 1 and any(not i['returncode'] for i in items):
      flaky.append(test_case)
    elif len(items) == 1 and items[0]['returncode'] == 0:
      success.append(test_case)
    else:
      assert False, items

  saved = {
    'test_cases': results,
    'success': success,
    'flaky': flaky,
    'fail': fail,
    'duration': duration,
  }
  LogResults(result_file, saved)
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
  missing = set(test_cases) - set(success) - set(flaky) - set(fail)
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
    logging.getLogger().addHandler(logging_console)

    if options.log_file:
      logging_rotating_file = logging.handlers.RotatingFileHandler(
          options.log_file, maxBytes=10 * 1024 * 1024, backupCount=5)
      logging_rotating_file.setLevel(logging.DEBUG)
      logging_rotating_file.setFormatter(logging.Formatter(
          '%(asctime)s %(levelname)-8s %(module)15s(%(lineno)3d): %(message)s'))
      logging.getLogger().addHandler(logging_rotating_file)

    logging.getLogger().setLevel(logging.DEBUG)

    return options, args


class OptionParserWithTestSharding(OptionParserWithLogging):
  """Adds automatic handling of test sharding"""
  def __init__(self, **kwargs):
    OptionParserWithLogging.__init__(self, **kwargs)

    def as_digit(variable, default):
      return int(variable) if variable.isdigit() else default

    group = optparse.OptionGroup(self, 'Which shard to run')
    group.add_option(
        '-I', '--index',
        type='int',
        default=as_digit(os.environ.get('GTEST_SHARD_INDEX', ''), None),
        help='Shard index to run')
    group.add_option(
        '-S', '--shards',
        type='int',
        default=as_digit(os.environ.get('GTEST_TOTAL_SHARDS', ''), None),
        help='Total number of shards to calculate from the --index to run')
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

    group = optparse.OptionGroup(self, 'Which test cases to run')
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
        help='Runs a single test, provideded to keep compatibility with '
        'other tools')
    group.add_option(
        '--seed',
        type='int',
        default=os.environ.get('GTEST_RANDOM_SEED', '1'),
        help='Deterministically shuffle the test list if non-0. default: '
             '%default')
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
          options.seed)


class OptionParserTestCases(OptionParserWithTestShardingAndFiltering):
  def __init__(self, *args, **kwargs):
    OptionParserWithTestShardingAndFiltering.__init__(self, *args, **kwargs)
    self.add_option(
        '-j', '--jobs',
        type='int',
        default=num_processors(),
        help='number of parallel jobs; default=%default')
    self.add_option(
        '-t', '--timeout',
        type='int',
        default=120,
        help='Timeout for a single test case, in seconds default:%default')


def main(argv):
  """CLI frontend to validate arguments."""
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

  cmd = fix_python_path(args)

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
    result_file = options.result or '%s.run_test_cases' % args[-1]

  return run_test_cases(
      cmd,
      cwd,
      test_cases,
      options.jobs,
      options.timeout,
      options.retries,
      options.run_all,
      options.max_failures,
      options.no_cr,
      options.gtest_output,
      result_file,
      options.verbose)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
