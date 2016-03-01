# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Takes care of manipulating the chrome's HTTP cache.
"""

from datetime import datetime
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
from pylib import constants

import options


OPTIONS = options.OPTIONS


# Cache back-end types supported by cachetool.
BACKEND_TYPES = ['simple']

# Default build output directory.
OUT_DIRECTORY = os.getenv('CR_OUT_FULL', os.path.join(
    os.path.dirname(__file__), '../../../out/Release'))

# Default cachetool binary location.
CACHETOOL_BIN_PATH = os.path.join(OUT_DIRECTORY, 'cachetool')


def _RemoteCacheDirectory():
  """Returns the path of the cache directory's on the remote device."""
  return '/data/data/{}/cache/Cache'.format(
      constants.PACKAGE_INFO[OPTIONS.chrome_package_name].package)


def _UpdateTimestampFromAdbStat(filename, stat):
  os.utime(filename, (stat.st_time, stat.st_time))


def _AdbShell(adb, cmd):
  adb.Shell(subprocess.list2cmdline(cmd))


def _AdbUtime(adb, filename, timestamp):
  """Adb equivalent of os.utime(filename, (timestamp, timestamp))
  """
  touch_stamp = datetime.fromtimestamp(timestamp).strftime('%Y%m%d.%H%M%S')
  _AdbShell(adb, ['touch', '-t', touch_stamp, filename])


def PullBrowserCache(device):
  """Pulls the browser cache from the device and saves it locally.

  Cache is saved with the same file structure as on the device. Timestamps are
  important to preserve because indexing and eviction depends on them.

  Returns:
    Temporary directory containing all the browser cache.
  """
  _INDEX_DIRECTORY_NAME = 'index-dir'
  _REAL_INDEX_FILE_NAME = 'the-real-index'

  remote_cache_directory = _RemoteCacheDirectory()
  print remote_cache_directory
  save_target = tempfile.mkdtemp(suffix='.cache')
  for filename, stat in device.adb.Ls(remote_cache_directory):
    if filename == '..':
      continue
    if filename == '.':
      cache_directory_stat = stat
      continue
    original_file = os.path.join(remote_cache_directory, filename)
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


def PushBrowserCache(device, local_cache_path):
  """Pushes the browser cache saved locally to the device.

  Args:
    device: Android device.
    local_cache_path: The directory's path containing the cache locally.
  """
  remote_cache_directory = _RemoteCacheDirectory()

  # Clear previous cache.
  _AdbShell(device.adb, ['rm', '-rf', remote_cache_directory])
  _AdbShell(device.adb, ['mkdir', remote_cache_directory])

  # Push cache content.
  device.adb.Push(local_cache_path, remote_cache_directory)

  # Walk through the local cache to update mtime on the device.
  def MirrorMtime(local_path):
    cache_relative_path = os.path.relpath(local_path, start=local_cache_path)
    remote_path = os.path.join(remote_cache_directory, cache_relative_path)
    _AdbUtime(device.adb, remote_path, os.stat(local_path).st_mtime)

  for local_directory_path, dirnames, filenames in os.walk(
        local_cache_path, topdown=False):
    for filename in filenames:
      MirrorMtime(os.path.join(local_directory_path, filename))
    for dirname in dirnames:
      MirrorMtime(os.path.join(local_directory_path, dirname))
  MirrorMtime(local_cache_path)


def ZipDirectoryContent(root_directory_path, archive_dest_path):
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


def UnzipDirectoryContent(archive_path, directory_dest_path):
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


class CacheBackend(object):
  """Takes care of reading and deleting cached keys.
  """

  def __init__(self, cache_directory_path, cache_backend_type,
               cachetool_bin_path=CACHETOOL_BIN_PATH):
    """Chrome cache back-end constructor.

    Args:
      cache_directory_path: The directory path where the cache is locally
        stored.
      cache_backend_type: A cache back-end type in BACKEND_TYPES.
      cachetool_bin_path: Path of the cachetool binary.
    """
    assert os.path.isdir(cache_directory_path)
    assert cache_backend_type in BACKEND_TYPES
    assert os.path.isfile(cachetool_bin_path), 'invalid ' + cachetool_bin_path
    self._cache_directory_path = cache_directory_path
    self._cache_backend_type = cache_backend_type
    self._cachetool_bin_path = cachetool_bin_path
    # Make sure cache_directory_path is a valid cache.
    self._CachetoolCmd('validate')

  def ListKeys(self):
    """Lists cache's keys.

    Returns:
      A list of all keys stored in the cache.
    """
    return [k.strip() for k in self._CachetoolCmd('list_keys').split('\n')[:-1]]

  def GetStreamForKey(self, key, index):
    """Gets a key's stream.

    Args:
      key: The key to access the stream.
      index: The stream index:
          index=0 is the HTTP response header;
          index=1 is the transport encoded content;
          index=2 is the compiled content.

    Returns:
      String holding stream binary content.
    """
    return self._CachetoolCmd('get_stream', key, str(index))

  def DeleteKey(self, key):
    """Deletes a key from the cache.

    Args:
      key: The key delete.
    """
    self._CachetoolCmd('delete_key', key)

  def _CachetoolCmd(self, operation, *args):
    """Runs the cache editor tool and return the stdout.

    Args:
      operation: Cachetool operation.
      *args: Additional operation argument to append to the command line.

    Returns:
      Cachetool's stdout string.
    """
    editor_tool_cmd = [
        self._cachetool_bin_path,
        self._cache_directory_path,
        self._cache_backend_type,
        operation]
    editor_tool_cmd.extend(args)
    process = subprocess.Popen(editor_tool_cmd, stdout=subprocess.PIPE)
    stdout_data, _ = process.communicate()
    assert process.returncode == 0
    return stdout_data


def ApplyUrlWhitelistToCacheArchive(cache_archive_path,
                                    whitelisted_urls,
                                    output_cache_archive_path):
  """Generate a new cache archive containing only whitelisted urls.

  Args:
    cache_archive_path: Path of the cache archive to apply the white listing.
    whitelisted_urls: Set of url to keep in cache.
    output_cache_archive_path: Destination path of cache archive containing only
      white-listed urls.
  """
  cache_temp_directory = tempfile.mkdtemp(suffix='.cache')
  try:
    UnzipDirectoryContent(cache_archive_path, cache_temp_directory)
    backend = CacheBackend(cache_temp_directory, 'simple')
    cached_urls = backend.ListKeys()
    for cached_url in cached_urls:
      if cached_url not in whitelisted_urls:
        backend.DeleteKey(cached_url)
    for cached_url in backend.ListKeys():
      assert cached_url in whitelisted_urls
    ZipDirectoryContent(cache_temp_directory, output_cache_archive_path)
  finally:
    shutil.rmtree(cache_temp_directory)


if __name__ == '__main__':
  import argparse
  parser = argparse.ArgumentParser(description='Tests cache back-end.')
  parser.add_argument('cache_path', type=str)
  parser.add_argument('backend_type', type=str, choices=BACKEND_TYPES)
  command_line_args = parser.parse_args()

  cache_backend = CacheBackend(
      cache_directory_path=command_line_args.cache_path,
      cache_backend_type=command_line_args.backend_type)
  keys = cache_backend.ListKeys()
  print '{}\'s HTTP response header:'.format(keys[0])
  print cache_backend.GetStreamForKey(keys[0], 0)
  cache_backend.DeleteKey(keys[1])
  assert keys[1] not in cache_backend.ListKeys()
