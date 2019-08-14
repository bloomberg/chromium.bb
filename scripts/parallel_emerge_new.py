# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper script to run emerge, with sane defaults.

Usage:
 ./parallel_emerge [--board=BOARD] [--workon=PKGS]
                   [--force-remote-binary=PKGS] [emerge args] package

This script is a simple wrapper around emerge that handles legacy command line
arguments as well as setting reasonable defaults for parallelism.
"""

from __future__ import print_function

import argparse
import multiprocessing
import os

from chromite.lib import commandline
from chromite.lib import cros_build_lib


class LookupBoardSysroot(argparse.Action):
  """Translates board argument to sysroot location."""

  def __init__(self, option_strings, dest, **kwargs):
    super(LookupBoardSysroot, self).__init__(option_strings, dest, **kwargs)

  def __call__(self, parser, namespace, values, option_string=None):
    sysroot = cros_build_lib.GetSysroot(values)
    setattr(namespace, 'sysroot', sysroot)


def ParallelEmergeArgParser():
  """Helper function to create command line argument parser for this wrapper.

  We need to be compatible with emerge arg format.  We scrape arguments that
  are specific to parallel_emerge, and pass through the rest directly to
  emerge.

  Returns:
    commandline.ArgumentParser that captures arguments specific to
    parallel_emerge
  """
  parser = commandline.ArgumentParser()

  board_group = parser.add_mutually_exclusive_group()
  board_group.add_argument(
      '--board',
      default=None,
      action=LookupBoardSysroot,
  )
  board_group.add_argument(
      '--sysroot',
      action='store',
      metavar='PATH',
  )

  parser.add_argument(
      '--root',
      action='store',
      metavar='PATH',
  )
  parser.add_argument(
      '--workon',
      action='append',
      metavar='PKGS',
  )
  parser.add_argument(
      '--rebuild',
      action='store_true',
      default=False,
  )
  parser.add_argument(
      '--force-remote-binary',
      action='append',
      help=argparse.SUPPRESS,
  )
  parser.add_argument(
      '--root-deps',
      action='store',
      default=None,
      dest='root_deps',
      help=argparse.SUPPRESS,
  )
  parser.add_argument(
      '-j',
      '--jobs',
      default=multiprocessing.cpu_count(),
      metavar='PARALLEL_JOBCOUNT',
  )

  parser.add_argument(
      '--retries',
      help=argparse.SUPPRESS,
      deprecated='Build retries are no longer supported.',
  )
  parser.add_argument(
      '--eventlogfile',
      help=argparse.SUPPRESS,
      deprecated=
      'parallel_emerge no longer records failed packages. Set CROS_METRICS_DIR '
      'in the system environment to get a log of failed packages and which '
      'phase they failed in.',
  )

  return parser


def main(argv):

  parser = ParallelEmergeArgParser()
  parsed_args, emerge_args = parser.parse_known_args(argv)
  parsed_args = vars(parsed_args)

  os.environ['CLEAN_DELAY'] = '0'

  if parsed_args.get('sysroot'):
    emerge_args.extend(['--sysroot', parsed_args['sysroot']])
    os.environ['PORTAGE_CONFIGROOT'] = parsed_args['sysroot']

  if parsed_args.get('root'):
    emerge_args.extend(['--root', parsed_args['root']])

  if parsed_args.get('rebuild'):
    emerge_args.append('--rebuild-if-unbuilt')

  if parsed_args.get('workon'):
    emerge_args.append('--reinstall-atoms=%s' % ' '.join(parsed_args['workon']))
    emerge_args.append('--usepkg-exclude=%s' % ' '.join(parsed_args['workon']))

  if parsed_args.get('force_remote_binary'):
    emerge_args.append(
        '--useoldpkg-atoms=%s' % ' '.join(parsed_args['force_remote_binary']))

  if parsed_args.get('root_deps') is not None:
    emerge_args.append('--root-deps=%s' % parsed_args['root_deps'])
  else:
    emerge_args.append('--root-deps')

  emerge_args.append('--jobs=%s' % parsed_args['jobs'])
  emerge_args.append('--rebuild-exclude=chromeos-base/chromeos-chrome')

  # TODO(cjmcdonald): Change the exec target back to just 'emerge' once
  #                   python3 is the default in the SDK.
  os.execvp('/usr/lib64/python-exec/python3.6/emerge', ['emerge'] + emerge_args)
