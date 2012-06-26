#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Traces each test cases of a google-test executable individually.

Gives detailed information about each test case. The logs can be read afterward
with ./trace_inputs.py read -l /path/to/executable.logs
"""

import fnmatch
import logging
import multiprocessing
import optparse
import os
import sys
import time

import isolate_common
import list_test_cases
import trace_inputs
import worker_pool

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
    cmd = list_test_cases.fix_python_path(cmd)
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
      if not valid:
        self.progress.increase_count()
      if retry:
        self.progress.update_item('%s - %d' % (test_case, retry))
      else:
        self.progress.update_item(test_case)
      if valid:
        break
    return out


def get_test_cases(executable, whitelist, blacklist, index, shards):
  """Returns the filtered list of test cases.

  This is done synchronously.
  """
  try:
    tests = list_test_cases.list_test_cases(
        executable, index, shards, False, False, False)
  except list_test_cases.Failure, e:
    print e.args[0]
    return None

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


def trace_test_cases(
    executable, root_dir, cwd_dir, variables, test_cases, jobs, output_file):
  """Traces test cases one by one."""
  if not test_cases:
    return 0

  # Resolve any symlink.
  root_dir = os.path.realpath(root_dir)
  full_cwd_dir = os.path.join(root_dir, cwd_dir)
  logname = output_file + '.logs'

  progress = worker_pool.Progress(len(test_cases))
  with worker_pool.ThreadPool(jobs or multiprocessing.cpu_count()) as pool:
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
  results = api.parse_log(logname, isolate_common.default_blacklist)
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
        flattened[test_case].update({
            'processes': len(list(item_results.process.all)),
            'results': item_results.flatten(),
            'duration': item['duration'],
            'returncode': item['returncode'],
            'valid': item['valid'],
            'variables':
              isolate_common.generate_simplified(
                  item_results.existent,
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

  value = isolate_common.generate_isolate(
      files,
      root_dir,
      variables,
      cwd_dir)
  with open('%s.isolate' % output_file, 'wb') as f:
    isolate_common.pretty_print(value, f)
  return 0


def main():
  """CLI frontend to validate arguments."""
  default_variables = [('OS', isolate_common.get_flavor())]
  if sys.platform in ('win32', 'cygwin'):
    default_variables.append(('EXECUTABLE_SUFFIX', '.exe'))
  else:
    default_variables.append(('EXECUTABLE_SUFFIX', ''))
  parser = optparse.OptionParser(
      usage='%prog <options> [gtest]',
      description=sys.modules['__main__'].__doc__)
  parser.format_description = lambda *_: parser.description
  parser.add_option(
      '-c', '--cwd',
      default='chrome',
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
  group = optparse.OptionGroup(parser, 'Which test cases to run')
  group.add_option(
      '-w', '--whitelist',
      default=[],
      action='append',
      help='filter to apply to test cases to run, wildcard-style, defaults to '
           'all test')
  group.add_option(
      '-b', '--blacklist',
      default=[],
      action='append',
      help='filter to apply to test cases to skip, wildcard-style, defaults to '
           'no test')
  group.add_option(
      '-i', '--index',
      type='int',
      help='Shard index to run')
  group.add_option(
      '-s', '--shards',
      type='int',
      help='Total number of shards to calculate from the --index to run')
  group.add_option(
      '-T', '--test-case-file',
      help='File containing the exact list of test cases to run')
  parser.add_option_group(group)
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

  if bool(options.shards) != bool(options.index is not None):
    parser.error('Use both --index X --shards Y or none of them')

  executable = args[0]
  if not os.path.isabs(executable):
    executable = os.path.abspath(os.path.join(options.root_dir, executable))
  if not options.out:
    options.out = '%s.test_cases' % executable

  # First, grab the test cases.
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
