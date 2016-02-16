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

import device_setup
import devtools_monitor
import json
import page_track
import pull_sandwich_metrics
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

# Cache directory's path on the device.
_REMOTE_CACHE_DIRECTORY = '/data/data/' + _CHROME_PACKAGE + '/cache/Cache'


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
      json.dump({'traceEvents': events['events'], 'metadata': {}}, f, indent=2)
  except IOError:
    logging.warning('Could not save a trace: %s' % filename)
    # Swallow the exception.


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
  for filename, stat in device.adb.Ls(_REMOTE_CACHE_DIRECTORY):
    if filename == '..':
      continue
    if filename == '.':
      cache_directory_stat = stat
      continue
    original_file = os.path.join(_REMOTE_CACHE_DIRECTORY, filename)
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
  _AdbShell(device.adb, ['rm', '-rf', _REMOTE_CACHE_DIRECTORY])
  _AdbShell(device.adb, ['mkdir', _REMOTE_CACHE_DIRECTORY])

  # Push cache content.
  device.adb.Push(local_cache_path, _REMOTE_CACHE_DIRECTORY)

  # Walk through the local cache to update mtime on the device.
  def MirrorMtime(local_path):
    cache_relative_path = os.path.relpath(local_path, start=local_cache_path)
    remote_path = os.path.join(_REMOTE_CACHE_DIRECTORY, cache_relative_path)
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
  parser.add_argument('--cache-op',
                      choices=['clear', 'save', 'push'],
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
  args = parser.parse_args()

  if not os.path.isdir(args.output):
    try:
      os.makedirs(args.output)
    except OSError:
      logging.error('Cannot create directory for results: %s' % args.output)
      raise
  else:
    _CleanPreviousTraces(args.output)

  job_urls = _ReadUrlsFromJobDescription(args.job)
  device = device_utils.DeviceUtils.HealthyDevices()[0]
  local_cache_archive_path = os.path.join(args.output, 'cache.zip')
  local_cache_directory_path = None

  if args.cache_op == 'push':
    assert os.path.isfile(local_cache_archive_path)
    local_cache_directory_path = tempfile.mkdtemp(suffix='.cache')
    _UnzipDirectoryContent(local_cache_archive_path, local_cache_directory_path)

  with device_setup.WprHost(device, args.wpr_archive, args.wpr_record,
      args.disable_wpr_script_injection) as additional_flags:
    pages_loaded = 0
    for _ in xrange(args.repeat):
      for url in job_urls:
        if args.cache_op == 'push':
          device.KillAll(_CHROME_PACKAGE, quiet=True)
          _PushBrowserCache(device, local_cache_directory_path)
        with device_setup.DeviceConnection(
            device=device,
            additional_flags=additional_flags) as connection:
          if (pages_loaded == 0 and args.cache_op == 'save' or
              args.cache_op == 'clear'):
            connection.ClearCache()
          page_track.PageTrack(connection)
          tracing_track = tracing.TracingTrack(connection,
              categories=pull_sandwich_metrics.CATEGORIES)
          connection.SetUpMonitoring()
          connection.SendAndIgnoreResponse('Page.navigate', {'url': url})
          connection.StartMonitoring()
          pages_loaded += 1
          _SaveChromeTrace(tracing_track.ToJsonDict(), args.output,
              str(pages_loaded))

  if local_cache_directory_path:
    shutil.rmtree(local_cache_directory_path)

  if args.cache_op == 'save':
    # Move Chrome to background to allow it to flush the index.
    device.adb.Shell('am start com.google.android.launcher')
    time.sleep(_TIME_TO_DEVICE_IDLE_SECONDS)
    device.KillAll(_CHROME_PACKAGE, quiet=True)
    time.sleep(_TIME_TO_DEVICE_IDLE_SECONDS)

    cache_directory_path = _PullBrowserCache(device)
    _ZipDirectoryContent(cache_directory_path, local_cache_archive_path)
    shutil.rmtree(cache_directory_path)


if __name__ == '__main__':
  sys.exit(main())
