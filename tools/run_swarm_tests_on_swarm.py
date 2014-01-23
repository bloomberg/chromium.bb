#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Runs the whole set unit tests on swarm.

This is done in a few steps:
  - Archive the whole directory as a single .isolated file.
  - Create one test-specific .isolated for each test to run. The file is created
    directly and archived manually with isolateserver.py.
  - Trigger each of these test-specific .isolated file per OS.
  - Get all results out of order.
"""

import datetime
import glob
import getpass
import logging
import optparse
import os
import shutil
import subprocess
import sys
import tempfile
import time

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(BASE_DIR)

sys.path.insert(0, ROOT_DIR)

from utils import threading_utils
from utils import tools


def check_output(cmd, cwd):
  return subprocess.check_output([sys.executable] + cmd, cwd=cwd)


def capture(cmd, cwd):
  start = time.time()
  p = subprocess.Popen([sys.executable] + cmd, cwd=cwd, stdout=subprocess.PIPE)
  out = p.communicate()[0]
  return p.returncode, out, time.time() - start


def archive_tree(root, isolate_server):
  """Archives a whole tree and return the sha1 of the .isolated file.

  Manually creates a temporary isolated file and archives it.
  """
  logging.info('archive_tree(%s)', root)
  cmd = [
      'isolateserver.py', 'archive', '--isolate-server', isolate_server, root,
  ]
  if logging.getLogger().isEnabledFor(logging.INFO):
    cmd.append('--verbose')
  out = check_output(cmd, root)
  return out.split()[0]


def archive_isolated_triggers(cwd, isolate_server, tree_isolated, tests):
  """Creates and archives all the .isolated files for the tests at once.

  Archiving them in one batch is faster than archiving each file individually.
  Also the .isolated files can be reused across OSes, reducing the amount of
  I/O.

  Returns:
    list of (test, sha1) tuples.
  """
  logging.info(
      'archive_isolated_triggers(%s, %s, %s)', cwd, tree_isolated, tests)
  tempdir = tempfile.mkdtemp(prefix='run_swarm_tests_on_swarm_')
  try:
    isolateds = []
    for test in tests:
      test_name = os.path.basename(test)
      # Creates a manual .isolated file. See
      # https://code.google.com/p/swarming/wiki/IsolatedDesign for more details.
      isolated = {
        'algo': 'sha-1',
        'command': ['python', test],
        'includes': [tree_isolated],
        'version': '1.0',
      }
      v = os.path.join(tempdir, test_name + '.isolated')
      tools.write_json(v, isolated, True)
      isolateds.append(v)
    cmd = [
        'isolateserver.py', 'archive', '--isolate-server', isolate_server,
    ] + isolateds
    if logging.getLogger().isEnabledFor(logging.INFO):
      cmd.append('--verbose')
    items = [i.split() for i in check_output(cmd, cwd).splitlines()]
    assert len(items) == len(tests)
    assert all(
        items[i][1].endswith(os.path.basename(tests[i]) + '.isolated')
        for i in xrange(len(tests)))
    return zip(tests, [i[0] for i in items])
  finally:
    shutil.rmtree(tempdir)


def trigger(
    cwd, swarm_server, isolate_server, task_name, platform, isolated_hash):
  """Triggers a specified .isolated file."""
  cmd = [
      'swarming.py',
      'trigger',
      '--swarming', swarm_server,
      '--isolate-server', isolate_server,
      '--dimension', 'os', platform,
      '--task-name', task_name,
      isolated_hash,
  ]
  return capture(cmd, cwd)


def collect(cwd, swarm_server, task_name):
  cmd = [
      'swarming.py',
      'collect',
      '--swarming', swarm_server,
      task_name,
  ]
  return capture(cmd, cwd)


class Runner(object):
  def __init__(self, isolate_server, swarm_server, add_task, progress):
    self.isolate_server = isolate_server
    self.swarm_server = swarm_server
    self.add_task = add_task
    self.progress = progress
    self.prefix = (
        getpass.getuser() + '-' + datetime.datetime.now().isoformat() + '-')

  def trigger(self, task_name, platform, isolated_hash):
    returncode, stdout, duration = trigger(
        ROOT_DIR,
        self.swarm_server,
        self.isolate_server,
        task_name,
        platform,
        isolated_hash)
    step_name = '%s (%3.2fs)' % (task_name, duration)
    if returncode:
      line = 'Failed to trigger %s\n%s' % (step_name, stdout)
      self.progress.update_item(line, index=1)
      return
    self.progress.update_item('Triggered %s' % step_name, index=1)
    self.add_task(0, self.collect, task_name, platform)

  def collect(self, task_name, platform):
    returncode, stdout, duration = collect(
        ROOT_DIR, self.swarm_server, task_name)
    step_name = '%s (%3.2fs)' % (task_name, duration)
    if returncode:
      # Only print the output for failures, successes are unexciting.
      self.progress.update_item(
          'Failed %s:\n%s' % (step_name, stdout), index=1)
      return (task_name, platform, stdout)
    self.progress.update_item('Passed %s' % step_name, index=1)


def run_swarm_tests_on_swarm(swarm_server, isolate_server, oses, tests, logs):
  """Archives, triggers swarming jobs and gets results."""
  start = time.time()
  # First, archive the whole tree.
  tree_isolated = archive_tree(ROOT_DIR, isolate_server)

  # Create and archive all the .isolated files.
  isolateds = archive_isolated_triggers(
      ROOT_DIR, isolate_server, tree_isolated, tests)
  logging.debug('%s', isolateds)
  print('Archival took %3.2fs' % (time.time() - start))

  # Trigger all the jobs and get results. This is parallelized in worker
  # threads.
  runs = len(isolateds) * len(oses)
  # triger + collect
  total = 2 * runs
  columns = [('index', 0), ('size', total)]
  progress = threading_utils.Progress(columns)
  progress.use_cr_only = False
  failed_tests = []
  with threading_utils.ThreadPoolWithProgress(
      progress, runs, runs, total) as pool:
    start = time.time()
    runner = Runner(isolate_server, swarm_server, pool.add_task, progress)
    for test_path, isolated in isolateds:
      test_name = os.path.basename(test_path).split('.')[0]
      for platform in oses:
        task_name = '%s/%s/%s' % (test_name, platform, isolated)
        pool.add_task(0, runner.trigger, task_name, platform, isolated)

    for failed_test in pool.iter_results():
      # collect() only return test case failures.
      test_name, platform, stdout = failed_test
      failed_tests.append(test_name)
      if logs:
        # Write the logs are they are retrieved.
        if not os.path.isdir(logs):
          os.makedirs(logs)
        name = '%s_%s.log' % (platform, test_name.split('/', 1)[0])
        with open(os.path.join(logs, name), 'wb') as f:
          f.write(stdout)
  duration = time.time() - start
  print('\nCompleted in %3.2fs' % duration)
  if failed_tests:
    print('Detected the following failures:')
    for test in sorted(failed_tests):
      print('  %s' % test)
  return bool(failed_tests)


def main():
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-I', '--isolate-server',
      metavar='URL', default='',
      help='Isolate server to use')
  parser.add_option(
      '-S', '--swarming',
      metavar='URL', default='',
      help='Swarming server to use')
  parser.add_option(
      '-l', '--logs',
      help='Destination where to store the failure logs (recommended)')
  parser.add_option('-o', '--os', help='Run tests only on this OS')
  parser.add_option(
      '-t', '--test', action='append',
      help='Run only these test, can be specified multiple times')
  parser.add_option('-v', '--verbose', action='store_true')
  options, args = parser.parse_args()
  if args:
    parser.error('Unsupported argument %s' % args)
  if options.verbose:
    os.environ['ISOLATE_DEBUG'] = '1'

  if not options.isolate_server:
    parser.error('--isolate-server is required.')
  if not options.swarming:
    parser.error('--swarming is required.')

  logging.basicConfig(level=logging.DEBUG if options.verbose else logging.ERROR)

  oses = ['Linux', 'Mac', 'Windows']
  tests = [
      os.path.relpath(i, ROOT_DIR)
      for i in (
      glob.glob(os.path.join(ROOT_DIR, 'tests', '*_test.py')) +
      glob.glob(os.path.join(ROOT_DIR, 'googletest', 'tests', '*_test.py')))
  ]
  valid_tests = sorted(map(os.path.basename, tests))
  assert len(valid_tests) == len(set(valid_tests)), (
      'Can\'t have 2 tests with the same base name')

  if options.test:
    for t in options.test:
      if not t in valid_tests:
        parser.error(
            '--test %s is unknown. Valid values are:\n%s' % (
              t, '\n'.join('  ' + i for i in valid_tests)))
    filters = tuple(os.path.sep + t for t in options.test)
    tests = [t for t in tests if t.endswith(filters)]

  if options.os:
    if options.os not in oses:
      parser.error(
          '--os %s is unknown. Valid values are %s' % (
            options.os, ', '.join(sorted(oses))))
    oses = [options.os]

  if sys.platform in ('win32', 'cygwin'):
    # If we are on Windows, don't generate the tests for Linux and Mac since
    # they use symlinks and we can't create symlinks on windows.
    oses = ['Windows']
    if options.os != 'win32':
      print('Linux and Mac tests skipped since running on Windows.')

  return run_swarm_tests_on_swarm(
      options.swarming,
      options.isolate_server,
      oses,
      tests,
      options.logs)


if __name__ == '__main__':
  sys.exit(main())
