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
  task_parser = task_manager.CommandLineParser()

  # Command parser when dealing with SandwichRunner.
  sandwich_runner_parser = argparse.ArgumentParser(add_help=False)
  sandwich_runner_parser.add_argument('--android', default=None, type=str,
                                      dest='android_device_serial',
                                      help='Android device\'s serial to use.')

  # Plumbing parser to configure OPTIONS.
  plumbing_parser = OPTIONS.GetParentParser('plumbing options')

  # Main parser
  parser = argparse.ArgumentParser(parents=[plumbing_parser])
  subparsers = parser.add_subparsers(dest='subcommand', help='subcommand line')

  # Record test trace subcommand.
  record_trace_parser = subparsers.add_parser('record-test-trace',
      parents=[sandwich_runner_parser],
      help='Record a test trace using the trace_test.webserver_test.')
  record_trace_parser.add_argument('--source-dir', type=str, required=True,
                                   help='Base path where the files are opened '
                                        'by the web server.')
  record_trace_parser.add_argument('--page', type=str, required=True,
                                   help='Source page in source-dir to navigate '
                                        'to.')
  record_trace_parser.add_argument('-o', '--output', type=str, required=True,
                                   help='Output path of the generated trace.')

  # Run subcommand.
  run_parser = subparsers.add_parser('run',
      parents=[sandwich_runner_parser, task_parser],
      help='Run all steps using the task manager infrastructure.')
  run_parser.add_argument('-g', '--gen-full', action='store_true',
                          help='Generate the full graph with all possible '
                               'benchmarks.')
  run_parser.add_argument('--job', required=True,
      help='JSON file with job description such as in sandwich_jobs/.')
  run_parser.add_argument('-m', '--measure', default=[], nargs='+',
      choices=[_SPEED_INDEX_MEASUREMENT, _MEMORY_MEASUREMENT],
      dest='optional_measures', help='Enable optional measurements.')
  run_parser.add_argument('--wpr-archive', default=None, type=str,
                          dest='wpr_archive_path',
                          help='WebPageReplay archive to use, instead of '
                               'generating one.')
  run_parser.add_argument('--url-repeat', default=1, type=int,
                          help='How many times to repeat the urls.')

  return parser


def _GetAndroidDeviceFromArgs(args):
  if args.android_device_serial:
    return device_setup.GetDeviceFromSerial(args.android_device_serial)
  return None


def _RecordWebServerTestTrace(args):
  with common_util.TemporaryDirectory() as out_path:
    sandwich_runner = SandwichRunner()
    sandwich_runner.android_device = _GetAndroidDeviceFromArgs(args)
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
  builder = sandwich_task_builder.SandwichTaskBuilder(
      output_directory=args.output,
      android_device=_GetAndroidDeviceFromArgs(args),
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

  if args.subcommand == 'record-test-trace':
    return _RecordWebServerTestTrace(args)
  if args.subcommand == 'run':
    return _RunAllMain(args)
  assert False


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
