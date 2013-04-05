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
import shutil
import subprocess
import sys
import tempfile

import run_isolated


ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


def run(cmd, verbose):
  if verbose:
    print('Running: %s' % ' '.join(cmd))
  cmd = [sys.executable, os.path.join(ROOT_DIR, cmd[0])] + cmd[1:]
  if verbose and sys.platform != 'win32':
    cmd = ['time', '-p'] + cmd
  subprocess.check_call(cmd)


def doall(isolate, isolated, swarm_server, cad_server, slave_os, verbose):
  """Does the archive, trigger and get results dance."""
  prefix = getpass.getuser() + '-' + datetime.datetime.now().isoformat() + '-'
  shards = 1

  if verbose:
    os.environ.setdefault('ISOLATE_DEBUG', '2')

  tempdir = None
  try:
    if not isolated:
      # A directory is used because isolated + '.state' will also be created.
      tempdir = tempfile.mkdtemp(prefix='swarm_trigger_and_get_results')
      isolated = os.path.join(tempdir, 'swarm_trigger.isolated')
      step_name = os.path.basename(isolate).split('.', 1)[0]
    else:
      step_name = os.path.basename(isolated).split('.', 1)[0]

    print('Archiving')
    cmd = [
      'isolate.py',
      'hashtable',
      '--outdir', cad_server,
      '--isolated', isolated,
    ]
    if isolate:
      cmd.extend(('--isolate', isolate))
    if slave_os:
      cmd.extend(('-V', 'OS', run_isolated.FLAVOR_MAPPING[slave_os]))
    run(cmd, verbose)

    print('\nRunning')
    hashval = hashlib.sha1(open(isolated, 'rb').read()).hexdigest()
    cmd = [
      'swarm_trigger_step.py',
      '--swarm-url', swarm_server,
      '--test-name-prefix', prefix,
      '--data-server', cad_server,
      '--run_from_hash',
      hashval,
      step_name,
      str(shards),
      '',
    ]
    if slave_os:
      cmd.extend(('--os_image', slave_os))
    run(cmd, verbose)

    print('\nGetting results')
    run(
        [
          'swarm_get_results.py',
          '--url', swarm_server,
          prefix + step_name,
        ],
        verbose)
    return 0
  finally:
    if tempdir:
      shutil.rmtree(tempdir)


def main():
  run_isolated.disable_buffering()
  parser = optparse.OptionParser(
      usage='%prog <options>',
      description=sys.modules[__name__].__doc__)
  parser.add_option('-i', '--isolate', help='.isolate file to use')
  parser.add_option(
      '-s', '--isolated',
      help='.isolated file to use. One of -i or -s must be used.')
  parser.add_option(
      '-o', '--os_image',
      metavar='OS',
      help='Swarm OS image to request. Should be one of the valid sys.platform '
           'values like darwin, linux2 or win32.')
  parser.add_option(
      '-u', '--swarm-url',
      metavar='URL',
      default='https://chromium-swarm-dev.appspot.com',
      help='Specify the url of the Swarm server. Defaults to %default')
  parser.add_option(
      '-d', '--data-server',
      default='https://isolateserver-dev.appspot.com/',
      metavar='URL',
      help='The server where all the test data is stored. Defaults to %default')
  parser.add_option('-v', '--verbose', action='store_true')
  options, args = parser.parse_args()

  if args:
    parser.error('Use one of -i or -s but no unsupported arguments: %s' % args)
  if not options.isolate and not options.isolated:
    parser.error('Use one of -i or -s')

  return doall(
      options.isolate,
      options.isolated,
      options.swarm_url,
      options.data_server,
      options.os_image,
      options.verbose)


if __name__ == '__main__':
  sys.exit(main())
