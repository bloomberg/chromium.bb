#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Reads a .isolated, creates a tree of hardlinks and runs the test.

Keeps a local cache.
"""

__version__ = '0.2'

import ctypes
import hashlib
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

from third_party.depot_tools import fix_encoding

from utils import lru
from utils import tools
from utils import zip_package

import isolateserver


# Absolute path to this file (can be None if running from zip on Mac).
THIS_FILE_PATH = os.path.abspath(__file__) if __file__ else None

# Directory that contains this file (might be inside zip package).
BASE_DIR = os.path.dirname(THIS_FILE_PATH) if __file__ else None

# Directory that contains currently running script file.
if zip_package.get_main_script_path():
  MAIN_DIR = os.path.dirname(
      os.path.abspath(zip_package.get_main_script_path()))
else:
  # This happens when 'import run_isolated' is executed at the python
  # interactive prompt, in that case __file__ is undefined.
  MAIN_DIR = None

# Types of action accepted by link_file().
HARDLINK, HARDLINK_WITH_FALLBACK, SYMLINK, COPY = range(1, 5)

# The name of the log file to use.
RUN_ISOLATED_LOG_FILE = 'run_isolated.log'

# The name of the log to use for the run_test_cases.py command
RUN_TEST_CASES_LOG = 'run_test_cases.log'

# Maximum expected delay (in seconds) between successive file fetches
# in run_tha_test. If it takes longer than that, a deadlock might be happening
# and all stack frames for all threads are dumped to log.
DEADLOCK_TIMEOUT = 5 * 60


# Used by get_flavor().
FLAVOR_MAPPING = {
  'cygwin': 'win',
  'win32': 'win',
  'darwin': 'mac',
  'sunos5': 'solaris',
  'freebsd7': 'freebsd',
  'freebsd8': 'freebsd',
}


def get_as_zip_package(executable=True):
  """Returns ZipPackage with this module and all its dependencies.

  If |executable| is True will store run_isolated.py as __main__.py so that
  zip package is directly executable be python.
  """
  # Building a zip package when running from another zip package is
  # unsupported and probably unneeded.
  assert not zip_package.is_zipped_module(sys.modules[__name__])
  assert THIS_FILE_PATH
  assert BASE_DIR
  package = zip_package.ZipPackage(root=BASE_DIR)
  package.add_python_file(THIS_FILE_PATH, '__main__.py' if executable else None)
  package.add_python_file(os.path.join(BASE_DIR, 'isolateserver.py'))
  package.add_directory(os.path.join(BASE_DIR, 'third_party'))
  package.add_directory(os.path.join(BASE_DIR, 'utils'))
  return package


def get_flavor():
  """Returns the system default flavor. Copied from gyp/pylib/gyp/common.py."""
  return FLAVOR_MAPPING.get(sys.platform, 'linux')


def os_link(source, link_name):
  """Add support for os.link() on Windows."""
  if sys.platform == 'win32':
    if not ctypes.windll.kernel32.CreateHardLinkW(
        unicode(link_name), unicode(source), 0):
      raise OSError()
  else:
    os.link(source, link_name)


def readable_copy(outfile, infile):
  """Makes a copy of the file that is readable by everyone."""
  shutil.copy2(infile, outfile)
  read_enabled_mode = (os.stat(outfile).st_mode | stat.S_IRUSR |
                       stat.S_IRGRP | stat.S_IROTH)
  os.chmod(outfile, read_enabled_mode)


def link_file(outfile, infile, action):
  """Links a file. The type of link depends on |action|."""
  logging.debug('Mapping %s to %s' % (infile, outfile))
  if action not in (HARDLINK, HARDLINK_WITH_FALLBACK, SYMLINK, COPY):
    raise ValueError('Unknown mapping action %s' % action)
  if not os.path.isfile(infile):
    raise isolateserver.MappingError('%s is missing' % infile)
  if os.path.isfile(outfile):
    raise isolateserver.MappingError(
        '%s already exist; insize:%d; outsize:%d' %
        (outfile, os.stat(infile).st_size, os.stat(outfile).st_size))

  if action == COPY:
    readable_copy(outfile, infile)
  elif action == SYMLINK and sys.platform != 'win32':
    # On windows, symlink are converted to hardlink and fails over to copy.
    os.symlink(infile, outfile)  # pylint: disable=E1101
  else:
    try:
      os_link(infile, outfile)
    except OSError as e:
      if action == HARDLINK:
        raise isolateserver.MappingError(
            'Failed to hardlink %s to %s: %s' % (infile, outfile, e))
      # Probably a different file system.
      logging.warning(
          'Failed to hardlink, failing back to copy %s to %s' % (
            infile, outfile))
      readable_copy(outfile, infile)


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
  assert os.path.isabs(root), root
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


def get_free_space(path):
  """Returns the number of free bytes."""
  if sys.platform == 'win32':
    free_bytes = ctypes.c_ulonglong(0)
    ctypes.windll.kernel32.GetDiskFreeSpaceExW(
        ctypes.c_wchar_p(path), None, None, ctypes.pointer(free_bytes))
    return free_bytes.value
  # For OSes other than Windows.
  f = os.statvfs(path)  # pylint: disable=E1101
  return f.f_bfree * f.f_frsize


def make_temp_dir(prefix, root_dir):
  """Returns a temporary directory on the same file system as root_dir."""
  base_temp_dir = None
  if not is_same_filesystem(root_dir, tempfile.gettempdir()):
    base_temp_dir = os.path.dirname(root_dir)
  return tempfile.mkdtemp(prefix=prefix, dir=base_temp_dir)


class CachePolicies(object):
  def __init__(self, max_cache_size, min_free_space, max_items):
    """
    Arguments:
    - max_cache_size: Trim if the cache gets larger than this value. If 0, the
                      cache is effectively a leak.
    - min_free_space: Trim if disk free space becomes lower than this value. If
                      0, it unconditionally fill the disk.
    - max_items: Maximum number of items to keep in the cache. If 0, do not
                 enforce a limit.
    """
    self.max_cache_size = max_cache_size
    self.min_free_space = min_free_space
    self.max_items = max_items


class DiskCache(object):
  """Stateful LRU cache in a flat hash table in a directory.

  Saves its state as json file.
  """
  STATE_FILE = 'state.json'

  def __init__(self, cache_dir, retriever, policies, algo):
    """
    Arguments:
    - cache_dir: Directory where to place the cache.
    - retriever: API where to fetch items from.
    - policies: cache retention policies.
    - algo: hashing algorithm used.
    """
    self.cache_dir = cache_dir
    self.retriever = retriever
    self.policies = policies
    self._pool = None
    self.state_file = os.path.join(cache_dir, self.STATE_FILE)
    self.lru = lru.LRUDict()

    # Items currently being fetched. Keep it local to reduce lock contention.
    self._pending_queue = set()

    # Profiling values.
    self._added = []
    self._removed = []
    self._free_disk = 0

    with tools.Profiler('Setup'):
      if not os.path.isdir(self.cache_dir):
        os.makedirs(self.cache_dir)

      # Load state of the cache.
      if os.path.isfile(self.state_file):
        try:
          self.lru = lru.LRUDict.load(self.state_file)
        except ValueError as err:
          logging.error('Failed to load cache state: %s' % (err,))
          # Don't want to keep broken state file.
          os.remove(self.state_file)

      # Ensure that all files listed in the state still exist and add new ones.
      previous = self.lru.keys_set()
      unknown = []
      for filename in os.listdir(self.cache_dir):
        if filename == self.STATE_FILE:
          continue
        if filename in previous:
          previous.remove(filename)
          continue
        # An untracked file.
        if not isolateserver.is_valid_hash(filename, algo):
          logging.warning('Removing unknown file %s from cache', filename)
          os.remove(self._path(filename))
          continue
        # File that's not referenced in 'state.json'.
        # TODO(vadimsh): Verify its SHA1 matches file name.
        logging.warning('Adding unknown file %s to cache', filename)
        unknown.append(filename)

      if unknown:
        # Add as oldest files. They will be deleted eventually if not accessed.
        self._add_oldest_list(unknown)
        logging.warning('Added back %d unknown files', len(unknown))

      if previous:
        # Filter out entries that were not found.
        logging.warning('Removed %d lost files', len(previous))
        for filename in previous:
          self.lru.pop(filename)
      self.trim()

  def set_pool(self, pool):
    """Sets an isolateserver.WorkerPool."""
    self._pool = pool

  def __enter__(self):
    return self

  def __exit__(self, _exc_type, _exec_value, _traceback):
    with tools.Profiler('CleanupTrimming'):
      self.trim()

    logging.info(
        '%5d (%8dkb) added', len(self._added), sum(self._added) / 1024)
    logging.info(
        '%5d (%8dkb) current',
        len(self.lru),
        sum(self.lru.itervalues()) / 1024)
    logging.info(
        '%5d (%8dkb) removed', len(self._removed), sum(self._removed) / 1024)
    logging.info('       %8dkb free', self._free_disk / 1024)
    return False

  def trim(self):
    """Trims anything we don't know, make sure enough free space exists."""
    # Ensure maximum cache size.
    if self.policies.max_cache_size:
      total_size = sum(self.lru.itervalues())
      while total_size > self.policies.max_cache_size:
        total_size -= self._remove_lru_file()

    # Ensure maximum number of items in the cache.
    if self.policies.max_items and len(self.lru) > self.policies.max_items:
      for _ in xrange(len(self.lru) - self.policies.max_items):
        self._remove_lru_file()

    # Ensure enough free space.
    self._free_disk = get_free_space(self.cache_dir)
    trimmed_due_to_space = False
    while (
        self.policies.min_free_space and
        self.lru and
        self._free_disk < self.policies.min_free_space):
      trimmed_due_to_space = True
      self._remove_lru_file()
      self._free_disk = get_free_space(self.cache_dir)
    if trimmed_due_to_space:
      total = sum(self.lru.itervalues())
      logging.warning(
          'Trimmed due to not enough free disk space: %.1fkb free, %.1fkb '
          'cache (%.1f%% of its maximum capacity)',
          self._free_disk / 1024.,
          total / 1024.,
          100. * self.policies.max_cache_size / float(total),
          )
    self._save()

  def retrieve(self, priority, item, size):
    """Retrieves a file from the remote, if not already cached, and adds it to
    the cache.

    If the file is in the cache, verify that the file is valid (i.e. it is
    the correct size), retrieving it again if it isn't.
    """
    assert not '/' in item
    path = self._path(item)
    found = False

    if item in self.lru:
      # Note that is doesn't compute the hash so it could still be corrupted.
      if not isolateserver.is_valid_file(self._path(item), size):
        self.lru.pop(item)
        self._delete_file(item, size)
      else:
        # Was already in cache. Update it's LRU value by putting it at the end.
        self.lru.touch(item)
        found = True

    if not found:
      if item in self._pending_queue:
        # Already pending. The same object could be referenced multiple times.
        return
      # TODO(maruel): It should look at the free disk space, the current cache
      # size and the size of the new item on every new item:
      # - Trim the cache as more entries are listed when free disk space is low,
      #   otherwise if the amount of data downloaded during the run > free disk
      #   space, it'll crash.
      # - Make sure there's enough free disk space to fit all dependencies of
      #   this run! If not, abort early.
      self._pool.add_task(priority, self._store, item, path, size)
      self._pending_queue.add(item)

  def add(self, filepath, obj):
    """Forcibly adds a file to the cache."""
    if obj not in self.lru:
      link_file(self._path(obj), filepath, HARDLINK)
      self._add(obj)

  def store_to(self, obj, dest):
    link_file(dest, self._path(obj), HARDLINK)

  def read(self, item):
    """Reads an item from the cache."""
    with open(self._path(item), 'rb') as f:
      return f.read()

  def wait_for(self, items):
    """Starts a loop that waits for at least one of |items| to be retrieved.

    Returns the first item retrieved.
    """
    # Flush items already present.
    for item in items:
      if item in self.lru:
        return item

    assert all(i in self._pending_queue for i in items), (
        items, self._pending_queue)
    # Note that:
    #   len(self._pending_queue) ==
    #   ( len(self.remote_fetcher._workers) - self.remote_fetcher._ready +
    #     len(self._remote._queue) + len(self._remote.done))
    # There is no lock-free way to verify that.
    while self._pending_queue:
      item = self._pool.get_one_result()
      self._pending_queue.remove(item)
      self._add(item)
      if item in items:
        return item

  def _path(self, item):
    """Returns the path to one item."""
    return os.path.join(self.cache_dir, item)

  def _save(self):
    """Saves the LRU ordering."""
    self.lru.save(self.state_file)

  def _remove_lru_file(self):
    """Removes the last recently used file and returns its size."""
    item, size = self.lru.pop_oldest()
    self._delete_file(item, size)
    return size

  def _add(self, item):
    """Adds an item into LRU cache marking it as a newest one."""
    size = os.stat(self._path(item)).st_size
    self._added.append(size)
    self.lru.add(item, size)

  def _add_oldest_list(self, items):
    """Adds a bunch of items into LRU cache marking them as oldest ones."""
    pairs = []
    for item in items:
      size = os.stat(self._path(item)).st_size
      self._added.append(size)
      pairs.append((item, size))
    self.lru.batch_insert_oldest(pairs)

  def _store(self, item, path, expected_size):
    """Stores the data generated by remote_fetcher."""
    isolateserver.file_write(path, self.retriever(item, expected_size))
    return item

  def _delete_file(self, item, size):
    """Deletes cache file from the file system."""
    self._removed.append(size)
    try:
      os.remove(self._path(item))
    except OSError as e:
      logging.error('Error attempting to delete a file\n%s' % e)


def run_tha_test(isolated_hash, cache_dir, retriever, policies):
  """Downloads the dependencies in the cache, hardlinks them into a temporary
  directory and runs the executable.
  """
  algo = hashlib.sha1
  outdir = None
  try:
    cache = DiskCache(cache_dir, retriever, policies, algo)
    # |cache_dir| may not exist until DiskCache() instance is created.
    outdir = make_temp_dir('run_tha_test', cache_dir)
    try:
      settings = isolateserver.fetch_isolated(
          isolated_hash, cache, outdir, get_flavor(), algo, True)
    except isolateserver.ConfigError as e:
      print >> sys.stderr, str(e)
      return 1

    if settings.read_only:
      logging.info('Making files read only')
      make_writable(outdir, True)
    cwd = os.path.normpath(os.path.join(outdir, settings.relative_cwd))
    logging.info('Running %s, cwd=%s' % (settings.command, cwd))

    # TODO(csharp): This should be specified somewhere else.
    # TODO(vadimsh): Pass it via 'env_vars' in manifest.
    # Add a rotating log file if one doesn't already exist.
    env = os.environ.copy()
    if MAIN_DIR:
      env.setdefault('RUN_TEST_CASES_LOG_FILE',
                    os.path.join(MAIN_DIR, RUN_TEST_CASES_LOG))
    try:
      with tools.Profiler('RunTest'):
        return subprocess.call(settings.command, cwd=cwd, env=env)
    except OSError:
      print >> sys.stderr, 'Failed to run %s; cwd=%s' % (settings.command, cwd)
      return 1
  finally:
    if outdir:
      rmtree(outdir)


def main():
  tools.disable_buffering()
  parser = tools.OptionParserWithLogging(
      usage='%prog <options>',
      version=__version__,
      log_file=RUN_ISOLATED_LOG_FILE)

  group = optparse.OptionGroup(parser, 'Data source')
  group.add_option(
      '-s', '--isolated',
      metavar='FILE',
      help='File/url describing what to map or run')
  group.add_option(
      '-H', '--hash',
      help='Hash of the .isolated to grab from the hash table')
  group.add_option(
      '-I', '--isolate-server',
      metavar='URL', default='',
      help='Isolate server to use')
  group.add_option(
      '-n', '--namespace',
      default='default-gzip',
      help='namespace to use when using isolateserver, default: %default')
  parser.add_option_group(group)

  group = optparse.OptionGroup(parser, 'Cache management')
  group.add_option(
      '--cache',
      default='cache',
      metavar='DIR',
      help='Cache directory, default=%default')
  group.add_option(
      '--max-cache-size',
      type='int',
      metavar='NNN',
      default=20*1024*1024*1024,
      help='Trim if the cache gets larger than this value, default=%default')
  group.add_option(
      '--min-free-space',
      type='int',
      metavar='NNN',
      default=2*1024*1024*1024,
      help='Trim if disk free space becomes lower than this value, '
           'default=%default')
  group.add_option(
      '--max-items',
      type='int',
      metavar='NNN',
      default=100000,
      help='Trim if more than this number of items are in the cache '
           'default=%default')
  parser.add_option_group(group)

  options, args = parser.parse_args()

  if bool(options.isolated) == bool(options.hash):
    logging.debug('One and only one of --isolated or --hash is required.')
    parser.error('One and only one of --isolated or --hash is required.')
  if args:
    logging.debug('Unsupported args %s' % ' '.join(args))
    parser.error('Unsupported args %s' % ' '.join(args))
  if not options.isolate_server:
    parser.error('--isolate-server is required.')

  options.cache = os.path.abspath(options.cache)
  policies = CachePolicies(
      options.max_cache_size, options.min_free_space, options.max_items)

  retriever = isolateserver.get_storage_api(
      options.isolate_server, options.namespace)
  try:
    return run_tha_test(
        options.isolated or options.hash,
        options.cache,
        retriever.fetch,
        policies)
  except Exception, e:
    # Make sure any exception is logged.
    logging.exception(e)
    return 1


if __name__ == '__main__':
  # Ensure that we are always running with the correct encoding.
  fix_encoding.fix_encoding()
  sys.exit(main())
