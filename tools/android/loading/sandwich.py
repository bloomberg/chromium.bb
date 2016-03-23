#! /usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Instructs Chrome to load series of web pages and reports results.

When running Chrome is sandwiched between preprocessed disk caches and
WepPageReplay serving all connections.

TODO(pasko): implement cache preparation and WPR.
"""

import argparse
import csv
import logging
import os
import sys

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
from pylib import constants
import devil_chromium

import chrome_cache
import emulation
import options
import sandwich_metrics
import sandwich_misc
from sandwich_runner import SandwichRunner


# Use options layer to access constants.
OPTIONS = options.OPTIONS


def _ArgumentParser():
  """Build a command line argument's parser."""
  # Command parser when dealing with jobs.
  common_job_parser = argparse.ArgumentParser(add_help=False)
  common_job_parser.add_argument('--job', required=True,
                                 help='JSON file with job description.')

  # Plumbing parser to configure OPTIONS.
  plumbing_parser = OPTIONS.GetParentParser('plumbing options')

  # Main parser
  parser = argparse.ArgumentParser(parents=[plumbing_parser])
  subparsers = parser.add_subparsers(dest='subcommand', help='subcommand line')

  # Record WPR subcommand.
  record_wpr = subparsers.add_parser('record-wpr', parents=[common_job_parser],
                                     help='Record WPR from sandwich job.')
  record_wpr.add_argument('--wpr-archive', required=True, type=str,
                          dest='wpr_archive_path',
                          help='Web page replay archive to generate.')

  # Patch WPR subcommand.
  patch_wpr = subparsers.add_parser('patch-wpr',
                                     help='Patch WPR response headers.')
  patch_wpr.add_argument('--wpr-archive', required=True, type=str,
                         dest='wpr_archive_path',
                         help='Web page replay archive to patch.')

  # Create cache subcommand.
  create_cache_parser = subparsers.add_parser('create-cache',
      parents=[common_job_parser],
      help='Create cache from sandwich job.')
  create_cache_parser.add_argument('--cache-archive', required=True, type=str,
                                   dest='cache_archive_path',
                                   help='Cache archive destination path.')
  create_cache_parser.add_argument('--wpr-archive', default=None, type=str,
                                   dest='wpr_archive_path',
                                   help='Web page replay archive to create ' +
                                       'the cache from.')

  # Run subcommand.
  run_parser = subparsers.add_parser('run', parents=[common_job_parser],
                                     help='Run sandwich benchmark.')
  run_parser.add_argument('--output', required=True, type=str,
                          dest='trace_output_directory',
                          help='Path of output directory to create.')
  run_parser.add_argument('--cache-archive', type=str,
                          dest='cache_archive_path',
                          help='Cache archive destination path.')
  run_parser.add_argument('--cache-op',
                          choices=['clear', 'push', 'reload'],
                          dest='cache_operation',
                          default='clear',
                          help='Configures cache operation to do before '
                              +'launching Chrome. (Default is clear). The push'
                              +' cache operation requires --cache-archive to '
                              +'set.')
  run_parser.add_argument('--disable-wpr-script-injection',
                          action='store_true',
                          help='Disable WPR default script injection such as ' +
                              'overriding javascript\'s Math.random() and ' +
                              'Date() with deterministic implementations.')
  run_parser.add_argument('--network-condition', default=None,
      choices=sorted(emulation.NETWORK_CONDITIONS.keys()),
      help='Set a network profile.')
  run_parser.add_argument('--network-emulator', default='browser',
      choices=['browser', 'wpr'],
      help='Set which component is emulating the network condition.' +
          ' (Default to browser). Wpr network emulator requires --wpr-archive' +
          ' to be set.')
  run_parser.add_argument('--job-repeat', default=1, type=int,
                          help='How many times to run the job.')
  run_parser.add_argument('--record-video', action='store_true',
                          help='Configures either to record or not a video of '
                              +'chrome loading the web pages.')
  run_parser.add_argument('--wpr-archive', default=None, type=str,
                          dest='wpr_archive_path',
                          help='Web page replay archive to load job\'s urls ' +
                              'from.')

  # Pull metrics subcommand.
  create_cache_parser = subparsers.add_parser('extract-metrics',
      help='Extracts metrics from a loading trace and saves as CSV.')
  create_cache_parser.add_argument('--trace-directory', required=True,
                                   dest='trace_output_directory', type=str,
                                   help='Path of loading traces directory.')
  create_cache_parser.add_argument('--out-metrics', default=None, type=str,
                                   dest='metrics_csv_path',
                                   help='Path where to save the metrics\'s '+
                                      'CSV.')

  # Filter cache subcommand.
  filter_cache_parser = subparsers.add_parser('filter-cache',
      help='Cache filtering that keeps only resources discoverable by the HTML'+
          ' document parser.')
  filter_cache_parser.add_argument('--cache-archive', type=str, required=True,
                                   dest='cache_archive_path',
                                   help='Path of the cache archive to filter.')
  filter_cache_parser.add_argument('--output', type=str, required=True,
                                   dest='output_cache_archive_path',
                                   help='Path of filtered cache archive.')
  filter_cache_parser.add_argument('loading_trace_paths', type=str, nargs='+',
      metavar='LOADING_TRACE',
      help='A list of loading traces generated by a sandwich run for a given' +
          ' url. This is used to have a resource dependency graph to white-' +
          'list the ones discoverable by the HTML pre-scanner for that given ' +
          'url.')

  return parser


def _RecordWprMain(args):
  sandwich_runner = SandwichRunner(args.job)
  sandwich_runner.PullConfigFromArgs(args)
  sandwich_runner.wpr_record = True
  sandwich_runner.PrintConfig()
  if not os.path.isdir(os.path.dirname(args.wpr_archive_path)):
    os.makedirs(os.path.dirname(args.wpr_archive_path))
  sandwich_runner.Run()
  return 0


def _CreateCacheMain(args):
  sandwich_runner = SandwichRunner(args.job)
  sandwich_runner.PullConfigFromArgs(args)
  sandwich_runner.cache_operation = 'save'
  sandwich_runner.PrintConfig()
  if not os.path.isdir(os.path.dirname(args.cache_archive_path)):
    os.makedirs(os.path.dirname(args.cache_archive_path))
  sandwich_runner.Run()
  return 0


def _RunJobMain(args):
  sandwich_runner = SandwichRunner(args.job)
  sandwich_runner.PullConfigFromArgs(args)
  sandwich_runner.PrintConfig()
  sandwich_runner.Run()
  return 0


def _ExtractMetricsMain(args):
  trace_metrics_list = sandwich_metrics.PullMetricsFromOutputDirectory(
      args.trace_output_directory)
  trace_metrics_list.sort(key=lambda e: e['id'])
  with open(args.metrics_csv_path, 'w') as csv_file:
    writer = csv.DictWriter(csv_file,
                            fieldnames=sandwich_metrics.CSV_FIELD_NAMES)
    writer.writeheader()
    for trace_metrics in trace_metrics_list:
      writer.writerow(trace_metrics)
  return 0


def _FilterCacheMain(args):
  whitelisted_urls = set()
  for loading_trace_path in args.loading_trace_paths:
    whitelisted_urls.update(
        sandwich_misc.ExtractParserDiscoverableResources(loading_trace_path))
  if not os.path.isdir(os.path.dirname(args.output_cache_archive_path)):
    os.makedirs(os.path.dirname(args.output_cache_archive_path))
  chrome_cache.ApplyUrlWhitelistToCacheArchive(args.cache_archive_path,
                                               whitelisted_urls,
                                               args.output_cache_archive_path)
  return 0


def main(command_line_args):
  logging.basicConfig(level=logging.INFO)
  devil_chromium.Initialize()

  args = _ArgumentParser().parse_args(command_line_args)
  OPTIONS.SetParsedArgs(args)

  if args.subcommand == 'record-wpr':
    return _RecordWprMain(args)
  if args.subcommand == 'patch-wpr':
    sandwich_misc.PatchWpr(args.wpr_archive_path)
    return 0
  if args.subcommand == 'create-cache':
    return _CreateCacheMain(args)
  if args.subcommand == 'run':
    return _RunJobMain(args)
  if args.subcommand == 'extract-metrics':
    return _ExtractMetricsMain(args)
  if args.subcommand == 'filter-cache':
    return _FilterCacheMain(args)
  assert False


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
