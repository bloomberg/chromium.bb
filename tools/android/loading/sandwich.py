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
import sys
from urlparse import urlparse

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
from pylib import constants
import devil_chromium

import csv_util
import device_setup
import options
import sandwich_prefetch
import sandwich_swr
import sandwich_utils
import task_manager


# Use options layer to access constants.
OPTIONS = options.OPTIONS

_SPEED_INDEX_MEASUREMENT = 'speed-index'
_MEMORY_MEASUREMENT = 'memory'
_TTFMP_MEASUREMENT = 'ttfmp'
_CORPUS_DIR = 'sandwich_corpuses'

_MAIN_TRANSFORMER_LIST_NAME = 'no-network-emulation'


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


def _GenerateUrlDirectoryNames(urls):
  domain_times_encountered_per_domain = {}
  for url in urls:
    domain = '.'.join(urlparse(url).netloc.split('.')[-2:])
    domain_times_encountered = domain_times_encountered_per_domain.get(
        domain, 0)
    output_subdirectory = '{}.{}'.format(domain, domain_times_encountered)
    domain_times_encountered_per_domain[domain] = domain_times_encountered + 1
    yield url, output_subdirectory


def _ArgumentParser():
  """Build a command line argument's parser."""
  task_parser = task_manager.CommandLineParser()

  # Command parser when dealing with _RunBenchmarkMain.
  sandwich_runner_parser = argparse.ArgumentParser(
      add_help=False, parents=[task_parser])
  sandwich_runner_parser.add_argument('--android', default=None, type=str,
                                      dest='android_device_serial',
                                      help='Android device\'s serial to use.')
  sandwich_runner_parser.add_argument('-c', '--corpus', required=True,
      help='Path to a JSON file with a corpus such as in %s/.' % _CORPUS_DIR)
  sandwich_runner_parser.add_argument('-m', '--measure', default=[], nargs='+',
      choices=[_SPEED_INDEX_MEASUREMENT,
               _MEMORY_MEASUREMENT,
               _TTFMP_MEASUREMENT],
      dest='optional_measures', help='Enable optional measurements.')
  sandwich_runner_parser.add_argument('--wpr-archive', default=None, type=str,
      dest='wpr_archive_path',
      help='WebPageReplay archive to use, instead of generating one.')
  sandwich_runner_parser.add_argument('-r', '--url-repeat', default=1, type=int,
      help='How many times to repeat the urls.')

  # Plumbing parser to configure OPTIONS.
  plumbing_parser = OPTIONS.GetParentParser('plumbing options')

  # Main parser
  parser = argparse.ArgumentParser(parents=[plumbing_parser],
      fromfile_prefix_chars=task_manager.FROMFILE_PREFIX_CHARS)
  subparsers = parser.add_subparsers(dest='subcommand', help='subcommand line')

  # Run NoState-Prefetch benchmarks subcommand.
  run_parser = subparsers.add_parser('run',
      parents=[sandwich_runner_parser],
      help='Run all NoState-Prefetch benchmarks steps using the task '
           'manager infrastructure.')
  run_parser.add_argument('-g', '--gen-full', action='store_true',
      help='Generate the full graph with all possible benchmarks.')

  # Run Stale-While-Revalidate benchmarks subcommand.
  subparsers.add_parser('run-swr', parents=[sandwich_runner_parser],
      help='Run all Stale-While-Revalidate benchmarks steps using the task '
           'manager infrastructure.')

  # Collect subcommand.
  collect_csv_parser = subparsers.add_parser('collect-csv',
      help='Collects all CSVs from Sandwich output directory into a single '
           'CSV.')
  collect_csv_parser.add_argument('output_dir', type=str,
                                  help='Path to the run output directory.')
  collect_csv_parser.add_argument('output_csv', type=argparse.FileType('w'),
                                  help='Path to the output CSV.')

  return parser


def _GenerateNoStatePrefetchBenchmarkTasks(
    args, common_builder, main_transformer):
  builder = sandwich_prefetch.PrefetchBenchmarkBuilder(common_builder)
  builder.PopulateLoadBenchmark(sandwich_prefetch.EMPTY_CACHE_DISCOVERER,
                                _MAIN_TRANSFORMER_LIST_NAME,
                                transformer_list=[main_transformer])
  builder.PopulateLoadBenchmark(sandwich_prefetch.FULL_CACHE_DISCOVERER,
                                _MAIN_TRANSFORMER_LIST_NAME,
                                transformer_list=[main_transformer])
  if not args.gen_full:
    return
  for network_condition in ['Regular4G', 'Regular3G', 'Regular2G']:
    transformer_list_name = network_condition.lower()
    network_transformer = \
        sandwich_utils.NetworkSimulationTransformer(network_condition)
    transformer_list = [main_transformer, network_transformer]
    for subresource_discoverer in sandwich_prefetch.SUBRESOURCE_DISCOVERERS:
      if subresource_discoverer == sandwich_prefetch.FULL_CACHE_DISCOVERER:
        continue
      builder.PopulateLoadBenchmark(
          subresource_discoverer, transformer_list_name, transformer_list)


def _GenerateStaleWhileRevalidateBenchmarkTasks(
    args, common_builder, main_transformer):
  del args # unused.
  builder = sandwich_swr.StaleWhileRevalidateBenchmarkBuilder(common_builder)
  for enable_swr in [False, True]:
    builder.PopulateBenchmark(enable_swr, _MAIN_TRANSFORMER_LIST_NAME,
                              transformer_list=[main_transformer])
    for network_condition in ['Regular3G', 'Regular2G']:
      transformer_list_name = network_condition.lower()
      network_transformer = \
          sandwich_utils.NetworkSimulationTransformer(network_condition)
      transformer_list = [main_transformer, network_transformer]
      builder.PopulateBenchmark(
          enable_swr, transformer_list_name, transformer_list)


def _RunBenchmarkMain(args, task_generator):
  urls = ReadUrlsFromCorpus(args.corpus)
  android_device = None
  if args.android_device_serial:
    android_device = device_setup.GetDeviceFromSerial(
        args.android_device_serial)

  def MainTransformer(runner):
    runner.record_video = _SPEED_INDEX_MEASUREMENT in args.optional_measures
    runner.record_memory_dumps = _MEMORY_MEASUREMENT in args.optional_measures
    runner.record_first_meaningful_paint = (
        _TTFMP_MEASUREMENT in args.optional_measures)
    runner.repeat = args.url_repeat

  default_final_tasks = []
  for url, output_subdirectory in _GenerateUrlDirectoryNames(urls):
    common_builder = sandwich_utils.SandwichCommonBuilder(
        android_device=android_device,
        url=url,
        output_directory=args.output,
        output_subdirectory=output_subdirectory)
    if args.wpr_archive_path:
      common_builder.OverridePathToWprArchive(args.wpr_archive_path)
    else:
      common_builder.PopulateWprRecordingTask()
    task_generator(args, common_builder, MainTransformer)
    default_final_tasks.extend(common_builder.default_final_tasks)
  return task_manager.ExecuteWithCommandLine(args, default_final_tasks)


def main(command_line_args):
  logging.basicConfig(level=logging.INFO)
  devil_chromium.Initialize()

  args = _ArgumentParser().parse_args(command_line_args)
  OPTIONS.SetParsedArgs(args)

  if args.subcommand == 'run':
    return _RunBenchmarkMain(args, _GenerateNoStatePrefetchBenchmarkTasks)
  if args.subcommand == 'run-swr':
    return _RunBenchmarkMain(args, _GenerateStaleWhileRevalidateBenchmarkTasks)
  if args.subcommand == 'collect-csv':
    with args.output_csv as output_file:
      if not csv_util.CollectCSVsFromDirectory(args.output_dir, output_file):
        return 1
    return 0
  assert False


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
