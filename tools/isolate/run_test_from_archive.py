#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Reads a manifest, creates a tree of hardlinks and runs the test.

Keeps a local cache.
"""

import ctypes
import json
import logging
import optparse
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import time
import urllib


# Types of action accepted by recreate_tree().
HARDLINK, SYMLINK, COPY = range(4)[1:]


class MappingError(OSError):
  """Failed to recreate the tree."""
  pass


def os_link(source, link_name):
  """Add support for os.link() on Windows."""
  if sys.platform == 'win32':
    if not ctypes.windll.kernel32.CreateHardLinkW(
        unicode(link_name), unicode(source), 0):
      raise OSError()
  else:
    os.link(source, link_name)


def link_file(outfile, infile, action):
  """Links a file. The type of link depends on |action|."""
  logging.debug('Mapping %s to %s' % (infile, outfile))
  if action not in (HARDLINK, SYMLINK, COPY):
    raise ValueError('Unknown mapping action %s' % action)
  if os.path.isfile(outfile):
    raise MappingError('%s already exist' % outfile)

  if action == COPY:
    shutil.copy(infile, outfile)
  elif action == SYMLINK and sys.platform != 'win32':
    # On windows, symlink are converted to hardlink and fails over to copy.
    os.symlink(infile, outfile)
  else:
    try:
      os_link(infile, outfile)
    except OSError:
      # Probably a different file system.
      logging.warn(
          'Failed to hardlink, failing back to copy %s to %s' % (
            infile, outfile))
      shutil.copy(infile, outfile)


def _set_write_bit(path, read_only):
  """Sets or resets the executable bit on a file or directory."""
  mode = os.lstat(path).st_mode
  if read_only:
    mode = mode & 0500
  else:
    mode = mode | 0200
  if hasattr(os, 'lchmod'):
    os.lchmod(path, mode)  # pylint: disable=E1101
  else:
    if stat.S_ISLNK(mode):
      # Skip symlink without lchmod() support.
      logging.debug('Can\'t change +w bit on symlink %s' % path)
      return

    # TODO(maruel): Implement proper DACL modification on Windows.
    os.chmod(path, mode)


def make_writable(root, read_only):
  """Toggle the writable bit on a directory tree."""
  root = os.path.abspath(root)
  for dirpath, dirnames, filenames in os.walk(root, topdown=True):
    for filename in filenames:
      _set_write_bit(os.path.join(dirpath, filename), read_only)

    for dirname in dirnames:
      _set_write_bit(os.path.join(dirpath, dirname), read_only)


def rmtree(root):
  """Wrapper around shutil.rmtree() to retry automatically on Windows."""
  make_writable(root, False)
  if sys.platform == 'win32':
    for i in range(3):
      try:
        shutil.rmtree(root)
        break
      except WindowsError:  # pylint: disable=E0602
        delay = (i+1)*2
        print >> sys.stderr, (
            'The test has subprocess outliving it. Sleep %d seconds.' % delay)
        time.sleep(delay)
  else:
    shutil.rmtree(root)


def is_same_filesystem(path1, path2):
  """Returns True if both paths are on the same filesystem.

  This is required to enable the use of hardlinks.
  """
  assert os.path.isabs(path1), path1
  assert os.path.isabs(path2), path2
  if sys.platform == 'win32':
    # If the drive letter mismatches, assume it's a separate partition.
    # TODO(maruel): It should look at the underlying drive, a drive letter could
    # be a mount point to a directory on another drive.
    assert re.match(r'^[a-zA-Z]\:\\.*', path1), path1
    assert re.match(r'^[a-zA-Z]\:\\.*', path2), path2
    if path1[0].lower() != path2[0].lower():
      return False
  return os.stat(path1).st_dev == os.stat(path2).st_dev


def open_remote(file_or_url):
  """Reads a file or url."""
  if re.match(r'^https?://.+$', file_or_url):
    return urllib.urlopen(file_or_url)
  return open(file_or_url, 'rb')


def download_or_copy(file_or_url, dest):
  """Copies a file or download an url."""
  if re.match(r'^https?://.+$', file_or_url):
    try:
      urllib.URLopener().retrieve(file_or_url, dest)
    except IOError:
      logging.error('Failed to download ' + file_or_url)
  else:
    shutil.copy(file_or_url, dest)


def get_free_space(path):
  """Returns the number of free bytes."""
  if sys.platform == 'win32':
    free_bytes = ctypes.c_ulonglong(0)
    ctypes.windll.kernel32.GetDiskFreeSpaceExW(
        ctypes.c_wchar_p(path), None, None, ctypes.pointer(free_bytes))
    return free_bytes.value
  f = os.statvfs(path)
  return f.f_bfree * f.f_frsize


def fix_python_path(cmd):
  """Returns the fixed command line to call the right python executable."""
  out = cmd[:]
  if out[0] == 'python':
    out[0] = sys.executable
  elif out[0].endswith('.py'):
    out.insert(0, sys.executable)
  return out


class Cache(object):
  """Stateful LRU cache.

  Saves its state as json file.
  """
  STATE_FILE = 'state.json'

  def __init__(self, cache_dir, remote, max_cache_size, min_free_space):
    """
    Arguments:
    - cache_dir: Directory where to place the cache.
    - remote: Remote directory (NFS, SMB, etc) or HTTP url to fetch the objects
              from
    - max_cache_size: Trim if the cache gets larger than this value. If 0, the
                      cache is effectively a leak.
    - min_free_space: Trim if disk free space becomes lower than this value. If
                      0, it unconditionally fill the disk.
    """
    self.cache_dir = cache_dir
    self.remote = remote
    self.max_cache_size = max_cache_size
    self.min_free_space = min_free_space
    self.state_file = os.path.join(cache_dir, self.STATE_FILE)
    # The files are kept as an array in a LRU style. E.g. self.state[0] is the
    # oldest item.
    self.state = []

    if not os.path.isdir(self.cache_dir):
      os.makedirs(self.cache_dir)
    if os.path.isfile(self.state_file):
      try:
        self.state = json.load(open(self.state_file, 'r'))
      except ValueError:
        # Too bad. The file will be overwritten and the cache cleared.
        pass
    self.trim()

  def trim(self):
    """Trims anything we don't know, make sure enough free space exists."""
    # Ensure that all files listed in the state still exist.
    for filename in self.state[:]:
      if not os.path.exists(self.path(filename)):
        logging.info('Removing lost file %s' % filename)
        self.state.remove(filename)

    for filename in os.listdir(self.cache_dir):
      if filename == self.STATE_FILE or filename in self.state:
        continue
      logging.warn('Unknown file %s from cache' % filename)
      # Insert as the oldest file. It will be deleted eventually if not
      # accessed.
      self.state.insert(0, filename)

    # Ensure enough free space.
    while (
        self.min_free_space and
        self.state and
        get_free_space(self.cache_dir) < self.min_free_space):
      try:
        filename = self.state.pop(0)
        logging.info('Trimming %s' % filename)
        os.remove(self.path(filename))
      except OSError as e:
        logging.error('Error attempting to delete a file\n%s' % e)

    # Ensure maximum cache size.
    if self.max_cache_size and self.state:
      try:
        sizes = [os.stat(self.path(f)).st_size for f in self.state]
      except OSError:
        logging.error(
            'At least one file is missing; %s\n' % '\n'.join(self.state))
        raise

      while sizes and sum(sizes) > self.max_cache_size:
        # Delete the oldest item.
        try:
          filename = self.state.pop(0)
          logging.info('Trimming %s' % filename)
          os.remove(self.path(filename))
        except OSError as e:
          logging.error('Error attempting to delete a file\n%s' % e)
        sizes.pop(0)

    self.save()

  def retrieve(self, item):
    """Retrieves a file from the remote and add it to the cache."""
    assert not '/' in item
    try:
      index = self.state.index(item)
      # Was already in cache. Update it's LRU value.
      self.state.pop(index)
      self.state.append(item)
      return False
    except ValueError:
      out = self.path(item)
      download_or_copy(self.remote.rstrip('/') + '/' + item, out)
      if os.path.exists(out):
        self.state.append(item)
      else:
        logging.error('File, %s, not placed in cache' % item)
      return True
    finally:
      self.save()

  def path(self, item):
    """Returns the path to one item."""
    return os.path.join(self.cache_dir, item)

  def save(self):
    """Saves the LRU ordering."""
    json.dump(self.state, open(self.state_file, 'wb'))


class Profiler(object):
  def __init__(self, name):
    self.name = name
    self.start_time = None

  def __enter__(self):
    self.start_time = time.time()
    return self

  def __exit__(self, _exc_type, _exec_value, _traceback):
    time_taken = time.time() - self.start_time
    logging.debug('Profiling: Section %s took %i seconds',
                  self.name, time_taken)


def run_tha_test(manifest, cache_dir, remote, max_cache_size, min_free_space):
  """Downloads the dependencies in the cache, hardlinks them into a temporary
  directory and runs the executable.
  """
  cache = Cache(cache_dir, remote, max_cache_size, min_free_space)

  base_temp_dir = None
  if not is_same_filesystem(cache_dir, tempfile.gettempdir()):
    # Do not use tempdir since it's a separate filesystem than cache_dir. It
    # could be tmpfs for example. This would mean copying up to 100mb of data
    # there, when a simple tree of hardlinks would do.
    base_temp_dir = os.path.dirname(cache_dir)
  outdir = tempfile.mkdtemp(prefix='run_tha_test', dir=base_temp_dir)

  try:
    with Profiler('GetFiles') as _prof:
      for filepath, properties in manifest['files'].iteritems():
        outfile = os.path.join(outdir, filepath)
        outfiledir = os.path.dirname(outfile)
        if not os.path.isdir(outfiledir):
          os.makedirs(outfiledir)
        if 'sha-1' in properties:
          # A normal directory.
          infile = properties['sha-1']
          cache.retrieve(infile)
          link_file(outfile, cache.path(infile), HARDLINK)
        elif 'link' in properties:
          # A symlink.
          os.symlink(properties['link'], outfile)
        else:
          raise ValueError('Unexpected entry: %s' % properties)
        if 'mode' in properties:
          # It's not set on Windows.
          os.chmod(outfile, properties['mode'])

    cwd = os.path.join(outdir, manifest['relative_cwd'])
    if not os.path.isdir(cwd):
      os.makedirs(cwd)
    if manifest.get('read_only'):
      make_writable(outdir, True)
    cmd = manifest['command']
    # Ensure paths are correctly separated on windows.
    cmd[0] = cmd[0].replace('/', os.path.sep)
    cmd = fix_python_path(cmd)
    logging.info('Running %s, cwd=%s' % (cmd, cwd))
    try:
      with Profiler('RunTest') as _prof:
        return subprocess.call(cmd, cwd=cwd)
    except OSError:
      print >> sys.stderr, 'Failed to run %s; cwd=%s' % (cmd, cwd)
      raise
  finally:
    # Save first, in case an exception occur in the following lines, then clean
    # up.
    with Profiler('Cleanup') as _prof:
      cache.save()
      rmtree(outdir)
      cache.trim()


def main():
  parser = optparse.OptionParser(
      usage='%prog <options>', description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-v', '--verbose', action='count', default=1, help='Use multiple times')
  parser.add_option(
      '-m', '--manifest',
      metavar='FILE',
      help='File/url describing what to map or run')
  parser.add_option('--no-run', action='store_true', help='Skip the run part')
  parser.add_option(
      '--cache',
      default='cache',
      metavar='DIR',
      help='Cache directory, default=%default')
  parser.add_option(
      '-r', '--remote', metavar='URL', help='Remote where to get the items')
  parser.add_option(
      '--max-cache-size',
      type='int',
      metavar='NNN',
      default=20*1024*1024*1024,
      help='Trim if the cache gets larger than this value, default=%default')
  parser.add_option(
      '--min-free-space',
      type='int',
      metavar='NNN',
      default=1*1024*1024*1024,
      help='Trim if disk free space becomes lower than this value, '
           'default=%default')

  options, args = parser.parse_args()
  level = [logging.ERROR, logging.INFO, logging.DEBUG][min(2, options.verbose)]
  logging.basicConfig(
      level=level,
      format='%(levelname)5s %(module)15s(%(lineno)3d): %(message)s')

  if not options.manifest:
    parser.error('--manifest is required.')
  if not options.remote:
    parser.error('--remote is required.')
  if args:
    parser.error('Unsupported args %s' % ' '.join(args))

  manifest = json.load(open_remote(options.manifest))
  return run_tha_test(
      manifest, os.path.abspath(options.cache), options.remote,
      options.max_cache_size, options.min_free_space)


if __name__ == '__main__':
  sys.exit(main())
