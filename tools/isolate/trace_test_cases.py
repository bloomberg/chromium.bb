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

import isolate  # TODO(maruel): Remove references to isolate.
import run_test_cases
import trace_inputs

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(os.path.dirname(BASE_DIR))


class Tracer(object):
  def __init__(self, tracer, executable, cwd_dir, progress):
    # Constants
    self.tracer = tracer
    self.executable = executable
    self.cwd_dir = cwd_dir
    self.progress = progress

  def map(self, test_case):
    """Traces a single test case and returns its output."""
    cmd = [self.executable, '--gtest_filter=%s' % test_case]
    cmd = run_test_cases.fix_python_path(cmd)
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


def trace_test_cases(
    executable, root_dir, cwd_dir, variables, test_cases, jobs, output_file):
  """Traces test cases one by one."""
  assert not os.path.isabs(cwd_dir)
  assert os.path.isabs(root_dir) and os.path.isdir(root_dir)
  assert os.path.isfile(executable) and os.path.isabs(executable)

  if not test_cases:
    return 0

  # Resolve any symlink.
  root_dir = os.path.realpath(root_dir)
  full_cwd_dir = os.path.normpath(os.path.join(root_dir, cwd_dir))
  assert os.path.isdir(full_cwd_dir)
  logname = output_file + '.logs'

  progress = run_test_cases.Progress(len(test_cases))
  with run_test_cases.ThreadPool(jobs or multiprocessing.cpu_count()) as pool:
    api = trace_inputs.get_api()
    api.clean_trace(logname)
    with api.get_tracer(logname) as tracer:
      function = Tracer(tracer, executable, full_cwd_dir, progress).map
      for test_case in test_cases:
        pool.add_task(function, test_case)

      values = pool.join(progress, 0.1)

  print ''
  print '%.1fs Done post-processing logs. Parsing logs.' % (
      time.time() - progress.start)
  results = api.parse_log(logname, isolate.default_blacklist)
  print '%.1fs Done parsing logs.' % (
      time.time() - progress.start)

  # Strips to root_dir.
  results_processed = {}
  for item in results:
    if 'results' in item:
      item = item.copy()
      item['results'] = item['results'].strip_root(root_dir)
      results_processed[item['trace']] = item
    else:
      print >> sys.stderr, 'Got exception while tracing %s: %s' % (
          item['trace'], item['exception'])
  print '%.1fs Done stripping root.' % (
      time.time() - progress.start)

  # Flatten.
  flattened = {}
  for item_list in values:
    for item in item_list:
      if item['valid']:
        test_case = item['test_case']
        tracename = test_case.replace('/', '-')
        flattened[test_case] = results_processed[tracename].copy()
        item_results = flattened[test_case]['results']
        tracked, touched = isolate.split_touched(item_results.existent)
        flattened[test_case].update({
            'processes': len(list(item_results.process.all)),
            'results': item_results.flatten(),
            'duration': item['duration'],
            'returncode': item['returncode'],
            'valid': item['valid'],
            'variables':
              isolate.generate_simplified(
                  tracked,
                  [],
                  touched,
                  root_dir,
                  variables,
                  cwd_dir),
          })
        del flattened[test_case]['trace']
  print '%.1fs Done flattening.' % (
      time.time() - progress.start)

  # Make it dense if there is more than 20 results.
  trace_inputs.write_json(
      output_file,
      flattened,
      False)

  # Also write the .isolate file.
  # First, get all the files from all results. Use a map to remove dupes.
  files = {}
  for item in results_processed.itervalues():
    files.update((f.full_path, f) for f in item['results'].existent)
  # Convert back to a list, discard the keys.
  files = files.values()
  tracked, touched = isolate.split_touched(files)
  value = isolate.generate_isolate(
      tracked,
      [],
      touched,
      root_dir,
      variables,
      cwd_dir)
  with open('%s.isolate' % output_file, 'wb') as f:
    isolate.pretty_print(value, f)
  return 0


def main():
  """CLI frontend to validate arguments."""
  default_variables = [('OS', isolate.get_flavor())]
  if sys.platform in ('win32', 'cygwin'):
    default_variables.append(('EXECUTABLE_SUFFIX', '.exe'))
  else:
    default_variables.append(('EXECUTABLE_SUFFIX', ''))
  parser = run_test_cases.OptionParserWithTestShardingAndFiltering(
      usage='%prog <options> [gtest]',
      description=sys.modules['__main__'].__doc__)
  parser.format_description = lambda *_: parser.description
  parser.add_option(
      '-c', '--cwd',
      default='',
      help='Signal to start the process from this relative directory. When '
           'specified, outputs the inputs files in a way compatible for '
           'gyp processing. Should be set to the relative path containing the '
           'gyp file, e.g. \'chrome\' or \'net\'')
  parser.add_option(
      '-V', '--variable',
      nargs=2,
      action='append',
      default=default_variables,
      dest='variables',
      metavar='FOO BAR',
      help='Variables to process in the .isolate file, default: %default')
  parser.add_option(
      '--root-dir',
      default=ROOT_DIR,
      help='Root directory to base everything off. Default: %default')
  parser.add_option(
      '-o', '--out',
      help='output file, defaults to <executable>.test_cases')
  parser.add_option(
      '-j', '--jobs',
      type='int',
      help='number of parallel jobs')
  parser.add_option(
      '-t', '--timeout',
      default=120,
      type='int',
      help='number of parallel jobs')
  parser.add_option(
      '-v', '--verbose',
      action='count',
      default=0,
      help='Use multiple times to increase verbosity')
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

  options.root_dir = os.path.abspath(options.root_dir)
  if not os.path.isdir(options.root_dir):
    parser.error('--root-dir "%s" must exist' % options.root_dir)
  if not os.path.isdir(os.path.join(options.root_dir, options.cwd)):
    parser.error(
        '--cwd "%s" must be an existing directory relative to %s' %
          (options.cwd, options.root_dir))

  executable = args[0]
  if not os.path.isabs(executable):
    executable = os.path.abspath(os.path.join(options.root_dir, executable))
  if not os.path.isfile(executable):
    parser.error('"%s" doesn\'t exist.' % executable)

  if not options.out:
    options.out = '%s.test_cases' % executable

  # First, grab the test cases.
  if options.test_case_file:
    with open(options.test_case_file, 'r') as f:
      test_cases = filter(None, f.read().splitlines())
  else:
    test_cases = run_test_cases.get_test_cases(
        executable,
        options.whitelist,
        options.blacklist,
        options.index,
        options.shards)

  # Then run them.
  return trace_test_cases(
      executable,
      options.root_dir,
      options.cwd,
      dict(options.variables),
      test_cases,
      options.jobs,
      # TODO(maruel): options.timeout,
      options.out)


if __name__ == '__main__':
  sys.exit(main())
