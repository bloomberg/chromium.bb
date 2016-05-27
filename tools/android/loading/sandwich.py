#! /usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Instructs Chrome to load series of web pages and reports results.

When running Chrome is sandwiched between preprocessed disk caches and
WepPageReplay serving all connections.
"""

import argparse
import json
import logging
import os
import shutil
import sys
from urlparse import urlparse

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
from pylib import constants
import devil_chromium

import chrome_cache
import common_util
import csv_util
import device_setup
import emulation
import options
import sandwich_metrics
import sandwich_misc
import sandwich_runner
import sandwich_swr
import sandwich_task_builder
import task_manager
from trace_test.webserver_test import WebServer


# Use options layer to access constants.
OPTIONS = options.OPTIONS

_SPEED_INDEX_MEASUREMENT = 'speed-index'
_MEMORY_MEASUREMENT = 'memory'
_CORPUS_DIR = 'sandwich_corpuses'


def ReadUrlsFromCorpus(corpus_path):
  """Retrieves the list of URLs associated with the corpus name."""
  try:
    # Attempt to read by regular file name.
    json_file_name = corpus_path
    with open(json_file_name) as f:
      json_data = json.load(f)
  except IOError:
    # Extra sugar: attempt to load from _CORPUS_DIR.
    json_file_name = os.path.join(
        os.path.dirname(__file__), _CORPUS_DIR, corpus_path)
    with open(json_file_name) as f:
      json_data = json.load(f)

  key = 'urls'
  if json_data and key in json_data:
    url_list = json_data[key]
    if isinstance(url_list, list) and len(url_list) > 0:
      return url_list
  raise Exception(
      'File {} does not define a list named "urls"'.format(json_file_name))


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
  parser = argparse.ArgumentParser(parents=[plumbing_parser],
      fromfile_prefix_chars=task_manager.FROMFILE_PREFIX_CHARS)
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
  run_parser.add_argument('-c', '--corpus', required=True,
      help='Path to a JSON file with a corpus such as in %s/.' % _CORPUS_DIR)
  run_parser.add_argument('-m', '--measure', default=[], nargs='+',
      choices=[_SPEED_INDEX_MEASUREMENT, _MEMORY_MEASUREMENT],
      dest='optional_measures', help='Enable optional measurements.')
  run_parser.add_argument('--wpr-archive', default=None, type=str,
                          dest='wpr_archive_path',
                          help='WebPageReplay archive to use, instead of '
                               'generating one.')
  run_parser.add_argument('-r', '--url-repeat', default=1, type=int,
                          help='How many times to repeat the urls.')
  run_parser.add_argument('--swr-benchmark', action='store_true',
                          help='Run the Stale-While-Revalidate benchmarks.')

  # Collect subcommand.
  collect_csv_parser = subparsers.add_parser('collect-csv',
      help='Collects all CSVs from Sandwich output directory into a single '
           'CSV.')
  collect_csv_parser.add_argument('output_dir', type=str,
                                  help='Path to the run output directory.')
  collect_csv_parser.add_argument('output_csv', type=argparse.FileType('w'),
                                  help='Path to the output CSV.')

  return parser


def _GetAndroidDeviceFromArgs(args):
  if args.android_device_serial:
    return device_setup.GetDeviceFromSerial(args.android_device_serial)
  return None


def _RecordWebServerTestTrace(args):
  with common_util.TemporaryDirectory() as out_path:
    runner = sandwich_runner.SandwichRunner()
    runner.android_device = _GetAndroidDeviceFromArgs(args)
    # Reuse the WPR's forwarding to access the webpage from Android.
    runner.wpr_record = True
    runner.wpr_archive_path = os.path.join(out_path, 'wpr')
    runner.output_dir = os.path.join(out_path, 'run')
    with WebServer.Context(
        source_dir=args.source_dir, communication_dir=out_path) as server:
      address = server.Address()
      runner.url = 'http://%s/%s' % (address, args.page)
      runner.Run()
    trace_path = os.path.join(
        out_path, 'run', '0', sandwich_runner.TRACE_FILENAME)
    shutil.copy(trace_path, args.output)
  return 0


def _GenerateBenchmarkTasks(args, url, output_subdirectory):
  MAIN_TRANSFORMER_LIST_NAME = 'no-network-emulation'
  common_builder = sandwich_task_builder.SandwichCommonBuilder(
      android_device=_GetAndroidDeviceFromArgs(args),
      url=url,
      output_directory=args.output,
      output_subdirectory=output_subdirectory)
  if args.wpr_archive_path:
    common_builder.OverridePathToWprArchive(args.wpr_archive_path)
  else:
    common_builder.PopulateWprRecordingTask()

  def MainTransformer(runner):
    runner.record_video = _SPEED_INDEX_MEASUREMENT in args.optional_measures
    runner.record_memory_dumps = _MEMORY_MEASUREMENT in args.optional_measures
    runner.repeat = args.url_repeat

  if not args.swr_benchmark:
    builder = sandwich_task_builder.PrefetchBenchmarkBuilder(common_builder)
    builder.PopulateLoadBenchmark(sandwich_misc.EMPTY_CACHE_DISCOVERER,
                                  MAIN_TRANSFORMER_LIST_NAME,
                                  transformer_list=[MainTransformer])
    builder.PopulateLoadBenchmark(sandwich_misc.FULL_CACHE_DISCOVERER,
                                  MAIN_TRANSFORMER_LIST_NAME,
                                  transformer_list=[MainTransformer])
    if args.gen_full:
      for network_condition in ['Regular4G', 'Regular3G', 'Regular2G']:
        transformer_list_name = network_condition.lower()
        network_transformer = \
            sandwich_task_builder.NetworkSimulationTransformer(
                network_condition)
        transformer_list = [MainTransformer, network_transformer]
        for subresource_discoverer in sandwich_misc.SUBRESOURCE_DISCOVERERS:
          if subresource_discoverer == sandwich_misc.FULL_CACHE_DISCOVERER:
            continue
          builder.PopulateLoadBenchmark(subresource_discoverer,
              transformer_list_name, transformer_list)
  else:
    builder = sandwich_swr.StaleWhileRevalidateBenchmarkBuilder(common_builder)
    for enable_swr in [False, True]:
      builder.PopulateBenchmark(enable_swr, MAIN_TRANSFORMER_LIST_NAME,
                                transformer_list=[MainTransformer])
      for network_condition in ['Regular3G', 'Regular2G']:
        transformer_list_name = network_condition.lower()
        network_transformer = \
            sandwich_task_builder.NetworkSimulationTransformer(
                network_condition)
        transformer_list = [MainTransformer, network_transformer]
        builder.PopulateBenchmark(enable_swr, transformer_list_name,
                                  transformer_list)
  return common_builder.default_final_tasks


def _RunAllMain(args):
  urls = ReadUrlsFromCorpus(args.corpus)
  domain_times_encountered_per_domain = {}
  default_final_tasks = []
  for url in urls:
    domain = '.'.join(urlparse(url).netloc.split('.')[-2:])
    domain_times_encountered = domain_times_encountered_per_domain.get(
        domain, 0)
    output_subdirectory = '{}.{}'.format(domain, domain_times_encountered)
    domain_times_encountered_per_domain[domain] = domain_times_encountered + 1
    default_final_tasks.extend(
        _GenerateBenchmarkTasks(args, url, output_subdirectory))
  return task_manager.ExecuteWithCommandLine(args, default_final_tasks)


def main(command_line_args):
  logging.basicConfig(level=logging.INFO)
  devil_chromium.Initialize()

  args = _ArgumentParser().parse_args(command_line_args)
  OPTIONS.SetParsedArgs(args)

  if args.subcommand == 'record-test-trace':
    return _RecordWebServerTestTrace(args)
  if args.subcommand == 'run':
    return _RunAllMain(args)
  if args.subcommand == 'collect-csv':
    with args.output_csv as output_file:
      if not csv_util.CollectCSVsFromDirectory(args.output_dir, output_file):
        return 1
    return 0
  assert False


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
