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
import multiprocessing
import os
import shutil
import subprocess
import sys


INTERNAL_ERROR_EXIT_CODE = -1000


def _Spawn(args):
  """Triggers a swarming job. The arguments passed are:
  - The index of the job;
  - The command line arguments object;
  - The hash of the isolate job used to trigger.

  The return value is passed to a collect-style map() and consists of:
  - The index of the job;
  - The json file created by triggering and used to collect results;
  - The command line arguments object.
  """
  index, args, isolated_hash = args
  json_file = os.path.join(args.results, '%d.json' % index)
  trigger_args = [
      'tools/swarming_client/swarming.py', 'trigger',
      '-S', 'https://chromium-swarm.appspot.com',
      '-I', 'https://isolateserver.appspot.com',
      '-d', 'os', 'Linux',
      '-d', 'pool', 'Chrome',
      '-d', 'kvm', '1',
      '-s', isolated_hash,
      '--dump-json', json_file,
      '--',
      '--test-launcher-summary-output=${ISOLATED_OUTDIR}/output.json']
  filter_file = \
      'testing/buildbot/filters/fuchsia.' + args.test_name + '.filter'
  if os.path.isfile(filter_file):
    trigger_args.append('--test-launcher-filter-file=../../' + filter_file)
  with open(os.devnull, 'w') as nul:
    subprocess.check_call(trigger_args, stdout=nul)
  return (index, json_file, args)


def _Collect(spawn_result):
  index, json_file, args = spawn_result
  p = subprocess.Popen([
    'tools/swarming_client/swarming.py', 'collect',
    '-S', 'https://chromium-swarm.appspot.com',
    '--json', json_file,
    '--task-output-stdout=console'],
    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  stdout = p.communicate()[0]
  if p.returncode != 0 and len(stdout) < 2**10 and 'Internal error!' in stdout:
    exit_code = INTERNAL_ERROR_EXIT_CODE
    file_suffix = '.INTERNAL_ERROR'
  else:
    exit_code = p.returncode
    file_suffix = '' if exit_code == 0 else '.FAILED'
  filename = '%d%s.stdout.txt' % (index, file_suffix)
  with open(os.path.join(args.results, filename), 'w') as f:
    f.write(stdout)
  return exit_code


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

  if os.path.isdir(args.results):
    shutil.rmtree(args.results)
  os.makedirs(args.results)

  try:
    print 'Triggering %d tasks...' % args.copies
    pool = multiprocessing.Pool()
    spawn_args = zip(range(args.copies),
                     [args] * args.copies,
                     [isolated_hash] * args.copies)
    spawn_results = pool.imap_unordered(_Spawn, spawn_args)

    exit_codes = []
    collect_results = pool.imap_unordered(_Collect, spawn_results)
    for result in collect_results:
      exit_codes.append(result)
      successes = sum(1 for x in exit_codes if x == 0)
      errors = sum(1 for x in exit_codes if x == INTERNAL_ERROR_EXIT_CODE)
      failures = len(exit_codes) - successes - errors
      clear_to_eol = '\033[K'
      print('\r[%d/%d] collected: '
            '%d successes, %d failures, %d bot errors...%s' % (len(exit_codes),
                args.copies, successes, failures, errors, clear_to_eol)),
      sys.stdout.flush()

    print
    print 'Results logs collected into', os.path.abspath(args.results) + '.'
  finally:
    pool.close()
    pool.join()
  return 0


if __name__ == '__main__':
  sys.exit(main())
