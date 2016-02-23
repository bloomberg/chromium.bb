# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Takes care of manipulating the chrome's HTTP cache.
"""

import os
import subprocess


# Cache back-end types supported by cachetool.
BACKEND_TYPES = ['simple']

# Default build output directory.
OUT_DIRECTORY = os.getenv('CR_OUT_FULL', os.path.join(
    os.path.dirname(__file__), '../../../out/Release'))

# Default cachetool binary location.
CACHETOOL_BIN_PATH = os.path.join(OUT_DIRECTORY, 'cachetool')


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
