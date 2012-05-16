#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A manual version of trace_inputs.py that is specialized in tracing each
google-test test case individually.

This is mainly written to work around bugs in strace w.r.t. browser_tests.
"""

import fnmatch
import json
import multiprocessing
import optparse
import os
import sys
import tempfile
import time

import list_test_cases
import trace_inputs

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(os.path.dirname(BASE_DIR))


def trace_test_case(
    test_case, executable, root_dir, cwd_dir, product_dir, leak):
  """Traces a single test case and returns the .isolate compatible variable
  dict.
  """
  # Resolve any symlink
  root_dir = os.path.realpath(root_dir)

  api = trace_inputs.get_api()
  cmd = [executable, '--gtest_filter=%s' % test_case]

  if not leak:
    f, logname = tempfile.mkstemp(prefix='trace')
    os.close(f)
  else:
    logname = '%s.%s.log' % (executable, test_case.replace('/', '-'))
    f = None

  try:
    simplified = None
    for i in range(10):
      start = time.time()
      returncode, output = trace_inputs.trace(
          logname, cmd, os.path.join(root_dir, cwd_dir), api, True)
      if returncode and i < 5:
        print '\nFailed while running: %s' % ' '.join(cmd)
        continue
      duration = time.time() - start
      try:
        _, _, _, _, simplified = trace_inputs.load_trace(logname, root_dir, api)
        break
      except Exception:
        print '\nFailed loading the trace for: %s' % ' '.join(cmd)
    if simplified:
      variables = trace_inputs.generate_dict(simplified, cwd_dir, product_dir)
    else:
      variables = {}
    return {
      'case': test_case,
      'variables': variables,
      'result': returncode,
      'duration': duration,
      'output': output,
    }
  finally:
    if f:
      os.remove(logname)


def task(args):
  """Adaptor for multiprocessing.Pool().imap_unordered().

  It is executed asynchronously.
  """
  return trace_test_case(*args)


def get_test_cases(executable, skip):
  """Returns the filtered list of test cases.

  This is done synchronously.
  """
  try:
    out = list_test_cases.gtest_list_tests(executable)
  except list_test_cases.Failure, e:
    print e.args[0]
    return None

  tests = list_test_cases.parse_gtest_cases(out)
  tests = [t for t in tests if not any(fnmatch.fnmatch(t, s) for s in skip)]
  print 'Found %d test cases in %s' % (len(tests), os.path.basename(executable))
  return tests


def trace_test_cases(
    executable, root_dir, cwd_dir, product_dir, leak, skip, jobs, timeout):
  """Traces test cases one by one."""
  tests = get_test_cases(executable, skip)
  if not tests:
    return

  last_line = ''
  out = {}
  index = 0
  pool = multiprocessing.Pool(processes=jobs)
  start = time.time()
  try:
    g = ((t, executable, root_dir, cwd_dir, product_dir, leak) for t in tests)
    it = pool.imap_unordered(task, g)
    while True:
      try:
        result = it.next(timeout=timeout)
      except StopIteration:
        break
      case = result.pop('case')
      index += 1
      line = '%d of %d (%.1f%%), %.1fs: %s' % (
            index,
            len(tests),
            index * 100. / len(tests),
            time.time() - start,
            case)
      sys.stdout.write(
          '\r%s%s' % (line, ' ' * max(0, len(last_line) - len(line))))
      sys.stdout.flush()
      last_line = line
      # TODO(maruel): Retry failed tests.
      out[case] = result
    return 0
  except multiprocessing.TimeoutError, e:
    print 'Got a timeout while processing a task item %s' % e
    # Be sure to stop the pool on exception.
    pool.terminate()
    return 1
  except Exception, e:
    # Be sure to stop the pool on exception.
    pool.terminate()
    raise
  finally:
    with open('%s.test_cases' % executable, 'w') as f:
      json.dump(out, f, indent=2, sort_keys=True)
    pool.close()
    pool.join()


def main():
  """CLI frontend to validate arguments."""
  parser = optparse.OptionParser(
      usage='%prog <options> [gtest]')
  parser.allow_interspersed_args = False
  parser.add_option(
      '-c', '--cwd',
      default='chrome',
      help='Signal to start the process from this relative directory. When '
           'specified, outputs the inputs files in a way compatible for '
           'gyp processing. Should be set to the relative path containing the '
           'gyp file, e.g. \'chrome\' or \'net\'')
  parser.add_option(
      '-p', '--product-dir',
      default='out/Release',
      help='Directory for PRODUCT_DIR. Default: %default')
  parser.add_option(
      '--root-dir',
      default=ROOT_DIR,
      help='Root directory to base everything off. Default: %default')
  parser.add_option(
      '-l', '--leak',
      action='store_true',
      help='Leak trace files')
  parser.add_option(
      '-s', '--skip',
      default=[],
      action='append',
      help='filter to apply to test cases to skip, wildcard-style')
  parser.add_option(
      '-j', '--jobs',
      type='int',
      help='number of parallel jobs')
  parser.add_option(
      '-t', '--timeout',
      default=120,
      type='int',
      help='number of parallel jobs')
  options, args = parser.parse_args()

  if len(args) != 1:
    parser.error(
        'Please provide the executable line to run, if you need fancy things '
        'like xvfb, start this script from inside xvfb, it\'ll be faster.')
  executable = os.path.join(options.root_dir, options.product_dir, args[0])
  return trace_test_cases(
      executable,
      options.root_dir,
      options.cwd,
      options.product_dir,
      options.leak,
      options.skip,
      options.jobs,
      options.timeout)


if __name__ == '__main__':
  sys.exit(main())
