#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the whole set unit tests on swarm."""

import datetime
import glob
import getpass
import hashlib
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


# Mapping of the sys.platform value into Swarm OS value.
OSES = {'win32': 'win', 'linux2': 'linux', 'darwin': 'mac'}


class Runner(object):
  def __init__(self, isolate_server, swarm_server, add_task, progress, tempdir):
    self.isolate_server = isolate_server
    self.swarm_server = swarm_server
    self.add_task = add_task
    self.progress = progress
    self.tempdir = tempdir
    self.prefix = (
        getpass.getuser() + '-' + datetime.datetime.now().isoformat() + '-')

  @staticmethod
  def _call(args):
    start = time.time()
    proc = subprocess.Popen(
        [sys.executable] + args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=ROOT_DIR)
    stdout = proc.communicate()[0]
    return proc.returncode, stdout, time.time() - start

  def archive(self, test, platform):
    # Put the .isolated files in a temporary directory. This is simply done so
    # the current directory doesn't have the following files created:
    # - swarm_client_tests.isolated
    # - swarm_client_tests.isolated.state
    test_name = os.path.basename(test)
    handle, isolated = tempfile.mkstemp(
        dir=self.tempdir, prefix='run_swarm_tests_on_swarm_',
        suffix='.isolated')
    os.close(handle)
    try:
      returncode, stdout, duration = self._call(
          [
            'isolate.py',
            'archive',
            '--isolate', os.path.join(BASE_DIR, 'run_a_test.isolate'),
            '--isolated', isolated,
            '--outdir', self.isolate_server,
            '--variable', 'TEST_EXECUTABLE', test,
            '--variable', 'OS', OSES[platform],
          ])
      step_name = '%s/%s (%3.2fs)' % (platform, test_name, duration)
      if returncode:
        self.progress.update_item(
            'Failed to archive %s\n%s' % (step_name, stdout), index=1)
      else:
        hash_value = hashlib.sha1(open(isolated, 'rb').read()).hexdigest()
        logging.info('%s: %s', step_name, hash_value)
        self.progress.update_item('Archived %s' % step_name, index=1)
        self.add_task(0, self.trigger, test, platform, hash_value)
    finally:
      try:
        os.remove(isolated)
      except OSError:
        logging.debug('%s was already deleted', isolated)
    return None

  def trigger(self, test, platform, hash_value):
    test_name = os.path.basename(test)
    returncode, stdout, duration = self._call(
        [
          'swarming.py',
          'trigger',
          '--os', platform,
          '--swarming', self.swarm_server,
          '--task-prefix', self.prefix,
          '--isolate-server', self.isolate_server,
          '--task',
          # Isolated hash.
          hash_value,
          # Test name.
          'swarm_client_tests_%s_%s' % (platform, test_name),
          # Number of shards.
          '1',
          # test filter.
          '*',
        ])
    step_name = '%s/%s (%3.2fs)' % (platform, test_name, duration)
    if returncode:
      self.progress.update_item(
          'Failed to trigger %s\n%s' % (step_name, stdout), index=1)
    else:
      self.progress.update_item('Triggered %s' % step_name, index=1)
      self.add_task(0, self.get_result, test, platform)
    return None

  def get_result(self, test, platform):
    test_name = os.path.basename(test)
    name = '%s_%s' % (platform, test_name)
    returncode, stdout, duration = self._call(
        [
          'swarming.py',
          'collect',
          '--swarming', self.swarm_server,
          self.prefix + 'swarm_client_tests_' + name,
        ])
    step_name = '%s/%s (%3.2fs)' % (platform, test_name, duration)
    # Only print the output for failures, successes are unexciting.
    if returncode:
      self.progress.update_item(
          'Failed %s:\n%s' % (step_name, stdout), index=1)
      return (test_name, platform, stdout)
    self.progress.update_item('Passed %s' % step_name, index=1)
    return None


def run_swarm_tests_on_swarm(oses, tests, logs, isolate_server, swarm_server):
  runs = len(tests) * len(oses)
  total = 3 * runs
  columns = [('index', 0), ('size', total)]
  progress = threading_utils.Progress(columns)
  progress.use_cr_only = False
  tempdir = tempfile.mkdtemp(prefix='swarm_client_tests')
  try:
    with threading_utils.ThreadPoolWithProgress(
        progress, runs, runs, total) as pool:
      start = time.time()
      runner = Runner(
          isolate_server, swarm_server, pool.add_task, progress, tempdir)
      for test in tests:
        for platform in oses:
          pool.add_task(0, runner.archive, test, platform)

      failed_tests = pool.join()
      duration = time.time() - start
      print('')
  finally:
    shutil.rmtree(tempdir)

  if logs:
    os.makedirs(logs)
    for test, platform, stdout in failed_tests:
      name = '%s_%s' % (platform, os.path.basename(test))
      with open(os.path.join(logs, name + '.log'), 'wb') as f:
        f.write(stdout)

  print('Completed in %3.2fs' % duration)
  if failed_tests:
    failed_tests_per_os = {}
    for test, platform, _ in failed_tests:
      failed_tests_per_os.setdefault(test, []).append(platform)
    print('Detected the following failures:')
    for test, platforms in failed_tests_per_os.iteritems():
      print('  %s on %s' % (test, ', '.join(sorted(platforms))))
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
  parser.add_option('-t', '--test', help='Run only this test')
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

  # Note that the swarm and the isolate code use different strings for the
  # different oses.
  oses = OSES.keys()
  tests = [
    os.path.relpath(i, BASE_DIR)
    for i in glob.iglob(os.path.join(ROOT_DIR, 'tests', '*_test.py'))
  ]

  if options.test:
    valid_tests = sorted(map(os.path.basename, tests))
    if not options.test in valid_tests:
      parser.error(
          '--test %s is unknown. Valid values are:\n%s' % (
            options.test, '\n'.join('  ' + i for i in valid_tests)))
    tests = [t for t in tests if t.endswith(os.path.sep + options.test)]

  if options.os:
    if options.os not in oses:
      parser.error(
          '--os %s is unknown. Valid values are %s' % (
            options.os, ', '.join(sorted(oses))))
    oses = [options.os]

  if sys.platform in ('win32', 'cygwin'):
    # If we are on Windows, don't generate the tests for Linux and Mac since
    # they use symlinks and we can't create symlinks on windows.
    oses = ['win32']
    if options.os != 'win32':
      print('Linux and Mac tests skipped since running on Windows.')

  return run_swarm_tests_on_swarm(
      oses,
      tests,
      options.logs,
      options.isolate_server,
      options.swarming)


if __name__ == '__main__':
  sys.exit(main())
