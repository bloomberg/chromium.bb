#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the whole set unit tests on swarm."""

import datetime
import glob
import getpass
import hashlib
import optparse
import os
import shutil
import subprocess
import sys
import tempfile

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


def main():
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-i', '--isolate-server',
      default='https://isolateserver-dev.appspot.com/',
      help='Isolate server to use default:%default')
  parser.add_option(
      '-s', '--swarm-server',
      default='https://chromium-swarm-dev.appspot.com/',
      help='Isolate server to use default:%default')
  parser.add_option('-v', '--verbose', action='store_true')
  options, args = parser.parse_args()
  if args:
    parser.error('Unsupported argument %s' % args)
  if options.verbose:
    os.environ['ISOLATE_DEBUG'] = '1'

  prefix = getpass.getuser() + '-' + datetime.datetime.now().isoformat() + '-'

  # Note that the swarm and the isolate code use different strings for the
  # different oses.
  oses = ('win32', 'linux2', 'darwin')
  isolate_oses = ('win', 'linux', 'mac')
  tests = [
    os.path.relpath(i, ROOT_DIR)
    for i in glob.iglob(os.path.join(ROOT_DIR, '..', 'tests', '*_test.py'))
  ]

  on_windows = sys.platform in ('win32', 'cygwin')
  if on_windows:
    # If we are on Windows, don't generate the tests for Linux and Mac since
    # they use symlinks and we can't create symlinks on windows.
    oses = ('win32')
    isolate_oses = ('win')

  result = 0
  tempdir = tempfile.mkdtemp(prefix='swarm_client_tests')
  try:
    # Put the .isolated files in a temporary directory. This is simply done so
    # the current directory doesn't have the following files created:
    # - swarm_client_tests.isolate
    # - swarm_client_tests.isolated
    # - swarm_client_tests.isolated.state
    isolated = os.path.join(tempdir, 'swarm_client_tests.isolated')

    print('Archiving')
    hashvals = []
    for i, test in enumerate(tests):
      hashvals.append([])
      for platform in isolate_oses:
        subprocess.check_call(
            [
                sys.executable,
                'isolate.py',
                'hashtable',
                '--isolate', os.path.join(ROOT_DIR, 'run_a_test.isolate'),
                '--isolated', isolated,
                '--outdir', options.isolate_server,
                '--variable', 'TEST_EXECUTABLE', test,
                '--variable', 'OS', platform,
          ],
          cwd=os.path.dirname(ROOT_DIR))
        hashvals[i].append(
            hashlib.sha1(open(isolated, 'rb').read()).hexdigest())

    print('\nTriggering')
    for i, test in enumerate(tests):
      sys.stdout.write('  %s: ' % os.path.basename(test))
      for j, platform in enumerate(oses):
        sys.stdout.write(platform)
        if platform != oses[-1]:
          sys.stdout.write(', ')
        subprocess.check_call(
            [
              sys.executable,
              'swarm_trigger_step.py',
              '--os_image', platform,
              '--swarm-url', options.swarm_server,
              '--test-name-prefix', prefix,
              '--data-server', options.isolate_server,
              '--run_from_hash', hashvals[i][j],
              'swarm_client_tests_' + platform + os.path.basename(test),
              # Number of shards.
              '1',
              '',
            ],
            cwd=os.path.dirname(ROOT_DIR))
      sys.stdout.write('\n')

    print('\nGetting results')
    failed_tests = {}
    for i, test in enumerate(tests):
      print('  %s' % os.path.basename(test))
      for platform in oses:
        print('    Retrieving results for %s' % platform)
        process = subprocess.Popen(
            [
              sys.executable,
              'swarm_get_results.py',
              '--url', options.swarm_server,
              prefix + 'swarm_client_tests_' + platform +
                os.path.basename(test),
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            cwd=os.path.dirname(ROOT_DIR))
        stdout, _ = process.communicate()

        # Only print the output for failures, successes are unexciting.
        if process.returncode:
          print stdout
          failed_tests.setdefault(test, []).append(platform)
        result = result or process.returncode
  finally:
    shutil.rmtree(tempdir)

  if on_windows:
    print 'Linux and Mac tests skipped since running on Windows.'

  if result:
    print 'Detected the following failures:'
    for test, failed_oses in failed_tests.iteritems():
      print '  %s on %s' % (test, ','.join(failed_oses))
  else:
    print 'No Swarm errors detected :)'

  return result


if __name__ == '__main__':
  sys.exit(main())
