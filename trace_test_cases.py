#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Traces each test cases of a google-test executable individually.

Gives detailed information about each test case. The logs can be read afterward
with ./trace_inputs.py read -l /path/to/executable.logs
"""

import logging
import multiprocessing
import os
import re
import sys
import time

import run_test_cases
import trace_inputs

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(os.path.dirname(BASE_DIR))


def sanitize_test_case_name(test_case):
  """Removes characters that are valid as test case names but invalid as file
  names.
  """
  return test_case.replace('/', '-')


class Tracer(object):
  def __init__(self, tracer, cmd, cwd_dir, progress):
    # Constants
    self.tracer = tracer
    self.cmd = cmd[:]
    self.cwd_dir = cwd_dir
    self.progress = progress

  def map(self, test_case):
    """Traces a single test case and returns its output."""
    cmd = self.cmd[:]
    cmd.append('--gtest_filter=%s' % test_case)
    tracename = sanitize_test_case_name(test_case)

    out = []
    for retry in range(5):
      start = time.time()
      returncode, output = self.tracer.trace(cmd, self.cwd_dir, tracename, True)
      duration = time.time() - start
      # TODO(maruel): Define a way to detect if a strace log is valid.
      valid = True
      out.append(
          {
            'test_case': test_case,
            'tracename': tracename,
            'returncode': returncode,
            'duration': duration,
            'valid': valid,
            'output': output,
          })
      logging.debug(
          'Tracing %s done: %d, %.1fs' % (test_case, returncode,  duration))
      if retry:
        self.progress.update_item(
            '%s - %d' % (test_case, retry), True, not valid)
      else:
        self.progress.update_item(test_case, True, not valid)
      if valid:
        break
    return out


def trace_test_cases(cmd, cwd_dir, test_cases, jobs, logname):
  """Traces each test cases individually but all in parallel."""
  assert os.path.isabs(cwd_dir) and os.path.isdir(cwd_dir), cwd_dir

  if not test_cases:
    return []

  # Resolve any symlink.
  cwd_dir = os.path.realpath(cwd_dir)
  assert os.path.isdir(cwd_dir)

  api = trace_inputs.get_api()
  api.clean_trace(logname)

  jobs = jobs or multiprocessing.cpu_count()
  # Try to do black magic here by guessing a few of the run_test_cases.py
  # flags. It's cheezy but it works.
  for i, v in enumerate(cmd):
    if v.endswith('run_test_cases.py'):
      # Found it. Process the arguments here.
      _, options, _ = run_test_cases.process_args(cmd[i:])
      # Always override with the lowest value.
      jobs = min(options.jobs, jobs)
      break

  progress = run_test_cases.Progress(len(test_cases))
  with run_test_cases.ThreadPool(progress, jobs, jobs,
                                 len(test_cases)) as pool:
    with api.get_tracer(logname) as tracer:
      function = Tracer(tracer, cmd, cwd_dir, progress).map
      for test_case in test_cases:
        pool.add_task(0, function, test_case)

      results = pool.join()
  print('')
  return results


def write_details(logname, outfile, root_dir, blacklist, results):
  """Writes an .test_cases file with all the information about each test
  case.
  """
  api = trace_inputs.get_api()
  logs = dict(
      (i.pop('trace'), i) for i in api.parse_log(logname, blacklist, None))
  results_processed = {}
  exception = None
  for items in results:
    item = items[-1]
    assert item['valid']
    # Load the results;
    log_dict = logs[item['tracename']]
    if log_dict.get('exception'):
      exception = exception or log_dict['exception']
      continue
    trace_result = log_dict['results']
    if root_dir:
      trace_result = trace_result.strip_root(root_dir)
    results_processed[item['test_case']] = {
      'trace': trace_result.flatten(),
      'duration': item['duration'],
      'output': item['output'],
      'returncode': item['returncode'],
    }

  # Make it dense if there is more than 20 results.
  trace_inputs.write_json(
      outfile,
      results_processed,
      len(results_processed) > 20)
  if exception:
    raise exception[0], exception[1], exception[2]


def main():
  """CLI frontend to validate arguments."""
  run_test_cases.run_isolated.disable_buffering()
  parser = run_test_cases.OptionParserTestCases(
      usage='%prog <options> [gtest]')
  parser.format_description = lambda *_: parser.description
  parser.add_option(
      '-o', '--out',
      help='output file, defaults to <executable>.test_cases')
  parser.add_option(
      '-r', '--root-dir',
      help='Root directory under which file access should be noted')
  # TODO(maruel): Add support for options.timeout.
  parser.remove_option('--timeout')
  options, args = parser.parse_args()

  if not args:
    parser.error(
        'Please provide the executable line to run, if you need fancy things '
        'like xvfb, start this script from *inside* xvfb, it\'ll be much faster'
        '.')

  cmd = run_test_cases.fix_python_path(args)
  cmd[0] = os.path.abspath(cmd[0])
  if not os.path.isfile(cmd[0]):
    parser.error('Tracing failed for: %s\nIt doesn\'t exit' % ' '.join(cmd))

  if not options.out:
    options.out = '%s.test_cases' % cmd[-1]
  options.out = os.path.abspath(options.out)
  if options.root_dir:
    options.root_dir = os.path.abspath(options.root_dir)
  logname = options.out + '.log'

  def blacklist(f):
    return any(re.match(b, f) for b in options.blacklist)

  test_cases = parser.process_gtest_options(cmd, os.getcwd(), options)

  # Then run them.
  print('Tracing...')
  results = trace_test_cases(
      cmd,
      os.getcwd(),
      test_cases,
      options.jobs,
      logname)
  print('Reading trace logs...')
  write_details(logname, options.out, options.root_dir, blacklist, results)
  return 0


if __name__ == '__main__':
  sys.exit(main())
