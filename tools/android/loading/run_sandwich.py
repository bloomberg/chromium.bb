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


def _ArgumentParser():
  """Build a command line argument's parser.
  """
  parser = argparse.ArgumentParser()
  parser.add_argument('--job', required=True,
                      help='JSON file with job description.')
  parser.add_argument('--output', required=True,
                      help='Name of output directory to create.')
  parser.add_argument('--repeat', default=1, type=int,
                      help='How many times to run the job')
  parser.add_argument('--cache-op',
                      choices=['clear', 'save', 'push', 'reload'],
                      default='clear',
                      help='Configures cache operation to do before launching '
                          +'Chrome. (Default is clear).')
  parser.add_argument('--wpr-archive', default=None, type=str,
                      help='Web page replay archive to load job\'s urls from.')
  parser.add_argument('--wpr-record', default=False, action='store_true',
                      help='Record web page replay archive.')
  parser.add_argument('--disable-wpr-script-injection', default=False,
                      action='store_true',
                      help='Disable WPR default script injection such as ' +
                          'overriding javascript\'s Math.random() and Date() ' +
                          'with deterministic implementations.')
  parser.add_argument('--network-condition', default=None,
      choices=sorted(chrome_setup.NETWORK_CONDITIONS.keys()),
      help='Set a network profile.')
  parser.add_argument('--network-emulator', default='browser',
      choices=['browser', 'wpr'],
      help='Set which component is emulating the network condition.' +
          ' (Default to browser)')
  return parser


def main():
  logging.basicConfig(level=logging.INFO)
  devil_chromium.Initialize()

  # Don't give the argument yet. All we are interested in for now is accessing
  # the default values of OPTIONS.
  OPTIONS.ParseArgs([])

  args = _ArgumentParser().parse_args()

  if not os.path.isdir(args.output):
    try:
      os.makedirs(args.output)
    except OSError:
      logging.error('Cannot create directory for results: %s' % args.output)
      raise
  else:
    _CleanPreviousTraces(args.output)

  run_infos = {
    'cache-op': args.cache_op,
    'job': args.job,
    'urls': []
  }
  job_urls = _ReadUrlsFromJobDescription(args.job)
  device = device_utils.DeviceUtils.HealthyDevices()[0]
  local_cache_archive_path = os.path.join(args.output, 'cache.zip')
  local_cache_directory_path = None
  wpr_network_condition_name = None
  browser_network_condition_name = None
  if args.network_emulator == 'wpr':
    wpr_network_condition_name = args.network_condition
  elif args.network_emulator == 'browser':
    browser_network_condition_name = args.network_condition
  else:
    assert False

  if args.cache_op == 'push':
    assert os.path.isfile(local_cache_archive_path)
    local_cache_directory_path = tempfile.mkdtemp(suffix='.cache')
    chrome_cache.UnzipDirectoryContent(
        local_cache_archive_path, local_cache_directory_path)

  with device_setup.WprHost(device, args.wpr_archive,
      record=args.wpr_record,
      network_condition_name=wpr_network_condition_name,
      disable_script_injection=args.disable_wpr_script_injection
      ) as additional_flags:
    def _RunNavigation(url, clear_cache, trace_id):
      with device_setup.DeviceConnection(
          device=device,
          additional_flags=additional_flags) as connection:
        additional_metadata = {}
        if browser_network_condition_name:
          additional_metadata = chrome_setup.SetUpEmulationAndReturnMetadata(
              connection=connection,
              emulated_device_name=None,
              emulated_network_name=browser_network_condition_name)
        loading_trace = trace_recorder.MonitorUrl(
            connection, url,
            clear_cache=clear_cache,
            categories=pull_sandwich_metrics.CATEGORIES,
            timeout=_DEVTOOLS_TIMEOUT)
        loading_trace.metadata.update(additional_metadata)
        if trace_id != None:
          loading_trace_path = os.path.join(
              args.output, str(trace_id), 'trace.json')
          os.makedirs(os.path.dirname(loading_trace_path))
          loading_trace.ToJsonFile(loading_trace_path)

    for _ in xrange(args.repeat):
      for url in job_urls:
        clear_cache = False
        if args.cache_op == 'clear':
          clear_cache = True
        elif args.cache_op == 'push':
          device.KillAll(OPTIONS.chrome_package_name, quiet=True)
          chrome_cache.PushBrowserCache(device, local_cache_directory_path)
        elif args.cache_op == 'reload':
          _RunNavigation(url, clear_cache=True, trace_id=None)
        elif args.cache_op == 'save':
          clear_cache = not run_infos['urls']
        _RunNavigation(url, clear_cache=clear_cache,
                       trace_id=len(run_infos['urls']))
        run_infos['urls'].append(url)

  if local_cache_directory_path:
    shutil.rmtree(local_cache_directory_path)

  if args.cache_op == 'save':
    # Move Chrome to background to allow it to flush the index.
    device.adb.Shell('am start com.google.android.launcher')
    time.sleep(_TIME_TO_DEVICE_IDLE_SECONDS)
    device.KillAll(OPTIONS.chrome_package_name, quiet=True)
    time.sleep(_TIME_TO_DEVICE_IDLE_SECONDS)

    cache_directory_path = chrome_cache.PullBrowserCache(device)
    chrome_cache.ZipDirectoryContent(
        cache_directory_path, local_cache_archive_path)
    shutil.rmtree(cache_directory_path)

  with open(os.path.join(args.output, 'run_infos.json'), 'w') as file_output:
    json.dump(run_infos, file_output, indent=2)


if __name__ == '__main__':
  sys.exit(main())
