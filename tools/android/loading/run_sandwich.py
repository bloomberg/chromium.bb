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
from datetime import datetime
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
from pylib import constants
import devil_chromium

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

# Directory name under --output to save the cache from the device.
_CACHE_DIRECTORY_NAME = 'cache'

# Name of cache subdirectory on the device where the cache index is stored.
_INDEX_DIRECTORY_NAME = 'index-dir'

# Name of the file containing the cache index. This file is stored on the device
# in the cache directory under _INDEX_DIRECTORY_NAME.
_REAL_INDEX_FILE_NAME = 'the-real-index'

# An estimate of time to wait for the device to become idle after expensive
# operations, such as opening the launcher activity.
_TIME_TO_DEVICE_IDLE_SECONDS = 2


def _RemoteCacheDirectory():
  return '/data/data/{}/cache/Cache'.format(OPTIONS.chrome_package_name)

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


def _UpdateTimestampFromAdbStat(filename, stat):
  os.utime(filename, (stat.st_time, stat.st_time))


def _AdbShell(adb, cmd):
  adb.Shell(subprocess.list2cmdline(cmd))


def _AdbUtime(adb, filename, timestamp):
  """Adb equivalent of os.utime(filename, (timestamp, timestamp))
  """
  touch_stamp = datetime.fromtimestamp(timestamp).strftime('%Y%m%d.%H%M%S')
  _AdbShell(adb, ['touch', '-t', touch_stamp, filename])


def _PullBrowserCache(device):
  """Pulls the browser cache from the device and saves it locally.

  Cache is saved with the same file structure as on the device. Timestamps are
  important to preserve because indexing and eviction depends on them.

  Returns:
    Temporary directory containing all the browser cache.
  """
  save_target = tempfile.mkdtemp(suffix='.cache')
  for filename, stat in device.adb.Ls(_RemoteCacheDirectory()):
    if filename == '..':
      continue
    if filename == '.':
      cache_directory_stat = stat
      continue
    original_file = os.path.join(_RemoteCacheDirectory(), filename)
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
  return save_target


def _PushBrowserCache(device, local_cache_path):
  """Pushes the browser cache saved locally to the device.

  Args:
    device: Android device.
    local_cache_path: The directory's path containing the cache locally.
  """
  # Clear previous cache.
  _AdbShell(device.adb, ['rm', '-rf', _RemoteCacheDirectory()])
  _AdbShell(device.adb, ['mkdir', _RemoteCacheDirectory()])

  # Push cache content.
  device.adb.Push(local_cache_path, _RemoteCacheDirectory())

  # Walk through the local cache to update mtime on the device.
  def MirrorMtime(local_path):
    cache_relative_path = os.path.relpath(local_path, start=local_cache_path)
    remote_path = os.path.join(_RemoteCacheDirectory(), cache_relative_path)
    _AdbUtime(device.adb, remote_path, os.stat(local_path).st_mtime)

  for local_directory_path, dirnames, filenames in os.walk(
        local_cache_path, topdown=False):
    for filename in filenames:
      MirrorMtime(os.path.join(local_directory_path, filename))
    for dirname in dirnames:
      MirrorMtime(os.path.join(local_directory_path, dirname))
  MirrorMtime(local_cache_path)


def _ZipDirectoryContent(root_directory_path, archive_dest_path):
  """Zip a directory's content recursively with all the directories'
  timestamps preserved.

  Args:
    root_directory_path: The directory's path to archive.
    archive_dest_path: Archive destination's path.
  """
  with zipfile.ZipFile(archive_dest_path, 'w') as zip_output:
    timestamps = {}
    root_directory_stats = os.stat(root_directory_path)
    timestamps['.'] = {
        'atime': root_directory_stats.st_atime,
        'mtime': root_directory_stats.st_mtime}
    for directory_path, dirnames, filenames in os.walk(root_directory_path):
      for dirname in dirnames:
        subdirectory_path = os.path.join(directory_path, dirname)
        subdirectory_relative_path = os.path.relpath(subdirectory_path,
                                                     root_directory_path)
        subdirectory_stats = os.stat(subdirectory_path)
        timestamps[subdirectory_relative_path] = {
            'atime': subdirectory_stats.st_atime,
            'mtime': subdirectory_stats.st_mtime}
      for filename in filenames:
        file_path = os.path.join(directory_path, filename)
        file_archive_name = os.path.join('content',
            os.path.relpath(file_path, root_directory_path))
        file_stats = os.stat(file_path)
        timestamps[file_archive_name[8:]] = {
            'atime': file_stats.st_atime,
            'mtime': file_stats.st_mtime}
        zip_output.write(file_path, arcname=file_archive_name)
    zip_output.writestr('timestamps.json',
                        json.dumps(timestamps, indent=2))


def _UnzipDirectoryContent(archive_path, directory_dest_path):
  """Unzip a directory's content recursively with all the directories'
  timestamps preserved.

  Args:
    archive_path: Archive's path to unzip.
    directory_dest_path: Directory destination path.
  """
  if not os.path.exists(directory_dest_path):
    os.makedirs(directory_dest_path)

  with zipfile.ZipFile(archive_path) as zip_input:
    timestamps = None
    for file_archive_name in zip_input.namelist():
      if file_archive_name == 'timestamps.json':
        timestamps = json.loads(zip_input.read(file_archive_name))
      elif file_archive_name.startswith('content/'):
        file_relative_path = file_archive_name[8:]
        file_output_path = os.path.join(directory_dest_path, file_relative_path)
        file_parent_directory_path = os.path.dirname(file_output_path)
        if not os.path.exists(file_parent_directory_path):
          os.makedirs(file_parent_directory_path)
        with open(file_output_path, 'w') as f:
          f.write(zip_input.read(file_archive_name))

    assert timestamps
    for relative_path, stats in timestamps.iteritems():
      output_path = os.path.join(directory_dest_path, relative_path)
      if not os.path.exists(output_path):
        os.makedirs(output_path)
      os.utime(output_path, (stats['atime'], stats['mtime']))


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
    _UnzipDirectoryContent(local_cache_archive_path, local_cache_directory_path)

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
          _PushBrowserCache(device, local_cache_directory_path)
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

    cache_directory_path = _PullBrowserCache(device)
    _ZipDirectoryContent(cache_directory_path, local_cache_archive_path)
    shutil.rmtree(cache_directory_path)

  with open(os.path.join(args.output, 'run_infos.json'), 'w') as file_output:
    json.dump(run_infos, file_output, indent=2)


if __name__ == '__main__':
  sys.exit(main())
