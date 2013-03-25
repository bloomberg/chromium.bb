#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Archives a .isolated file, triggers it on Swarm and get the results."""

import datetime
import getpass
import hashlib
import optparse
import os
import subprocess
import sys

import run_isolated


ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


def run(cmd, verbose):
  if verbose:
    print('Running: %s' % ' '.join(cmd))
  cmd = [sys.executable, os.path.join(ROOT_DIR, cmd[0])] + cmd[1:]
  if verbose and sys.platform != 'win32':
    cmd = ['time', '-p'] + cmd
  subprocess.check_call(cmd)


def doall(isolated, verbose):
  # TODO(maruel): Make them options if necessary.
  swarm_server = 'http://chromium-swarm.appspot.com'
  cad_server = 'http://isolateserver.appspot.com/'
  prefix = getpass.getuser() + '-' + datetime.datetime.now().isoformat() + '-'
  step_name = os.path.basename(isolated).split('.', 1)[0]
  shards = 1

  if verbose:
    os.environ.setdefault('ISOLATE_DEBUG', '1')

  print('Archiving')
  run(
      [
        'isolate.py',
        'hashtable',
        '--isolated', isolated,
        '--outdir', cad_server,
      ],
      verbose)

  print('\nRunning')
  hashval = hashlib.sha1(open(isolated, 'rb').read()).hexdigest()
  run(
      [
        'swarm_trigger_step.py',
        '--swarm-url', swarm_server,
        '--test-name-prefix', prefix,
        '--data-server', cad_server,
        '--run_from_hash',
        hashval,
        step_name,
        str(shards),
        '',
      ],
      verbose)

  print('\nGetting results')
  run(
      [
        'swarm_get_results.py',
        '--url', swarm_server,
        prefix + step_name,
      ],
      verbose)


def main():
  run_isolated.disable_buffering()
  parser = optparse.OptionParser(
      usage='%prog <options> [.isolated]',
      description=sys.modules[__name__].__doc__)
  parser.add_option('-v', '--verbose', action='store_true')
  options, args = parser.parse_args()

  if len(args) != 1:
    parser.error('Require one argument, a .isolate file.')

  isolated = args[0]
  if not os.path.isfile(isolated):
    parser.error(
        'Did you forget to ninja foo_test_run with '
        'GYP_DEFINES="test_isolation_mode=hashtable" ?')
  doall(args[0], options.verbose)
  return 0


if __name__ == '__main__':
  sys.exit(main())
