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
import logging
import os
import sys
import time

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
from pylib import constants
import devil_chromium

import device_setup
import devtools_monitor
import json
import page_track
import tracing


_JOB_SEARCH_PATH = 'sandwich_jobs'

# Directory name under --output to save the cache from the device.
_CACHE_DIRECTORY_NAME = 'cache'

# Name of cache subdirectory on the device where the cache index is stored.
_INDEX_DIRECTORY_NAME = 'index-dir'

# Name of the file containing the cache index. This file is stored on the device
# in the cache directory under _INDEX_DIRECTORY_NAME.
_REAL_INDEX_FILE_NAME = 'the-real-index'

# Name of the chrome package.
_CHROME_PACKAGE = (
    constants.PACKAGE_INFO[device_setup.DEFAULT_CHROME_PACKAGE].package)

# An estimate of time to wait for the device to become idle after expensive
# operations, such as opening the launcher activity.
_TIME_TO_DEVICE_IDLE_SECONDS = 2


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


def _SaveChromeTrace(events, directory, subdirectory):
  """Saves the trace events, ignores IO errors.

  Args:
    events: a dict as returned by TracingTrack.ToJsonDict()
    directory: directory name contining all traces
    subdirectory: directory name to create this particular trace in
  """
  target_directory = os.path.join(directory, subdirectory)
  filename = os.path.join(target_directory, 'trace.json')
  try:
    os.makedirs(target_directory)
    with open(filename, 'w') as f:
      json.dump({'traceEvents': events['events'], 'metadata': {}}, f)
  except IOError:
    logging.warning('Could not save a trace: %s' % filename)
    # Swallow the exception.


def _UpdateTimestampFromAdbStat(filename, stat):
  os.utime(filename, (stat.st_time, stat.st_time))


def _SaveBrowserCache(device, output_directory):
  """Pulls the browser cache from the device and saves it locally.

  Cache is saved with the same file structure as on the device. Timestamps are
  important to preserve because indexing and eviction depends on them.

  Args:
    output_directory: name of the directory for saving cache.
  """
  save_target = os.path.join(output_directory, _CACHE_DIRECTORY_NAME)
  try:
    os.makedirs(save_target)
  except IOError:
    logging.warning('Could not create directory: %s' % save_target)
    raise

  cache_directory = '/data/data/' + _CHROME_PACKAGE + '/cache/Cache'
  for filename, stat in device.adb.Ls(cache_directory):
    if filename == '..':
      continue
    if filename == '.':
      cache_directory_stat = stat
      continue
    original_file = os.path.join(cache_directory, filename)
    saved_file = os.path.join(save_target, filename)
    device.adb.Pull(original_file, saved_file)
    _UpdateTimestampFromAdbStat(saved_file, stat)
    if filename == _INDEX_DIRECTORY_NAME:
      # The directory containing the index was pulled recursively, update the
      # timestamps for known files. They are ignored by cache backend, but may
      # be useful for debugging.
      index_dir_stat = stat
      saved_index_dir = os.path.join(save_target, _INDEX_DIRECTORY_NAME)
      saved_index_file = os.path.join(saved_index_dir, _REAL_INDEX_FILE_NAME)
      for sub_file, sub_stat in device.adb.Ls(original_file):
        if sub_file == _REAL_INDEX_FILE_NAME:
          _UpdateTimestampFromAdbStat(saved_index_file, sub_stat)
          break
      _UpdateTimestampFromAdbStat(saved_index_dir, index_dir_stat)

  # Store the cache directory modification time. It is important to update it
  # after all files in it have been written. The timestamp is compared with
  # the contents of the index file when freshness is determined.
  _UpdateTimestampFromAdbStat(save_target, cache_directory_stat)


def main():
  logging.basicConfig(level=logging.INFO)
  devil_chromium.Initialize()

  parser = argparse.ArgumentParser()
  parser.add_argument('--job', required=True,
                      help='JSON file with job description.')
  parser.add_argument('--output', required=True,
                      help='Name of output directory to create.')
  parser.add_argument('--repeat', default=1, type=int,
                      help='How many times to run the job')
  parser.add_argument('--save-cache', default=False,
                      action='store_true',
                      help='Clear HTTP cache before start,' +
                      'save cache before exit.')
  parser.add_argument('--wpr-archive', default=None, type=str,
                      help='Web page replay archive to load job\'s urls from.')
  parser.add_argument('--wpr-record', default=False, action='store_true',
                      help='Record web page replay archive.')
  args = parser.parse_args()

  try:
    os.makedirs(args.output)
  except OSError:
    logging.error('Cannot create directory for results: %s' % args.output)
    raise

  job_urls = _ReadUrlsFromJobDescription(args.job)
  device = device_utils.DeviceUtils.HealthyDevices()[0]

  with device_setup.WprHost(device,
                            args.wpr_archive,
                            args.wpr_record) as additional_flags:
    pages_loaded = 0
    for iteration in xrange(args.repeat):
      for url in job_urls:
        with device_setup.DeviceConnection(
            device=device,
            additional_flags=additional_flags) as connection:
          if iteration == 0 and pages_loaded == 0 and args.save_cache:
            connection.ClearCache()
          page_track.PageTrack(connection)
          tracing_track = tracing.TracingTrack(connection,
              categories='blink,cc,netlog,renderer.scheduler,toplevel,v8')
          connection.SetUpMonitoring()
          connection.SendAndIgnoreResponse('Page.navigate', {'url': url})
          connection.StartMonitoring()
          pages_loaded += 1
          _SaveChromeTrace(tracing_track.ToJsonDict(), args.output,
              str(pages_loaded))

  if args.save_cache:
    # Move Chrome to background to allow it to flush the index.
    device.adb.Shell('am start com.google.android.launcher')
    time.sleep(_TIME_TO_DEVICE_IDLE_SECONDS)
    device.KillAll(_CHROME_PACKAGE, quiet=True)
    time.sleep(_TIME_TO_DEVICE_IDLE_SECONDS)
    _SaveBrowserCache(device, args.output)


if __name__ == '__main__':
  sys.exit(main())
