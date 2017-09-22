#!/usr/bin/env python

# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a Fuchsia gtest-based test on Swarming, optionally many times,
collecting the output of the runs into a directory. Useful for flake checking,
and faster than using trybots by avoiding repeated bot_update, compile, archive,
etc. and allowing greater parallelism.

To use, run in a new shell (it blocks until all Swarming jobs complete):

  tools/fuchsia/run-swarmed.py -t content_unittests --out-dir=out/fuch

The logs of the runs will be stored in results/ (or specify a results directory
with --results=some_dir). You can then do something like `grep -L SUCCESS
results/*` to find the tests that failed or otherwise process the log files.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--out-dir', default='out/fuch', help='Build directory.')
  parser.add_argument('--test-name', '-t', required=True,
                      help='Name of test to run.')
  parser.add_argument('--copies', '-n', type=int, default=1,
                      help='Number of copies to spawn.')
  parser.add_argument('--results', '-r', default='results',
                      help='Directory in which to store results.')
  args = parser.parse_args()

  subprocess.check_call(
      ['tools/mb/mb.py', 'isolate', '//' + args.out_dir, args.test_name])

  print 'If you get authentication errors, follow:'
  print '  https://www.chromium.org/developers/testing/isolated-testing/for-swes#TOC-Login-on-the-services'

  print 'Uploading to isolate server, this can take a while...'
  archive_output = subprocess.check_output(
      ['tools/swarming_client/isolate.py', 'archive',
       '-I', 'https://isolateserver.appspot.com',
       '-i', os.path.join(args.out_dir, args.test_name + '.isolate'),
       '-s', os.path.join(args.out_dir, args.test_name + '.isolated')])
  isolated_hash = archive_output.split()[0]
  json_files = []
  for i in range(args.copies):
    cur_json = tempfile.NamedTemporaryFile()
    json_files.append(cur_json)
    trigger_args = ['tools/swarming_client/swarming.py', 'trigger',
        '-S', 'https://chromium-swarm.appspot.com',
        '-I', 'https://isolateserver.appspot.com',
        '-d', 'os', 'Linux',
        '-d', 'pool', 'Chrome',
        '-d', 'kvm', '1',
        '-s', isolated_hash,
        '--dump-json', cur_json.name,
        '--',
        '--test-launcher-summary-output=${ISOLATED_OUTDIR}/output.json']
    filter_file = \
        'testing/buildbot/filters/fuchsia.' + args.test_name + '.filter'
    if os.path.isfile(filter_file):
      trigger_args.append('--test-launcher-filter-file=../../' + filter_file)
    subprocess.check_call(trigger_args)

  if os.path.isdir(args.results):
    shutil.rmtree(args.results)
  os.makedirs(args.results)

  print 'Waiting for run%s to complete...' % ('' if args.copies == 1 else 's')
  for i, json in enumerate(json_files):
    p = subprocess.Popen(['tools/swarming_client/swarming.py', 'collect',
      '-S', 'https://chromium-swarm.appspot.com',
      '--json', json.name,
      '--task-output-stdout=console'], stdout=subprocess.PIPE)
    stdout = p.communicate()[0]
    filename = str(i) + ('' if p.returncode == 0 else '.FAILED') + '.stdout.txt'
    with open(os.path.join(args.results, filename), 'w') as f:
      f.write(stdout)

  print 'Results logs collected into', args.results + '.'
  return 0


if __name__ == '__main__':
  sys.exit(main())
