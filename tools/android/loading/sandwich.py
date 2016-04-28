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
import shutil
import sys

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
from pylib import constants
import devil_chromium

import chrome_cache
import common_util
import device_setup
import emulation
import options
import sandwich_metrics
import sandwich_misc
from sandwich_runner import SandwichRunner
import sandwich_task_builder
import task_manager
from trace_test.webserver_test import WebServer


# Use options layer to access constants.
OPTIONS = options.OPTIONS

_SPEED_INDEX_MEASUREMENT = 'speed-index'
_MEMORY_MEASUREMENT = 'memory'


def _ArgumentParser():
  """Build a command line argument's parser."""
  # Command parser when dealing with jobs.
  common_job_parser = argparse.ArgumentParser(add_help=False)
  common_job_parser.add_argument('--job', required=True,
                                 help='JSON file with job description.')
  common_job_parser.add_argument('--android', default=None, type=str,
                                 dest='android_device_serial',
                                 help='Android device\'s serial to use.')

  task_parser = task_manager.CommandLineParser()

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
  filter_cache_parser.add_argument('--subresource-discoverer', required=True,
      help='Strategy for populating the cache with a subset of resources, '
           'according to the way they can be discovered',
      choices=sandwich_misc.SUBRESOURCE_DISCOVERERS)
  filter_cache_parser.add_argument('--output', type=str, required=True,
                                   dest='output_cache_archive_path',
                                   help='Path of filtered cache archive.')
  filter_cache_parser.add_argument('loading_trace_paths', type=str, nargs='+',
      metavar='LOADING_TRACE',
      help='A list of loading traces generated by a sandwich run for a given' +
          ' url. This is used to have a resource dependency graph to white-' +
          'list the ones discoverable by the HTML pre-scanner for that given ' +
          'url.')

  # Record test trace subcommand.
  record_trace_parser = subparsers.add_parser('record-test-trace',
      help='Record a test trace using the trace_test.webserver_test.')
  record_trace_parser.add_argument('--source-dir', type=str, required=True,
                                   help='Base path where the files are opened'
                                        'by the web server.')
  record_trace_parser.add_argument('--page', type=str, required=True,
                                   help='Source page in source-dir to navigate '
                                        'to.')
  record_trace_parser.add_argument('-o', '--output', type=str, required=True,
                                   help='Output path of the generated trace.')

  # Run all subcommand.
  run_all = subparsers.add_parser('run-all',
                       parents=[common_job_parser, task_parser],
                       help='Run all steps using the task manager '
                            'infrastructure.')
  run_all.add_argument('-m', '--measure', default=[], dest='optional_measures',
                       choices=[_SPEED_INDEX_MEASUREMENT, _MEMORY_MEASUREMENT],
                       nargs='+', help='Enable optional measurements.')
  run_all.add_argument('-g', '--gen-full', action='store_true',
                       help='Generate the full graph with all possible'
                            'benchmarks.')
  run_all.add_argument('--wpr-archive', default=None, type=str,
                       dest='wpr_archive_path',
                       help='WebPageReplay archive to use, instead of '
                            'generating one.')
  run_all.add_argument('--url-repeat', default=1, type=int,
                       help='How many times to repeat the urls.')

  return parser


def _CreateSandwichRunner(args):
  sandwich_runner = SandwichRunner()
  sandwich_runner.LoadJob(args.job)
  sandwich_runner.PullConfigFromArgs(args)
  if args.android_device_serial is not None:
    sandwich_runner.android_device = \
        device_setup.GetDeviceFromSerial(args.android_device_serial)
  return sandwich_runner


def _RecordWprMain(args):
  sandwich_runner = _CreateSandwichRunner(args)
  sandwich_runner.wpr_record = True
  sandwich_runner.PrintConfig()
  if not os.path.isdir(os.path.dirname(args.wpr_archive_path)):
    os.makedirs(os.path.dirname(args.wpr_archive_path))
  sandwich_runner.Run()
  return 0


def _CreateCacheMain(args):
  sandwich_runner = _CreateSandwichRunner(args)
  sandwich_runner.cache_operation = 'save'
  sandwich_runner.PrintConfig()
  if not os.path.isdir(os.path.dirname(args.cache_archive_path)):
    os.makedirs(os.path.dirname(args.cache_archive_path))
  sandwich_runner.Run()
  return 0


def _RunJobMain(args):
  sandwich_runner = _CreateSandwichRunner(args)
  sandwich_runner.PrintConfig()
  sandwich_runner.Run()
  return 0


def _ExtractMetricsMain(args):
  run_metrics_list = sandwich_metrics.ExtractMetricsFromRunnerOutputDirectory(
      args.trace_output_directory)
  run_metrics_list.sort(key=lambda e: e['repeat_id'])
  with open(args.metrics_csv_path, 'w') as csv_file:
    writer = csv.DictWriter(csv_file,
                            fieldnames=sandwich_metrics.CSV_FIELD_NAMES)
    writer.writeheader()
    for run_metrics in run_metrics_list:
      writer.writerow(run_metrics)
  return 0


def _FilterCacheMain(args):
  whitelisted_urls = set()
  for loading_trace_path in args.loading_trace_paths:
    whitelisted_urls.update(sandwich_misc.ExtractDiscoverableUrls(
        loading_trace_path, args.subresource_discoverer))
  if not os.path.isdir(os.path.dirname(args.output_cache_archive_path)):
    os.makedirs(os.path.dirname(args.output_cache_archive_path))
  chrome_cache.ApplyUrlWhitelistToCacheArchive(args.cache_archive_path,
                                               whitelisted_urls,
                                               args.output_cache_archive_path)
  return 0


def _RecordWebServerTestTrace(args):
  with common_util.TemporaryDirectory() as out_path:
    sandwich_runner = SandwichRunner()
    # Reuse the WPR's forwarding to access the webpage from Android.
    sandwich_runner.wpr_record = True
    sandwich_runner.wpr_archive_path = os.path.join(out_path, 'wpr')
    sandwich_runner.trace_output_directory = os.path.join(out_path, 'run')
    with WebServer.Context(
        source_dir=args.source_dir, communication_dir=out_path) as server:
      address = server.Address()
      sandwich_runner.urls = ['http://%s/%s' % (address, args.page)]
      sandwich_runner.Run()
    trace_path = os.path.join(
        out_path, 'run', '0', sandwich_runner.TRACE_FILENAME)
    shutil.copy(trace_path, args.output)
  return 0


def _RunAllMain(args):
  android_device = None
  if args.android_device_serial:
    android_device = \
        device_setup.GetDeviceFromSerial(args.android_device_serial)
  builder = sandwich_task_builder.SandwichTaskBuilder(
      output_directory=args.output,
      android_device=android_device,
      job_path=args.job)
  if args.wpr_archive_path:
    builder.OverridePathToWprArchive(args.wpr_archive_path)
  else:
    builder.PopulateWprRecordingTask()
  builder.PopulateCommonPipelines()

  def MainTransformer(runner):
    runner.record_video = _SPEED_INDEX_MEASUREMENT in args.optional_measures
    runner.record_memory_dumps = _MEMORY_MEASUREMENT in args.optional_measures
    runner.job_repeat = args.url_repeat

  transformer_list_name = 'no-network-emulation'
  builder.PopulateLoadBenchmark(sandwich_misc.EMPTY_CACHE_DISCOVERER,
                                transformer_list_name,
                                transformer_list=[MainTransformer])
  builder.PopulateLoadBenchmark(sandwich_misc.FULL_CACHE_DISCOVERER,
                                transformer_list_name,
                                transformer_list=[MainTransformer])

  if args.gen_full:
    for network_condition in ['Regular4G', 'Regular3G', 'Regular2G']:
      transformer_list_name = network_condition.lower()
      network_transformer = \
          sandwich_task_builder.NetworkSimulationTransformer(network_condition)
      transformer_list = [MainTransformer, network_transformer]
      for subresource_discoverer in sandwich_misc.SUBRESOURCE_DISCOVERERS:
        if subresource_discoverer == sandwich_misc.FULL_CACHE_DISCOVERER:
          continue
        builder.PopulateLoadBenchmark(subresource_discoverer,
            transformer_list_name, transformer_list)

  return task_manager.ExecuteWithCommandLine(
      args, builder.tasks.values(), builder.default_final_tasks)


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
  if args.subcommand == 'record-test-trace':
    return _RecordWebServerTestTrace(args)
  if args.subcommand == 'run-all':
    return _RunAllMain(args)
  assert False


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
