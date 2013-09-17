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

from third_party.depot_tools import fix_encoding

from utils import lru
from utils import threading_utils
from utils import tools
from utils import zip_package

import isolateserver
from isolateserver import ConfigError


# Absolute path to this file (can be None if running from zip on Mac).
THIS_FILE_PATH = os.path.abspath(__file__) if __file__ else None

# Directory that contains this file (might be inside zip package).
BASE_DIR = os.path.dirname(THIS_FILE_PATH) if __file__ else None

# Directory that contains currently running script file.
MAIN_DIR = os.path.dirname(os.path.abspath(zip_package.get_main_script_path()))

# Types of action accepted by link_file().
HARDLINK, HARDLINK_WITH_FALLBACK, SYMLINK, COPY = range(1, 5)

# The name of the log file to use.
RUN_ISOLATED_LOG_FILE = 'run_isolated.log'

# The name of the log to use for the run_test_cases.py command
RUN_TEST_CASES_LOG = 'run_test_cases.log'

# The delay (in seconds) to wait between logging statements when retrieving
# the required files. This is intended to let the user (or buildbot) know that
# the program is still running.
DELAY_BETWEEN_UPDATES_IN_SECS = 30

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


def load_isolated(content, os_flavor, algo):
  """Verifies the .isolated file is valid and loads this object with the json
  data.
  """
  try:
    data = json.loads(content)
  except ValueError:
    raise ConfigError('Failed to parse: %s...' % content[:100])

  if not isinstance(data, dict):
    raise ConfigError('Expected dict, got %r' % data)

  for key, value in data.iteritems():
    if key == 'command':
      if not isinstance(value, list):
        raise ConfigError('Expected list, got %r' % value)
      if not value:
        raise ConfigError('Expected non-empty command')
      for subvalue in value:
        if not isinstance(subvalue, basestring):
          raise ConfigError('Expected string, got %r' % subvalue)

    elif key == 'files':
      if not isinstance(value, dict):
        raise ConfigError('Expected dict, got %r' % value)
      for subkey, subvalue in value.iteritems():
        if not isinstance(subkey, basestring):
          raise ConfigError('Expected string, got %r' % subkey)
        if not isinstance(subvalue, dict):
          raise ConfigError('Expected dict, got %r' % subvalue)
        for subsubkey, subsubvalue in subvalue.iteritems():
          if subsubkey == 'l':
            if not isinstance(subsubvalue, basestring):
              raise ConfigError('Expected string, got %r' % subsubvalue)
          elif subsubkey == 'm':
            if not isinstance(subsubvalue, int):
              raise ConfigError('Expected int, got %r' % subsubvalue)
          elif subsubkey == 'h':
            if not isolateserver.is_valid_hash(subsubvalue, algo):
              raise ConfigError('Expected sha-1, got %r' % subsubvalue)
          elif subsubkey == 's':
            if not isinstance(subsubvalue, int):
              raise ConfigError('Expected int, got %r' % subsubvalue)
          else:
            raise ConfigError('Unknown subsubkey %s' % subsubkey)
        if bool('h' in subvalue) and bool('l' in subvalue):
          raise ConfigError(
              'Did not expect both \'h\' (sha-1) and \'l\' (link), got: %r' %
              subvalue)

    elif key == 'includes':
      if not isinstance(value, list):
        raise ConfigError('Expected list, got %r' % value)
      if not value:
        raise ConfigError('Expected non-empty includes list')
      for subvalue in value:
        if not isolateserver.is_valid_hash(subvalue, algo):
          raise ConfigError('Expected sha-1, got %r' % subvalue)

    elif key == 'read_only':
      if not isinstance(value, bool):
        raise ConfigError('Expected bool, got %r' % value)

    elif key == 'relative_cwd':
      if not isinstance(value, basestring):
        raise ConfigError('Expected string, got %r' % value)

    elif key == 'os':
      if os_flavor and value != os_flavor:
        raise ConfigError(
            'Expected \'os\' to be \'%s\' but got \'%s\'' %
            (os_flavor, value))

    else:
      raise ConfigError('Unknown key %s' % key)

  return data


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

  def __init__(self, cache_dir, remote_fetcher, policies, algo):
    """
    Arguments:
    - cache_dir: Directory where to place the cache.
    - remote_fetcher: isolateserver.RemoteOperation where to fetch items from.
    - policies: cache retention policies.
    - algo: hashing algorithm used.
    """
    self.cache_dir = cache_dir
    self.remote_fetcher = remote_fetcher
    self.policies = policies
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
          os.remove(self.path(filename))
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
    path = self.path(item)
    found = False

    if item in self.lru:
      # Note that is doesn't compute the hash so it could still be corrupted.
      if not isolateserver.is_valid_file(self.path(item), size):
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
      self.remote_fetcher.add_item(priority, item, path, size)
      self._pending_queue.add(item)

  def add(self, filepath, obj):
    """Forcibly adds a file to the cache."""
    if obj not in self.lru:
      link_file(self.path(obj), filepath, HARDLINK)
      self._add(obj)

  def path(self, item):
    """Returns the path to one item."""
    return os.path.join(self.cache_dir, item)

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
      item = self.remote_fetcher.get_one_result()
      self._pending_queue.remove(item)
      self._add(item)
      if item in items:
        return item

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
    size = os.stat(self.path(item)).st_size
    self._added.append(size)
    self.lru.add(item, size)

  def _add_oldest_list(self, items):
    """Adds a bunch of items into LRU cache marking them as oldest ones."""
    pairs = []
    for item in items:
      size = os.stat(self.path(item)).st_size
      self._added.append(size)
      pairs.append((item, size))
    self.lru.batch_insert_oldest(pairs)

  def _delete_file(self, item, size):
    """Deletes cache file from the file system."""
    self._removed.append(size)
    try:
      os.remove(self.path(item))
    except OSError as e:
      logging.error('Error attempting to delete a file\n%s' % e)


class IsolatedFile(object):
  """Represents a single parsed .isolated file."""
  def __init__(self, obj_hash, algo):
    """|obj_hash| is really the sha-1 of the file."""
    logging.debug('IsolatedFile(%s)' % obj_hash)
    self.obj_hash = obj_hash
    self.algo = algo
    # Set once all the left-side of the tree is parsed. 'Tree' here means the
    # .isolate and all the .isolated files recursively included by it with
    # 'includes' key. The order of each sha-1 in 'includes', each representing a
    # .isolated file in the hash table, is important, as the later ones are not
    # processed until the firsts are retrieved and read.
    self.can_fetch = False

    # Raw data.
    self.data = {}
    # A IsolatedFile instance, one per object in self.includes.
    self.children = []

    # Set once the .isolated file is loaded.
    self._is_parsed = False
    # Set once the files are fetched.
    self.files_fetched = False

  def load(self, content):
    """Verifies the .isolated file is valid and loads this object with the json
    data.
    """
    logging.debug('IsolatedFile.load(%s)' % self.obj_hash)
    assert not self._is_parsed
    self.data = load_isolated(content, get_flavor(), self.algo)
    self.children = [
      IsolatedFile(i, self.algo) for i in self.data.get('includes', [])
    ]
    self._is_parsed = True

  def fetch_files(self, cache, files):
    """Adds files in this .isolated file not present in |files| dictionary.

    Preemptively request files.

    Note that |files| is modified by this function.
    """
    assert self.can_fetch
    if not self._is_parsed or self.files_fetched:
      return
    logging.debug('fetch_files(%s)' % self.obj_hash)
    for filepath, properties in self.data.get('files', {}).iteritems():
      # Root isolated has priority on the files being mapped. In particular,
      # overriden files must not be fetched.
      if filepath not in files:
        files[filepath] = properties
        if 'h' in properties:
          # Preemptively request files.
          logging.debug('fetching %s' % filepath)
          cache.retrieve(
              isolateserver.RemoteOperation.MED,
              properties['h'],
              properties['s'])
    self.files_fetched = True


class Settings(object):
  """Results of a completely parsed .isolated file."""
  def __init__(self):
    self.command = []
    self.files = {}
    self.read_only = None
    self.relative_cwd = None
    # The main .isolated file, a IsolatedFile instance.
    self.root = None

  def load(self, cache, root_isolated_hash, algo):
    """Loads the .isolated and all the included .isolated asynchronously.

    It enables support for "included" .isolated files. They are processed in
    strict order but fetched asynchronously from the cache. This is important so
    that a file in an included .isolated file that is overridden by an embedding
    .isolated file is not fetched needlessly. The includes are fetched in one
    pass and the files are fetched as soon as all the ones on the left-side
    of the tree were fetched.

    The prioritization is very important here for nested .isolated files.
    'includes' have the highest priority and the algorithm is optimized for both
    deep and wide trees. A deep one is a long link of .isolated files referenced
    one at a time by one item in 'includes'. A wide one has a large number of
    'includes' in a single .isolated file. 'left' is defined as an included
    .isolated file earlier in the 'includes' list. So the order of the elements
    in 'includes' is important.
    """
    self.root = IsolatedFile(root_isolated_hash, algo)

    # Isolated files being retrieved now: hash -> IsolatedFile instance.
    pending = {}
    # Set of hashes of already retrieved items to refuse recursive includes.
    seen = set()

    def retrieve(isolated_file):
      h = isolated_file.obj_hash
      if h in seen:
        raise ConfigError('IsolatedFile %s is retrieved recursively' % h)
      assert h not in pending
      seen.add(h)
      pending[h] = isolated_file
      cache.retrieve(
          isolateserver.RemoteOperation.HIGH,
          h,
          isolateserver.UNKNOWN_FILE_SIZE)

    retrieve(self.root)

    while pending:
      item_hash = cache.wait_for(pending)
      item = pending.pop(item_hash)
      item.load(open(cache.path(item_hash), 'r').read())
      if item_hash == root_isolated_hash:
        # It's the root item.
        item.can_fetch = True

      for new_child in item.children:
        retrieve(new_child)

      # Traverse the whole tree to see if files can now be fetched.
      self._traverse_tree(cache, self.root)

    def check(n):
      return all(check(x) for x in n.children) and n.files_fetched
    assert check(self.root)

    self.relative_cwd = self.relative_cwd or ''
    self.read_only = self.read_only or False

  def _traverse_tree(self, cache, node):
    if node.can_fetch:
      if not node.files_fetched:
        self._update_self(cache, node)
      will_break = False
      for i in node.children:
        if not i.can_fetch:
          if will_break:
            break
          # Automatically mark the first one as fetcheable.
          i.can_fetch = True
          will_break = True
        self._traverse_tree(cache, i)

  def _update_self(self, cache, node):
    node.fetch_files(cache, self.files)
    # Grabs properties.
    if not self.command and node.data.get('command'):
      self.command = node.data['command']
    if self.read_only is None and node.data.get('read_only') is not None:
      self.read_only = node.data['read_only']
    if (self.relative_cwd is None and
        node.data.get('relative_cwd') is not None):
      self.relative_cwd = node.data['relative_cwd']


def run_tha_test(isolated_hash, cache_dir, retriever, policies):
  """Downloads the dependencies in the cache, hardlinks them into a temporary
  directory and runs the executable.
  """
  settings = Settings()
  remote = isolateserver.RemoteOperation(retriever)
  algo = hashlib.sha1
  with DiskCache(cache_dir, remote, policies, algo) as cache:
    outdir = make_temp_dir('run_tha_test', cache_dir)
    try:
      # Initiate all the files download.
      with tools.Profiler('GetIsolateds'):
        # Optionally support local files.
        if not isolateserver.is_valid_hash(isolated_hash, algo):
          # Adds it in the cache. While not strictly necessary, this simplifies
          # the rest.
          h = isolateserver.hash_file(isolated_hash, algo)
          cache.add(isolated_hash, h)
          isolated_hash = h
        settings.load(cache, isolated_hash, algo)

      if not settings.command:
        print >> sys.stderr, 'No command to run'
        return 1

      with tools.Profiler('GetRest'):
        isolateserver.create_directories(outdir, settings.files)
        isolateserver.create_links(outdir, settings.files.iteritems())
        remaining = isolateserver.generate_remaining_files(
            settings.files.iteritems())

        # Do bookkeeping while files are being downloaded in the background.
        cwd, cmd = isolateserver.setup_commands(
            outdir, settings.relative_cwd, settings.command[:])

        # Now block on the remaining files to be downloaded and mapped.
        logging.info('Retrieving remaining files')
        last_update = time.time()
        with threading_utils.DeadlockDetector(DEADLOCK_TIMEOUT) as detector:
          while remaining:
            detector.ping()
            obj = cache.wait_for(remaining)
            for filepath, properties in remaining.pop(obj):
              outfile = os.path.join(outdir, filepath)
              link_file(outfile, cache.path(obj), HARDLINK)
              if 'm' in properties:
                # It's not set on Windows.
                os.chmod(outfile, properties['m'])

            if time.time() - last_update > DELAY_BETWEEN_UPDATES_IN_SECS:
              msg = '%d files remaining...' % len(remaining)
              print msg
              logging.info(msg)
              last_update = time.time()

      if settings.read_only:
        logging.info('Making files read only')
        make_writable(outdir, True)
      logging.info('Running %s, cwd=%s' % (cmd, cwd))

      # TODO(csharp): This should be specified somewhere else.
      # TODO(vadimsh): Pass it via 'env_vars' in manifest.
      # Add a rotating log file if one doesn't already exist.
      env = os.environ.copy()
      env.setdefault('RUN_TEST_CASES_LOG_FILE',
                     os.path.join(MAIN_DIR, RUN_TEST_CASES_LOG))
      try:
        with tools.Profiler('RunTest'):
          return subprocess.call(cmd, cwd=cwd, env=env)
      except OSError:
        print >> sys.stderr, 'Failed to run %s; cwd=%s' % (cmd, cwd)
        raise
    finally:
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
      '-I', '--isolate-server', metavar='URL',
      default=
          'https://isolateserver.appspot.com',
      help='Remote where to get the items. Defaults to %default')
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

  options.cache = os.path.abspath(options.cache)
  policies = CachePolicies(
      options.max_cache_size, options.min_free_space, options.max_items)

  retriever = isolateserver.get_storage_api(
      options.isolate_server, options.namespace)
  try:
    return run_tha_test(
        options.isolated or options.hash,
        options.cache,
        retriever.retrieve,
        policies)
  except Exception, e:
    # Make sure any exception is logged.
    logging.exception(e)
    return 1


if __name__ == '__main__':
  # Ensure that we are always running with the correct encoding.
  fix_encoding.fix_encoding()
  sys.exit(main())
