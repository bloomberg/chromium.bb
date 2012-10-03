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
import sys
import time

import run_test_cases
import trace_inputs

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(os.path.dirname(BASE_DIR))


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
    tracename = test_case.replace('/', '-')

    out = []
    for retry in range(5):
      start = time.time()
      returncode, output = self.tracer.trace(
          cmd, self.cwd_dir, tracename, True)
      duration = time.time() - start
      # TODO(maruel): Define a way to detect if an strace log is valid.
      valid = True
      out.append(
          {
            'test_case': test_case,
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
  """Traces test cases one by one."""
  assert os.path.isabs(cwd_dir) and os.path.isdir(cwd_dir)

  if not test_cases:
    return 0

  # Resolve any symlink.
  cwd_dir = os.path.realpath(cwd_dir)
  assert os.path.isdir(cwd_dir)

  progress = run_test_cases.Progress(len(test_cases))
  with run_test_cases.ThreadPool(jobs or multiprocessing.cpu_count()) as pool:
    api = trace_inputs.get_api()
    api.clean_trace(logname)
    with api.get_tracer(logname) as tracer:
      function = Tracer(tracer, cmd, cwd_dir, progress).map
      for test_case in test_cases:
        pool.add_task(function, test_case)

      pool.join(progress, 0.1)
  print('')
  return 0


def main():
  """CLI frontend to validate arguments."""
  parser = run_test_cases.OptionParserTestCases(
      usage='%prog <options> [gtest]',
      description=sys.modules['__main__'].__doc__)
  parser.format_description = lambda *_: parser.description
  parser.add_option(
      '-o', '--out',
      help='output file, defaults to <executable>.test_cases')
  options, args = parser.parse_args()

  if not args:
    parser.error(
        'Please provide the executable line to run, if you need fancy things '
        'like xvfb, start this script from *inside* xvfb, it\'ll be much faster'
        '.')

  cmd = run_test_cases.fix_python_path(args)

  if not options.out:
    options.out = '%s.test_cases' % cmd[-1]

  test_cases = parser.process_gtest_options(cmd, options)

  # Then run them.
  return trace_test_cases(
      cmd,
      os.getcwd(),
      test_cases,
      options.jobs,
      # TODO(maruel): options.timeout,
      options.out)


if __name__ == '__main__':
  sys.exit(main())
