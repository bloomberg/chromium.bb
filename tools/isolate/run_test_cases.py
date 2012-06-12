#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs each test cases as a single shard, single process execution.

Similar to sharding_supervisor.py but finer grained. Runs multiple instances in
parallel.
"""

import fnmatch
import logging
import multiprocessing
import optparse
import os
import subprocess
import sys
import time

import list_test_cases
import trace_inputs
import worker_pool

TOOLS_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, TOOLS_DIR)

import find_depot_tools  # pylint: disable=F0401,W0611

import subprocess2


def call_with_timeout(cmd, cwd, timeout):
  """Runs an executable with an optional timeout."""
  if timeout:
    # This code path is much slower so only use it if necessary.
    try:
      output = subprocess2.check_output(
          cmd,
          cwd=cwd,
          timeout=timeout,
          stderr=subprocess2.STDOUT)
      return output, 0
    except subprocess2.CalledProcessError, e:
      return e.stdout, e.returncode
  else:
    # This code path is much faster.
    proc = subprocess.Popen(
        cmd, cwd=cwd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    output = proc.communicate()[0]
    return output, proc.returncode


class Runner(object):
  def __init__(self, executable, cwd_dir, timeout, progress):
    # Constants
    self.executable = executable
    self.cwd_dir = cwd_dir
    self.timeout = timeout
    self.progress = progress
    self.retry_count = 3

  def map(self, test_case):
    """Traces a single test case and returns its output."""
    cmd = [self.executable, '--gtest_filter=%s' % test_case]

    out = []
    for retry in range(self.retry_count):
      start = time.time()
      output, returncode = call_with_timeout(cmd, self.cwd_dir, self.timeout)
      duration = time.time() - start
      out.append(
          {
            'test_case': test_case,
            'returncode': returncode,
            'duration': duration,
            'output': output,
          })
      if returncode and retry != self.retry_count - 1:
        self.progress.increase_count()
      if retry:
        self.progress.update_item('%s - %d' % (test_case, retry))
      else:
        self.progress.update_item(test_case)
      if not returncode:
        break
    return out


def get_test_cases(executable, whitelist, blacklist):
  """Returns the filtered list of test cases.

  This is done synchronously.
  """
  try:
    out = list_test_cases.gtest_list_tests(executable)
  except list_test_cases.Failure, e:
    print e.args[0]
    return None

  tests = list_test_cases.parse_gtest_cases(out)

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


def run_test_cases(
    executable, whitelist, blacklist, jobs, timeout, stats_only, no_dump):
  """Traces test cases one by one."""
  test_cases = get_test_cases(executable, whitelist, blacklist)
  if not test_cases:
    return

  progress = worker_pool.Progress(len(test_cases))
  with worker_pool.ThreadPool(jobs or multiprocessing.cpu_count()) as pool:
    function = Runner(executable, os.getcwd(), timeout, progress).map
    for test_case in test_cases:
      pool.add_task(function, test_case)
    results = pool.join(progress, 0.1)
    duration = time.time() - progress.start
  results = dict((item[0]['test_case'], item) for item in results)
  if not no_dump:
    trace_inputs.write_json('%s.run_test_cases' % executable, results, False)
  sys.stderr.write('\n')
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

  if not stats_only:
    for test_case in sorted(fail):
      # Failed, print the last one:
      items = results[test_case]
      print items[-1]['output']

    for test_case in sorted(flaky):
      items = results[test_case]
      print '%s is flaky (tried %d times)' % (test_case, len(items))

  print 'Success: %4d %5.2f%%' % (len(success), len(success) * 100. / total)
  print 'Flaky:   %4d %5.2f%%' % (len(flaky), len(flaky) * 100. / total)
  print 'Fail:    %4d %5.2f%%' % (len(fail), len(fail) * 100. / total)
  print '%.1fs Done running %d tests with %d executions. %.1f test/s' % (
      duration,
      len(results),
      nb_runs,
      nb_runs / duration)
  return 0


def main():
  """CLI frontend to validate arguments."""
  parser = optparse.OptionParser(usage='%prog <options> [gtest]')
  parser.add_option(
      '-w', '--whitelist',
      default=[],
      action='append',
      help='filter to apply to test cases to run, wildcard-style, defaults to '
           'all test')
  parser.add_option(
      '-b', '--blacklist',
      default=[],
      action='append',
      help='filter to apply to test cases to skip, wildcard-style, defaults to '
           'no test')
  parser.add_option(
      '-j', '--jobs',
      type='int',
      help='number of parallel jobs')
  parser.add_option(
      '-t', '--timeout',
      type='int',
      help='Timeout for a single test case, in seconds default:%default')
  parser.add_option(
      '-s', '--stats',
      action='store_true',
      help='Only prints stats, not output')
  parser.add_option(
      '-v', '--verbose',
      action='count',
      default=int(os.environ.get('ISOLATE_DEBUG', 0)),
      help='Use multiple times')
  parser.add_option(
      '--no-dump',
      action='store_true',
      help='do not generate a .test_cases file')
  options, args = parser.parse_args()
  levels = [logging.ERROR, logging.WARNING, logging.INFO, logging.DEBUG]
  logging.basicConfig(
      level=levels[min(len(levels)-1, options.verbose)],
      format='%(levelname)5s %(module)15s(%(lineno)3d): %(message)s')

  if len(args) != 1:
    parser.error(
        'Please provide the executable line to run, if you need fancy things '
        'like xvfb, start this script from *inside* xvfb, it\'ll be much faster'
        '.')
  return run_test_cases(
      args[0],
      options.whitelist,
      options.blacklist,
      options.jobs,
      options.timeout,
      options.stats,
      options.no_dump)


if __name__ == '__main__':
  sys.exit(main())
