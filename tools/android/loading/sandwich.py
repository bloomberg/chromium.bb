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
import json
import logging
import os
import shutil
import sys
import tempfile
import time

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
from pylib import constants
import devil_chromium

import chrome_cache
import chrome_setup
import device_setup
import devtools_monitor
import options
import page_track
import pull_sandwich_metrics
import trace_recorder
import tracing
import wpr_backend


# Use options layer to access constants.
OPTIONS = options.OPTIONS

_JOB_SEARCH_PATH = 'sandwich_jobs'

# An estimate of time to wait for the device to become idle after expensive
# operations, such as opening the launcher activity.
_TIME_TO_DEVICE_IDLE_SECONDS = 2


# Devtools timeout of 1 minute to avoid websocket timeout on slow
# network condition.
_DEVTOOLS_TIMEOUT = 60


def _ReadUrlsFromJobDescription(job_name):
  """Retrieves the list of URLs associated with the job name."""
  try:
    # Extra sugar: attempt to load from a relative path.
    json_file_name = os.path.join(os.path.dirname(__file__), _JOB_SEARCH_PATH,
        job_name)
    with open(json_file_name) as f:
      json_data = json.load(f)
  except IOError:
    # Attempt to read by regular file name.
    with open(job_name) as f:
      json_data = json.load(f)

  key = 'urls'
  if json_data and key in json_data:
    url_list = json_data[key]
    if isinstance(url_list, list) and len(url_list) > 0:
      return url_list
  raise Exception('Job description does not define a list named "urls"')


def _CleanPreviousTraces(output_directories_path):
  """Cleans previous traces from the output directory.

  Args:
    output_directories_path: The output directory path where to clean the
        previous traces.
  """
  for dirname in os.listdir(output_directories_path):
    directory_path = os.path.join(output_directories_path, dirname)
    if not os.path.isdir(directory_path):
      continue
    try:
      int(dirname)
    except ValueError:
      continue
    shutil.rmtree(directory_path)


class SandwichRunner(object):
  """Sandwich runner.

  This object is meant to be configured first and then run using the Run()
  method. The runner can configure itself conveniently with parsed arguement
  using the PullConfigFromArgs() method. The only job is to make sure that the
  command line flags have `dest` parameter set to existing runner members.
  """

  def __init__(self, job_name):
    """Configures a sandwich runner out of the box.

    Public members are meant to be configured as wished before calling Run().

    Args:
      job_name: The job name to get the associated urls.
    """
    # Cache operation to do before doing the chrome navigation.
    #   Can be: clear,save,push,reload
    self.cache_operation = 'clear'

    # The cache archive's path to save to or push from. Is str or None.
    self.cache_archive_path = None

    # Controls whether the WPR server should do script injection.
    self.disable_wpr_script_injection = False

    # The job name. Is str.
    self.job_name = job_name

    # Number of times to repeat the job.
    self.job_repeat = 1

    # Network conditions to emulate. None if no emulation.
    self.network_condition = None

    # Network condition emulator. Can be: browser,wpr
    self.network_emulator = 'browser'

    # Output directory where to save the traces. Is str or None.
    self.trace_output_directory = None

    # List of urls to run.
    self.urls = _ReadUrlsFromJobDescription(job_name)

    # Path to the WPR archive to load or save. Is str or None.
    self.wpr_archive_path = None

    # Configures whether the WPR archive should be read or generated.
    self.wpr_record = False

    self._device = None
    self._chrome_additional_flags = []
    self._local_cache_directory_path = None

  def PullConfigFromArgs(self, args):
    """Configures the sandwich runner from parsed command line argument.

    Args:
      args: The command line parsed argument.
    """
    for config_name in self.__dict__.keys():
      if config_name in args.__dict__:
        self.__dict__[config_name] = args.__dict__[config_name]

  def PrintConfig(self):
    """Print the current sandwich runner configuration to stdout. """
    for config_name in sorted(self.__dict__.keys()):
      if config_name[0] != '_':
        print '{} = {}'.format(config_name, self.__dict__[config_name])

  def _CleanTraceOutputDirectory(self):
    assert self.trace_output_directory
    if not os.path.isdir(self.trace_output_directory):
      try:
        os.makedirs(self.trace_output_directory)
      except OSError:
        logging.error('Cannot create directory for results: %s',
            self.trace_output_directory)
        raise
    else:
      _CleanPreviousTraces(self.trace_output_directory)

  def _SaveRunInfos(self, urls):
    assert self.trace_output_directory
    run_infos = {
      'cache-op': self.cache_operation,
      'job_name': self.job_name,
      'urls': urls
    }
    with open(os.path.join(self.trace_output_directory, 'run_infos.json'),
              'w') as file_output:
      json.dump(run_infos, file_output, indent=2)

  def _GetEmulatorNetworkCondition(self, emulator):
    if self.network_emulator == emulator:
      return self.network_condition
    return None

  def _RunNavigation(self, url, clear_cache, trace_id=None):
    with device_setup.DeviceConnection(
        device=self._device,
        additional_flags=self._chrome_additional_flags) as connection:
      additional_metadata = {}
      if self._GetEmulatorNetworkCondition('browser'):
        additional_metadata = chrome_setup.SetUpEmulationAndReturnMetadata(
            connection=connection,
            emulated_device_name=None,
            emulated_network_name=self._GetEmulatorNetworkCondition('browser'))
      loading_trace = trace_recorder.MonitorUrl(
          connection, url,
          clear_cache=clear_cache,
          categories=pull_sandwich_metrics.CATEGORIES,
          timeout=_DEVTOOLS_TIMEOUT)
      loading_trace.metadata.update(additional_metadata)
      if trace_id != None and self.trace_output_directory:
        loading_trace_path = os.path.join(
            self.trace_output_directory, str(trace_id), 'trace.json')
        os.makedirs(os.path.dirname(loading_trace_path))
        loading_trace.ToJsonFile(loading_trace_path)

  def _RunUrl(self, url, trace_id=0):
    clear_cache = False
    if self.cache_operation == 'clear':
      clear_cache = True
    elif self.cache_operation == 'push':
      self._device.KillAll(OPTIONS.chrome_package_name, quiet=True)
      chrome_cache.PushBrowserCache(self._device,
                                    self._local_cache_directory_path)
    elif self.cache_operation == 'reload':
      self._RunNavigation(url, clear_cache=True)
    elif self.cache_operation == 'save':
      clear_cache = trace_id == 0
    self._RunNavigation(url, clear_cache=clear_cache, trace_id=trace_id)

  def _PullCacheFromDevice(self):
    assert self.cache_operation == 'save'
    assert self.cache_archive_path, 'Need to specify where to save the cache'

    # Move Chrome to background to allow it to flush the index.
    self._device.adb.Shell('am start com.google.android.launcher')
    time.sleep(_TIME_TO_DEVICE_IDLE_SECONDS)
    self._device.KillAll(OPTIONS.chrome_package_name, quiet=True)
    time.sleep(_TIME_TO_DEVICE_IDLE_SECONDS)

    cache_directory_path = chrome_cache.PullBrowserCache(self._device)
    chrome_cache.ZipDirectoryContent(
        cache_directory_path, self.cache_archive_path)
    shutil.rmtree(cache_directory_path)

  def Run(self):
    """SandwichRunner main entry point meant to be called once configured.
    """
    if self.trace_output_directory:
      self._CleanTraceOutputDirectory()

    self._device = device_utils.DeviceUtils.HealthyDevices()[0]
    self._chrome_additional_flags = []

    assert self._local_cache_directory_path == None
    if self.cache_operation == 'push':
      assert os.path.isfile(self.cache_archive_path)
      self._local_cache_directory_path = tempfile.mkdtemp(suffix='.cache')
      chrome_cache.UnzipDirectoryContent(
          self.cache_archive_path, self._local_cache_directory_path)

    ran_urls = []
    with device_setup.WprHost(self._device, self.wpr_archive_path,
        record=self.wpr_record,
        network_condition_name=self._GetEmulatorNetworkCondition('wpr'),
        disable_script_injection=self.disable_wpr_script_injection
        ) as additional_flags:
      self._chrome_additional_flags.extend(additional_flags)
      for _ in xrange(self.job_repeat):
        for url in self.urls:
          self._RunUrl(url, trace_id=len(ran_urls))
          ran_urls.append(url)

    if self._local_cache_directory_path:
      shutil.rmtree(self._local_cache_directory_path)
      self._local_cache_directory_path = None
    if self.cache_operation == 'save':
      self._PullCacheFromDevice()
    if self.trace_output_directory:
      self._SaveRunInfos(ran_urls)


def _ArgumentParser():
  """Build a command line argument's parser."""
  # Command parser when dealing with jobs.
  common_job_parser = argparse.ArgumentParser(add_help=False)
  common_job_parser.add_argument('--job', required=True,
                                 help='JSON file with job description.')

  # Main parser
  parser = argparse.ArgumentParser()
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
      choices=sorted(chrome_setup.NETWORK_CONDITIONS.keys()),
      help='Set a network profile.')
  run_parser.add_argument('--network-emulator', default='browser',
      choices=['browser', 'wpr'],
      help='Set which component is emulating the network condition.' +
          ' (Default to browser). Wpr network emulator requires --wpr-archive' +
          ' to be set.')
  run_parser.add_argument('--job-repeat', default=1, type=int,
                          help='How many times to run the job.')
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


def _PatchWprMain(args):
  # Sets the resources cache max-age to 10 years.
  MAX_AGE = 10 * 365 * 24 * 60 * 60
  CACHE_CONTROL = 'public, max-age={}'.format(MAX_AGE)

  wpr_archive = wpr_backend.WprArchiveBackend(args.wpr_archive_path)
  for url_entry in wpr_archive.ListUrlEntries():
    response_headers = url_entry.GetResponseHeadersDict()
    if 'cache-control' in response_headers and \
        response_headers['cache-control'] == CACHE_CONTROL:
      continue
    logging.info('patching %s' % url_entry.url)
    # TODO(gabadie): may need to patch Last-Modified and If-Modified-Since.
    # TODO(gabadie): may need to delete ETag.
    # TODO(gabadie): may need to patch Vary.
    # TODO(gabadie): may need to take care of x-cache.
    #
    # Override the cache-control header to set the resources max age to MAX_AGE.
    #
    # Important note: Some resources holding sensitive information might have
    # cache-control set to no-store which allow the resource to be cached but
    # not cached in the file system. NoState-Prefetch is going to take care of
    # this case. But in here, to simulate NoState-Prefetch, we don't have other
    # choices but save absolutely all cached resources on disk so they survive
    # after killing chrome for cache save, modification and push.
    url_entry.SetResponseHeader('cache-control', CACHE_CONTROL)
  wpr_archive.Persist()
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
  trace_metrics_list = pull_sandwich_metrics.PullMetricsFromOutputDirectory(
      args.trace_output_directory)
  trace_metrics_list.sort(key=lambda e: e['id'])
  with open(args.metrics_csv_path, 'w') as csv_file:
    writer = csv.DictWriter(csv_file,
                            fieldnames=pull_sandwich_metrics.CSV_FIELD_NAMES)
    writer.writeheader()
    for trace_metrics in trace_metrics_list:
      writer.writerow(trace_metrics)
  return 0


def main(command_line_args):
  logging.basicConfig(level=logging.INFO)
  devil_chromium.Initialize()

  # Don't give the argument yet. All we are interested in for now is accessing
  # the default values of OPTIONS.
  OPTIONS.ParseArgs([])

  args = _ArgumentParser().parse_args(command_line_args)

  if args.subcommand == 'record-wpr':
    return _RecordWprMain(args)
  if args.subcommand == 'patch-wpr':
    return _PatchWprMain(args)
  if args.subcommand == 'create-cache':
    return _CreateCacheMain(args)
  if args.subcommand == 'run':
    return _RunJobMain(args)
  if args.subcommand == 'extract-metrics':
    return _ExtractMetricsMain(args)
  assert False


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
