#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs each test cases as a single shard, single process execution.

Similar to sharding_supervisor.py but finer grained. Runs multiple instances in
parallel.
"""

import fnmatch
import json
import logging
import optparse
import os
import Queue
import subprocess
import sys
import threading
import time


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
  # TODO(maruel): Handle.
  'GTEST_OUTPUT',
  # TODO(maruel): Handle.
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
    return int(os.sysconf('SC_NPROCESSORS_ONLN'))


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
  import fcntl
  import select

  def recv_impl(conn, maxsize, timeout):
    """Reads from a pipe without blocking."""
    if not select.select([conn], [], [], timeout)[0]:
      return None

    # Temporarily make it non-blocking.
    flags = fcntl.fcntl(conn, fcntl.F_GETFL)
    if not conn.closed:
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


class QueueWithTimeout(Queue.Queue):
  """Implements timeout support in join()."""

  # QueueWithTimeout.join: Arguments number differs from overridden method
  # pylint: disable=W0221
  def join(self, timeout=None):
    """Returns True if all tasks are finished."""
    if not timeout:
      return Queue.Queue.join(self)
    start = time.time()
    self.all_tasks_done.acquire()
    try:
      while self.unfinished_tasks:
        remaining = time.time() - start - timeout
        if remaining <= 0:
          break
        self.all_tasks_done.wait(remaining)
      return not self.unfinished_tasks
    finally:
      self.all_tasks_done.release()


class WorkerThread(threading.Thread):
  """Keeps the results of each task in a thread-local outputs variable."""
  def __init__(self, tasks, *args, **kwargs):
    super(WorkerThread, self).__init__(*args, **kwargs)
    self._tasks = tasks
    self.outputs = []
    self.exceptions = []

    self.daemon = True
    self.start()

  def run(self):
    """Runs until a None task is queued."""
    while True:
      task = self._tasks.get()
      if task is None:
        # We're done.
        return
      try:
        func, args, kwargs = task
        self.outputs.append(func(*args, **kwargs))
      except Exception, e:
        logging.error('Caught exception! %s' % e)
        self.exceptions.append(sys.exc_info())
      finally:
        self._tasks.task_done()


class ThreadPool(object):
  """Implements a multithreaded worker pool oriented for mapping jobs with
  thread-local result storage.
  """
  def __init__(self, num_threads):
    self._tasks = QueueWithTimeout()
    self._workers = [
      WorkerThread(self._tasks, name='worker-%d' % i)
      for i in range(num_threads)
    ]

  def add_task(self, func, *args, **kwargs):
    """Adds a task, a function to be executed by a worker.

    The function's return value will be stored in the the worker's thread local
    outputs list.
    """
    self._tasks.put((func, args, kwargs))

  def join(self, progress=None, timeout=None):
    """Extracts all the results from each threads unordered."""
    if progress and timeout:
      while not self._tasks.join(timeout):
        progress.print_update()
      progress.print_update()
    else:
      self._tasks.join()
    out = []
    for w in self._workers:
      if w.exceptions:
        raise w.exceptions[0][0], w.exceptions[0][1], w.exceptions[0][2]
      out.extend(w.outputs)
      w.outputs = []
    # Look for exceptions.
    return out

  def close(self):
    """Closes all the threads."""
    for _ in range(len(self._workers)):
      # Enqueueing None causes the worker to stop.
      self._tasks.put(None)
    for t in self._workers:
      t.join()

  def __enter__(self):
    """Enables 'with' statement."""
    return self

  def __exit__(self, exc_type, exc_value, traceback):
    """Enables 'with' statement."""
    self.close()


class Progress(object):
  """Prints progress and accepts updates thread-safely."""
  def __init__(self, size):
    # To be used in the primary thread
    self.last_printed_line = ''
    self.index = 0
    self.start = time.time()
    self.size = size

    # To be used in all threads.
    self.queued_lines = Queue.Queue()

  def update_item(self, name, index=True, size=False):
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
        # Fill it with whitespace.
        # TODO(maruel): Read the console width when prossible and trim
        # next_line.
        # TODO(maruel): When not a console is used, do not fill with whitepace
        # but use \n instead.
        prefix = '\r' if self.last_printed_line else ''
        line = '%s%s%s' % (
            prefix,
            next_line,
            ' ' * max(0, len(self.last_printed_line) - len(next_line)))
        self.last_printed_line = next_line
      else:
        line = '\n%s\n' % name.strip('\n')
        self.last_printed_line = ''

      sys.stdout.write(line)


def fix_python_path(cmd):
  """Returns the fixed command line to call the right python executable."""
  out = cmd[:]
  if out[0] == 'python':
    out[0] = sys.executable
  elif out[0].endswith('.py'):
    out.insert(0, sys.executable)
  return out


def setup_gtest_env():
  """Copy the enviroment variables and setup for running a gtest."""
  env = os.environ.copy()
  for name in GTEST_ENV_VARS_TO_REMOVE:
    env.pop(name, None)

  # Forcibly enable color by default, if not already disabled.
  env.setdefault('GTEST_COLOR', 'on')

  return env


def gtest_list_tests(executable):
  """List all the test cases for a google test.

  See more info at http://code.google.com/p/googletest/.
  """
  cmd = [executable, '--gtest_list_tests']
  cmd = fix_python_path(cmd)
  env = setup_gtest_env()
  try:
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         env=env)
  except OSError, e:
    raise Failure('Failed to run %s\n%s' % (executable, str(e)))
  out, err = p.communicate()
  if p.returncode:
    raise Failure('Failed to run %s\nstdout:\n%s\nstderr:\n%s' %
                  (executable, out, err), p.returncode)
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


def filter_bad_tests(tests, disabled=False, fails=False, flaky=False):
  """Filters out DISABLED_, FAILS_ or FLAKY_ tests."""
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


def parse_gtest_cases(out):
  """Expected format is a concatenation of this:
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
  return tests


def list_test_cases(executable, index, shards, disabled, fails, flaky):
  """Returns the list of test cases according to the specified criterias."""
  tests = parse_gtest_cases(gtest_list_tests(executable))
  if shards:
    tests = filter_shards(tests, index, shards)
  return filter_bad_tests(tests, disabled, fails, flaky)


class RunSome(object):
  """Thread-safe object deciding if testing should continue."""
  def __init__(self, expected_count, retries, min_failures, max_failure_ratio):
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
    - max_failure_ratio is the the ratio of permitted failures, e.g. 0.1 to stop
      after 10% of failed test cases.

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

    self._min_failures_tolerated = self._min_failures * self._retries
    # Pre-calculate the maximum number of allowable failures. Note that
    # _max_failures can be lower than _min_failures.
    self._max_failures_tolerated = round(
        (expected_count * retries) * max_failure_ratio)

    # Variables.
    self._lock = threading.Lock()
    self._passed = 0
    self._failures = 0

  def should_stop(self):
    """Stops once a threshold was reached. This includes retries."""
    with self._lock:
      # Accept at least the minimum number of failures.
      if self._failures <= self._min_failures_tolerated:
        return False
      return self._failures >= self._max_failures_tolerated

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
  @staticmethod
  def should_stop():
    return False
  @staticmethod
  def got_result(_):
    pass


class Runner(object):
  def __init__(
      self, executable, cwd_dir, timeout, progress, retry_count, decider):
    # Constants
    self.executable = executable
    self.cwd_dir = cwd_dir
    self.timeout = timeout
    self.progress = progress
    self.retry_count = retry_count
    # It is important to remove the shard environment variables since it could
    # conflict with --gtest_filter.
    self.env = setup_gtest_env()
    self.decider = decider

  def map(self, test_case):
    """Traces a single test case and returns its output."""
    cmd = [self.executable, '--gtest_filter=%s' % test_case]
    cmd = fix_python_path(cmd)
    out = []
    for retry in range(self.retry_count):
      if self.decider.should_stop():
        break

      start = time.time()
      output, returncode = call_with_timeout(
          cmd,
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
      out.append(data)
      if sys.platform == 'win32':
        output = output.replace('\r\n', '\n')
      size = returncode and retry != self.retry_count - 1
      if retry:
        self.progress.update_item(
            '%s (%.2fs) - retry #%d' % (test_case, duration, retry),
            True,
            size)
      else:
        self.progress.update_item(
            '%s (%.2fs)' % (test_case, duration), True, size)
      if logging.getLogger().isEnabledFor(logging.INFO):
        self.progress.update_item(output, False, False)
      if not returncode:
        break
    else:
      # The test failed. Print its output. No need to print it with logging
      # level at INFO since it was already printed above.
      if not logging.getLogger().isEnabledFor(logging.INFO):
        self.progress.update_item(output, False, False)
    return out


def get_test_cases(executable, whitelist, blacklist, index, shards):
  """Returns the filtered list of test cases.

  This is done synchronously.
  """
  try:
    tests = list_test_cases(executable, index, shards, False, False, False)
  except Failure, e:
    print e.args[0]
    return None

  if shards:
    # This is necessary for Swarm log parsing.
    print 'Note: This is test shard %d of %d.' % (index+1, shards)

  # Filters the test cases with the two lists.
  if blacklist:
    tests = [
      t for t in tests if not any(fnmatch.fnmatch(t, s) for s in blacklist)
    ]
  if whitelist:
    tests = [
      t for t in tests if any(fnmatch.fnmatch(t, s) for s in whitelist)
    ]
  logging.info(
      'Found %d test cases in %s' % (len(tests), os.path.basename(executable)))
  return tests


def LogResults(result_file, results):
  """Write the results out to a file if one is given."""
  if not result_file:
    return
  with open(result_file, 'wb') as f:
    json.dump(results, f, sort_keys=True, indent=2)


def run_test_cases(executable, test_cases, jobs, timeout, run_all, result_file):
  """Traces test cases one by one."""
  if not test_cases:
    return 0
  progress = Progress(len(test_cases))
  retries = 3
  if run_all:
    decider = RunAll()
  else:
    # If 10% of test cases fail, just too bad.
    decider = RunSome(len(test_cases), retries, 2, 0.1)
  with ThreadPool(jobs) as pool:
    function = Runner(
        executable, os.getcwd(), timeout, progress, retries, decider).map
    for test_case in test_cases:
      pool.add_task(function, test_case)
    results = pool.join(progress, 0.1)
    duration = time.time() - progress.start
  results = dict((item[0]['test_case'], item) for item in results if item)
  LogResults(result_file, results)
  sys.stdout.write('\n')
  total = len(results)
  if not total:
    return 1

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

  print 'Summary:'
  for test_case in sorted(flaky):
    items = results[test_case]
    print '%s is flaky (tried %d times)' % (test_case, len(items))

  for test_case in sorted(fail):
    print '%s failed' % (test_case)

  if decider.should_stop():
    print '** STOPPED EARLY due to high failure rate **'
  print 'Success: %4d %5.2f%%' % (len(success), len(success) * 100. / total)
  print 'Flaky:   %4d %5.2f%%' % (len(flaky), len(flaky) * 100. / total)
  print 'Fail:    %4d %5.2f%%' % (len(fail), len(fail) * 100. / total)
  print '%.1fs Done running %d tests with %d executions. %.1f test/s' % (
      duration,
      len(results),
      nb_runs,
      nb_runs / duration)
  return int(bool(fail))


class OptionParserWithTestSharding(optparse.OptionParser):
  """Adds automatic handling of test sharding"""
  def __init__(self, *args, **kwargs):
    optparse.OptionParser.__init__(self, *args, **kwargs)

    def as_digit(variable, default):
      return int(variable) if variable.isdigit() else default

    group = optparse.OptionGroup(self, 'Which shard to run')
    group.add_option(
        '-i', '--index',
        type='int',
        default=as_digit(os.environ.get('GTEST_SHARD_INDEX', ''), None),
        help='Shard index to run')
    group.add_option(
        '-s', '--shards',
        type='int',
        default=as_digit(os.environ.get('GTEST_TOTAL_SHARDS', ''), None),
        help='Total number of shards to calculate from the --index to run')
    self.add_option_group(group)

  def parse_args(self, *args, **kwargs):
    options, args = optparse.OptionParser.parse_args(self, *args, **kwargs)
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


def main(argv):
  """CLI frontend to validate arguments."""
  parser = OptionParserWithTestShardingAndFiltering(
      usage='%prog <options> [gtest]')
  parser.add_option(
      '-j', '--jobs',
      type='int',
      default=num_processors(),
      help='number of parallel jobs; default=%default')
  parser.add_option(
      '-t', '--timeout',
      type='int',
      default=120,
      help='Timeout for a single test case, in seconds default:%default')
  parser.add_option(
      '-v', '--verbose',
      action='count',
      default=int(os.environ.get('ISOLATE_DEBUG', 0)),
      help='Use multiple times')
  parser.add_option(
      '--run-all',
      action='store_true',
      default=bool(int(os.environ.get('RUN_TEST_CASES_RUN_ALL', '0'))),
      help='Do not fail early when a large number of test cases fail')
  parser.add_option(
      '--no-dump',
      action='store_true',
      help='do not generate a .run_test_cases file')
  parser.add_option(
      '--result',
      default=os.environ.get('RUN_TEST_CASES_RESULT_FILE', ''),
      help='Override the default name of the generated .run_test_cases file')
  options, args = parser.parse_args(argv)

  levels = [logging.ERROR, logging.INFO, logging.DEBUG]
  logging.basicConfig(
      level=levels[min(len(levels)-1, options.verbose)],
      format='%(levelname)5s %(module)15s(%(lineno)3d): %(message)s')

  if len(args) != 1:
    parser.error(
        'Please provide the executable line to run, if you need fancy things '
        'like xvfb, start this script from *inside* xvfb, it\'ll be much faster'
        '.')

  executable = args[0]
  if not os.path.isabs(executable):
    executable = os.path.abspath(executable)
  if sys.platform in ('cygwin', 'win32'):
    if not os.path.splitext(executable)[1]:
      executable += '.exe'
  if not os.path.isfile(executable):
    parser.error('"%s" doesn\'t exist.' % executable)

  # Grab the test cases.
  if options.test_case_file:
    with open(options.test_case_file, 'r') as f:
      test_cases = filter(None, f.read().splitlines())
  else:
    test_cases = get_test_cases(
        executable,
        options.whitelist,
        options.blacklist,
        options.index,
        options.shards)

  if not test_cases:
    # If test_cases is None then there was a problem generating the tests to
    # run, so this should be considered a failure.
    return int(test_cases is None)

  if options.no_dump:
    result_file = None
  else:
    if options.result:
      result_file = options.result
    else:
      result_file = '%s.run_test_cases' % executable

  return run_test_cases(
      executable,
      test_cases,
      options.jobs,
      options.timeout,
      options.run_all,
      result_file)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
