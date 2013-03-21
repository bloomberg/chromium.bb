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

  oses = ('win32', 'linux2', 'darwin')
  tests = [
    os.path.relpath(i, ROOT_DIR)
    for i in glob.iglob(os.path.join(ROOT_DIR, '..', 'tests', '*_test.py'))
  ]

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
    for test in tests:
      subprocess.check_call(
          [
            sys.executable,
            'isolate.py',
            'hashtable',
            '--isolate', os.path.join(ROOT_DIR, 'run_a_test.isolate'),
            '--isolated', isolated,
            '--outdir', options.isolate_server,
            '--variable', 'TEST_EXECUTABLE', test,
          ],
          cwd=os.path.dirname(ROOT_DIR))
      hashvals.append(hashlib.sha1(open(isolated, 'rb').read()).hexdigest())

    print('\nTriggering')
    for i, test in enumerate(tests):
      sys.stdout.write('  %s: ' % os.path.basename(test))
      for platform in oses:
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
              '--run_from_hash', hashvals[i],
              'swarm_client_tests_' + platform + os.path.basename(test),
              # Number of shards.
              '1',
              '',
            ],
            cwd=os.path.dirname(ROOT_DIR))
      sys.stdout.write('\n')

    print('\nGetting results')
    for i, test in enumerate(tests):
      print('  %s' % os.path.basename(test))
      for platform in oses:
        result = result or subprocess.call(
            [
              sys.executable,
              'swarm_get_results.py',
              '--url', options.swarm_server,
              prefix + 'swarm_client_tests_' + platform +
                os.path.basename(test),
            ],
            cwd=os.path.dirname(ROOT_DIR))
  finally:
    shutil.rmtree(tempdir)
  return result


if __name__ == '__main__':
  sys.exit(main())
