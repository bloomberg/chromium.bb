#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Reads a .isolated, creates a tree of hardlinks and runs the test.

Keeps a local cache.
"""

__version__ = '0.2'

import ctypes
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
from utils import threading_utils
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


def try_remove(filepath):
  """Removes a file without crashing even if it doesn't exist."""
  try:
    os.remove(filepath)
  except OSError:
    pass


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


class DiskCache(isolateserver.LocalCache):
  """Stateful LRU cache in a flat hash table in a directory.

  Saves its state as json file.
  """
  STATE_FILE = 'state.json'

  def __init__(self, cache_dir, policies, algo):
    """
    Arguments:
      cache_dir: directory where to place the cache.
      policies: cache retention policies.
      algo: hashing algorithm used.
    """
    super(DiskCache, self).__init__()
    self.algo = algo
    self.cache_dir = cache_dir
    self.policies = policies
    self.state_file = os.path.join(cache_dir, self.STATE_FILE)

    # All protected methods (starting with '_') except _path should be called
    # with this lock locked.
    self._lock = threading_utils.LockWithAssert()
    self._lru = lru.LRUDict()

    # Profiling values.
    self._added = []
    self._removed = []
    self._free_disk = 0

    with tools.Profiler('Setup'):
      with self._lock:
        self._load()

  def __enter__(self):
    return self

  def __exit__(self, _exc_type, _exec_value, _traceback):
    with tools.Profiler('CleanupTrimming'):
      with self._lock:
        self._trim()

        logging.info(
            '%5d (%8dkb) added',
            len(self._added), sum(self._added) / 1024)
        logging.info(
            '%5d (%8dkb) current',
            len(self._lru),
            sum(self._lru.itervalues()) / 1024)
        logging.info(
            '%5d (%8dkb) removed',
            len(self._removed), sum(self._removed) / 1024)
        logging.info(
            '       %8dkb free',
            self._free_disk / 1024)
    return False

  def cached_set(self):
    with self._lock:
      return self._lru.keys_set()

  def touch(self, digest, size):
    # Verify an actual file is valid. Note that is doesn't compute the hash so
    # it could still be corrupted. Do it outside the lock.
    if not isolateserver.is_valid_file(self._path(digest), size):
      return False

    # Update it's LRU position.
    with self._lock:
      if digest not in self._lru:
        return False
      self._lru.touch(digest)
    return True

  def evict(self, digest):
    with self._lock:
      self._lru.pop(digest)
      self._delete_file(digest, isolateserver.UNKNOWN_FILE_SIZE)

  def read(self, digest):
    with open(self._path(digest), 'rb') as f:
      return f.read()

  def write(self, digest, content):
    path = self._path(digest)
    try:
      size = isolateserver.file_write(path, content)
    except:
      # There are two possible places were an exception can occur:
      #   1) Inside |content| generator in case of network or unzipping errors.
      #   2) Inside file_write itself in case of disk IO errors.
      # In any case delete an incomplete file and propagate the exception to
      # caller, it will be logged there.
      try_remove(path)
      raise
    with self._lock:
      self._add(digest, size)

  def link(self, digest, dest, file_mode=None):
    link_file(dest, self._path(digest), HARDLINK)
    if file_mode is not None:
      os.chmod(dest, file_mode)

  def _load(self):
    """Loads state of the cache from json file."""
    self._lock.assert_locked()

    if not os.path.isdir(self.cache_dir):
      os.makedirs(self.cache_dir)

    # Load state of the cache.
    if os.path.isfile(self.state_file):
      try:
        self._lru = lru.LRUDict.load(self.state_file)
      except ValueError as err:
        logging.error('Failed to load cache state: %s' % (err,))
        # Don't want to keep broken state file.
        os.remove(self.state_file)

    # Ensure that all files listed in the state still exist and add new ones.
    previous = self._lru.keys_set()
    unknown = []
    for filename in os.listdir(self.cache_dir):
      if filename == self.STATE_FILE:
        continue
      if filename in previous:
        previous.remove(filename)
        continue
      # An untracked file.
      if not isolateserver.is_valid_hash(filename, self.algo):
        logging.warning('Removing unknown file %s from cache', filename)
        try_remove(self._path(filename))
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
        self._lru.pop(filename)
    self._trim()

  def _save(self):
    """Saves the LRU ordering."""
    self._lock.assert_locked()
    self._lru.save(self.state_file)

  def _trim(self):
    """Trims anything we don't know, make sure enough free space exists."""
    self._lock.assert_locked()

    # Ensure maximum cache size.
    if self.policies.max_cache_size:
      total_size = sum(self._lru.itervalues())
      while total_size > self.policies.max_cache_size:
        total_size -= self._remove_lru_file()

    # Ensure maximum number of items in the cache.
    if self.policies.max_items and len(self._lru) > self.policies.max_items:
      for _ in xrange(len(self._lru) - self.policies.max_items):
        self._remove_lru_file()

    # Ensure enough free space.
    self._free_disk = get_free_space(self.cache_dir)
    trimmed_due_to_space = False
    while (
        self.policies.min_free_space and
        self._lru and
        self._free_disk < self.policies.min_free_space):
      trimmed_due_to_space = True
      self._remove_lru_file()
      self._free_disk = get_free_space(self.cache_dir)
    if trimmed_due_to_space:
      total = sum(self._lru.itervalues())
      logging.warning(
          'Trimmed due to not enough free disk space: %.1fkb free, %.1fkb '
          'cache (%.1f%% of its maximum capacity)',
          self._free_disk / 1024.,
          total / 1024.,
          100. * self.policies.max_cache_size / float(total),
          )
    self._save()

  def _path(self, digest):
    """Returns the path to one item."""
    return os.path.join(self.cache_dir, digest)

  def _remove_lru_file(self):
    """Removes the last recently used file and returns its size."""
    self._lock.assert_locked()
    digest, size = self._lru.pop_oldest()
    self._delete_file(digest, size)
    return size

  def _add(self, digest, size=isolateserver.UNKNOWN_FILE_SIZE):
    """Adds an item into LRU cache marking it as a newest one."""
    self._lock.assert_locked()
    if size == isolateserver.UNKNOWN_FILE_SIZE:
      size = os.stat(self._path(digest)).st_size
    self._added.append(size)
    self._lru.add(digest, size)

  def _add_oldest_list(self, digests):
    """Adds a bunch of items into LRU cache marking them as oldest ones."""
    self._lock.assert_locked()
    pairs = []
    for digest in digests:
      size = os.stat(self._path(digest)).st_size
      self._added.append(size)
      pairs.append((digest, size))
    self._lru.batch_insert_oldest(pairs)

  def _delete_file(self, digest, size=isolateserver.UNKNOWN_FILE_SIZE):
    """Deletes cache file from the file system."""
    self._lock.assert_locked()
    try:
      if size == isolateserver.UNKNOWN_FILE_SIZE:
        size = os.stat(self._path(digest)).st_size
      os.remove(self._path(digest))
      self._removed.append(size)
    except OSError as e:
      logging.error('Error attempting to delete a file %s:\n%s' % (digest, e))


def run_tha_test(isolated_hash, storage, cache, algo, outdir):
  """Downloads the dependencies in the cache, hardlinks them into a |outdir|
  and runs the executable.
  """
  try:
    try:
      settings = isolateserver.fetch_isolated(
          isolated_hash=isolated_hash,
          storage=storage,
          cache=cache,
          algo=algo,
          outdir=outdir,
          os_flavor=get_flavor(),
          require_command=True)
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
  storage = isolateserver.get_storage(options.isolate_server, options.namespace)
  algo = isolateserver.get_hash_algo(options.namespace)

  try:
    # |options.cache| may not exist until DiskCache() instance is created.
    cache = DiskCache(options.cache, policies, algo)
    outdir = make_temp_dir('run_tha_test', options.cache)
    return run_tha_test(
        options.isolated or options.hash, storage, cache, algo, outdir)
  except Exception as e:
    # Make sure any exception is logged.
    logging.exception(e)
    return 1


if __name__ == '__main__':
  # Ensure that we are always running with the correct encoding.
  fix_encoding.fix_encoding()
  sys.exit(main())
