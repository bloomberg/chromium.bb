#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Reads a .isolated, creates a tree of hardlinks and runs the test.

To improve performance, it keeps a local cache. The local cache can safely be
deleted.

Any ${ISOLATED_OUTDIR} on the command line will be replaced by the location of a
temporary directory upon execution of the command specified in the .isolated
file. All content written to this directory will be uploaded upon termination
and the .isolated file describing this directory will be printed to stdout.
"""

__version__ = '0.3.2'

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

from utils import file_path
from utils import lru
from utils import on_error
from utils import threading_utils
from utils import tools
from utils import zip_package

import auth
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
  package.add_python_file(os.path.join(BASE_DIR, 'auth.py'))
  package.add_directory(os.path.join(BASE_DIR, 'third_party'))
  package.add_directory(os.path.join(BASE_DIR, 'utils'))
  return package


def hardlink(source, link_name):
  """Hardlinks a file.

  Add support for os.link() on Windows.
  """
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
      hardlink(infile, outfile)
    except OSError as e:
      if action == HARDLINK:
        raise isolateserver.MappingError(
            'Failed to hardlink %s to %s: %s' % (infile, outfile, e))
      # Probably a different file system.
      logging.warning(
          'Failed to hardlink, failing back to copy %s to %s' % (
            infile, outfile))
      readable_copy(outfile, infile)


def set_read_only(path, read_only):
  """Sets or resets the write bit on a file or directory.

  Zaps out access to 'group' and 'others'.
  """
  assert isinstance(read_only, bool), read_only
  mode = os.lstat(path).st_mode
  # TODO(maruel): Stop removing GO bits.
  if read_only:
    mode = mode & 0500
  else:
    mode = mode | 0200
  if hasattr(os, 'lchmod'):
    os.lchmod(path, mode)  # pylint: disable=E1101
  else:
    if stat.S_ISLNK(mode):
      # Skip symlink without lchmod() support.
      logging.debug(
          'Can\'t change %sw bit on symlink %s',
          '-' if read_only else '+', path)
      return

    # TODO(maruel): Implement proper DACL modification on Windows.
    os.chmod(path, mode)


def make_tree_read_only(root):
  """Makes all the files in the directories read only.

  Also makes the directories read only, only if it makes sense on the platform.

  This means no file can be created or deleted.
  """
  logging.debug('make_tree_read_only(%s)', root)
  assert os.path.isabs(root), root
  for dirpath, dirnames, filenames in os.walk(root, topdown=True):
    for filename in filenames:
      set_read_only(os.path.join(dirpath, filename), True)
    if sys.platform != 'win32':
      # It must not be done on Windows.
      for dirname in dirnames:
        set_read_only(os.path.join(dirpath, dirname), True)
  if sys.platform != 'win32':
    set_read_only(root, True)


def make_tree_files_read_only(root):
  """Makes all the files in the directories read only but not the directories
  themselves.

  This means files can be created or deleted.
  """
  logging.debug('make_tree_files_read_only(%s)', root)
  assert os.path.isabs(root), root
  if sys.platform != 'win32':
    set_read_only(root, False)
  for dirpath, dirnames, filenames in os.walk(root, topdown=True):
    for filename in filenames:
      set_read_only(os.path.join(dirpath, filename), True)
    if sys.platform != 'win32':
      # It must not be done on Windows.
      for dirname in dirnames:
        set_read_only(os.path.join(dirpath, dirname), False)


def make_tree_writeable(root):
  """Makes all the files in the directories writeable.

  Also makes the directories writeable, only if it makes sense on the platform.

  It is different from make_tree_deleteable() because it unconditionally affects
  the files.
  """
  logging.debug('make_tree_writeable(%s)', root)
  assert os.path.isabs(root), root
  if sys.platform != 'win32':
    set_read_only(root, False)
  for dirpath, dirnames, filenames in os.walk(root, topdown=True):
    for filename in filenames:
      set_read_only(os.path.join(dirpath, filename), False)
    if sys.platform != 'win32':
      # It must not be done on Windows.
      for dirname in dirnames:
        set_read_only(os.path.join(dirpath, dirname), False)


def make_tree_deleteable(root):
  """Changes the appropriate permissions so the files in the directories can be
  deleted.

  On Windows, the files are modified. On other platforms, modify the directory.
  It only does the minimum so the files can be deleted safely.

  Warning on Windows: since file permission is modified, the file node is
  modified. This means that for hard-linked files, every directory entry for the
  file node has its file permission modified.
  """
  logging.debug('make_tree_deleteable(%s)', root)
  assert os.path.isabs(root), root
  if sys.platform != 'win32':
    set_read_only(root, False)
  for dirpath, dirnames, filenames in os.walk(root, topdown=True):
    if sys.platform == 'win32':
      for filename in filenames:
        set_read_only(os.path.join(dirpath, filename), False)
    else:
      for dirname in dirnames:
        set_read_only(os.path.join(dirpath, dirname), False)


def rmtree(root):
  """Wrapper around shutil.rmtree() to retry automatically on Windows.

  On Windows, forcibly kills processes that are found to interfere with the
  deletion.

  Returns:
    True on normal execution, False if berserk techniques (like killing
    processes) had to be used.
  """
  make_tree_deleteable(root)
  logging.info('rmtree(%s)', root)
  if sys.platform != 'win32':
    shutil.rmtree(root)
    return True

  # Windows is more 'challenging'. First tries the soft way: tries 3 times to
  # delete and sleep a bit in between.
  max_tries = 3
  for i in xrange(max_tries):
    # errors is a list of tuple(function, path, excinfo).
    errors = []
    shutil.rmtree(root, onerror=lambda *args: errors.append(args))
    if not errors:
      return True
    if i == max_tries - 1:
      sys.stderr.write(
          'Failed to delete %s. The following files remain:\n' % root)
      for _, path, _ in errors:
        sys.stderr.write('- %s\n' % path)
    else:
      delay = (i+1)*2
      sys.stderr.write(
          'Failed to delete %s (%d files remaining).\n'
          '  Maybe the test has a subprocess outliving it.\n'
          '  Sleeping %d seconds.\n' %
          (root, len(errors), delay))
      time.sleep(delay)

  # The soft way was not good enough. Try the hard way. Enumerates both:
  # - all child processes from this process.
  # - processes where the main executable in inside 'root'. The reason is that
  #   the ancestry may be broken so stray grand-children processes could be
  #   undetected by the first technique.
  # This technique is not fool-proof but gets mostly there.
  def get_processes():
    processes = threading_utils.enum_processes_win()
    tree_processes = threading_utils.filter_processes_tree_win(processes)
    dir_processes = threading_utils.filter_processes_dir_win(processes, root)
    # Convert to dict to remove duplicates.
    processes = {p.ProcessId: p for p in tree_processes}
    processes.update((p.ProcessId, p) for p in dir_processes)
    processes.pop(os.getpid())
    return processes

  for i in xrange(3):
    sys.stderr.write('Enumerating processes:\n')
    processes = get_processes()
    if not processes:
      break
    for _, proc in sorted(processes.iteritems()):
      sys.stderr.write(
          '- pid %d; Handles: %d; Exe: %s; Cmd: %s\n' % (
            proc.ProcessId,
            proc.HandleCount,
            proc.ExecutablePath,
            proc.CommandLine))
    sys.stderr.write('Terminating %d processes.\n' % len(processes))
    for pid in sorted(processes):
      try:
        # Killing is asynchronous.
        os.kill(pid, 9)
        sys.stderr.write('- %d killed\n' % pid)
      except OSError:
        sys.stderr.write('- failed to kill %s\n' % pid)
    if i < 2:
      time.sleep((i+1)*2)
  else:
    processes = get_processes()
    if processes:
      sys.stderr.write('Failed to terminate processes.\n')
      raise errors[0][2][0], errors[0][2][1], errors[0][2][2]

  # Now that annoying processes in root are evicted, try again.
  errors = []
  shutil.rmtree(root, onerror=lambda *args: errors.append(args))
  if errors:
    # There's no hope.
    sys.stderr.write(
        'Failed to delete %s. The following files remain:\n' % root)
    for _, path, _ in errors:
      sys.stderr.write('- %s\n' % path)
    raise errors[0][2][0], errors[0][2][1], errors[0][2][2]
  return False


def try_remove(filepath):
  """Removes a file without crashing even if it doesn't exist."""
  try:
    # TODO(maruel): Not do it unless necessary since it slows this function
    # down.
    if sys.platform == 'win32':
      # Deleting a read-only file will fail if it is read-only.
      set_read_only(filepath, False)
    else:
      # Deleting a read-only file will fail if the directory is read-only.
      set_read_only(os.path.dirname(filepath), False)
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
  if root_dir and not is_same_filesystem(root_dir, tempfile.gettempdir()):
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

  def __init__(self, cache_dir, policies, hash_algo):
    """
    Arguments:
      cache_dir: directory where to place the cache.
      policies: cache retention policies.
      algo: hashing algorithm used.
    """
    super(DiskCache, self).__init__()
    self.cache_dir = cache_dir
    self.policies = policies
    self.hash_algo = hash_algo
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
    """Verifies an actual file is valid.

    Note that is doesn't compute the hash so it could still be corrupted if the
    file size didn't change.

    TODO(maruel): More stringent verification while keeping the check fast.
    """
    # Do the check outside the lock.
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
    # A stale broken file may remain. It is possible for the file to have write
    # access bit removed which would cause the file_write() call to fail to open
    # in write mode. Take no chance here.
    try_remove(path)
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
    # Make the file read-only in the cache.  This has a few side-effects since
    # the file node is modified, so every directory entries to this file becomes
    # read-only. It's fine here because it is a new file.
    set_read_only(path, True)
    with self._lock:
      self._add(digest, size)

  def hardlink(self, digest, dest, file_mode):
    """Hardlinks the file to |dest|.

    Note that the file permission bits are on the file node, not the directory
    entry, so changing the access bit on any of the directory entries for the
    file node will affect them all.
    """
    path = self._path(digest)
    link_file(dest, path, HARDLINK)
    if file_mode is not None:
      # Ignores all other bits.
      os.chmod(dest, file_mode & 0500)

  def _load(self):
    """Loads state of the cache from json file."""
    self._lock.assert_locked()

    if not os.path.isdir(self.cache_dir):
      os.makedirs(self.cache_dir)
    else:
      # Make sure the cache is read-only.
      # TODO(maruel): Calculate the cost and optimize the performance
      # accordingly.
      make_tree_read_only(self.cache_dir)

    # Load state of the cache.
    if os.path.isfile(self.state_file):
      try:
        self._lru = lru.LRUDict.load(self.state_file)
      except ValueError as err:
        logging.error('Failed to load cache state: %s' % (err,))
        # Don't want to keep broken state file.
        try_remove(self.state_file)

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
      if not isolateserver.is_valid_hash(filename, self.hash_algo):
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
    if sys.platform != 'win32':
      d = os.path.dirname(self.state_file)
      if os.path.isdir(d):
        # Necessary otherwise the file can't be created.
        set_read_only(d, False)
    if os.path.isfile(self.state_file):
      set_read_only(self.state_file, False)
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
      total_usage = sum(self._lru.itervalues())
      usage_percent = 0.
      if total_usage:
        usage_percent = 100. * self.policies.max_cache_size / float(total_usage)
      logging.warning(
          'Trimmed due to not enough free disk space: %.1fkb free, %.1fkb '
          'cache (%.1f%% of its maximum capacity)',
          self._free_disk / 1024.,
          total_usage / 1024.,
          usage_percent)
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
      try_remove(self._path(digest))
      self._removed.append(size)
    except OSError as e:
      logging.error('Error attempting to delete a file %s:\n%s' % (digest, e))


def change_tree_read_only(rootdir, read_only):
  """Changes the tree read-only bits according to the read_only specification.

  The flag can be 0, 1 or 2, which will affect the possibility to modify files
  and create or delete files.
  """
  if read_only == 2:
    # Files and directories (except on Windows) are marked read only. This
    # inhibits modifying, creating or deleting files in the test directory,
    # except on Windows where creating and deleting files is still possible.
    make_tree_read_only(rootdir)
  elif read_only == 1:
    # Files are marked read only but not the directories. This inhibits
    # modifying files but creating or deleting files is still possible.
    make_tree_files_read_only(rootdir)
  elif read_only in (0, None):
    # Anything can be modified. This is the default in the .isolated file
    # format.
    #
    # TODO(maruel): This is currently dangerous as long as DiskCache.touch()
    # is not yet changed to verify the hash of the content of the files it is
    # looking at, so that if a test modifies an input file, the file must be
    # deleted.
    make_tree_writeable(rootdir)
  else:
    raise ValueError(
        'change_tree_read_only(%s, %s): Unknown flag %s' %
        (rootdir, read_only, read_only))


def process_command(command, out_dir):
  """Replaces isolated specific variables in a command line."""
  filtered = []
  for arg in command:
    if '${ISOLATED_OUTDIR}' in arg:
      arg = arg.replace('${ISOLATED_OUTDIR}', out_dir).replace('/', os.sep)
    filtered.append(arg)
  return filtered


def run_tha_test(isolated_hash, storage, cache, extra_args):
  """Downloads the dependencies in the cache, hardlinks them into a temporary
  directory and runs the executable from there.

  A temporary directory is created to hold the output files. The content inside
  this directory will be uploaded back to |storage| packaged as a .isolated
  file.

  Arguments:
    isolated_hash: the sha-1 of the .isolated file that must be retrieved to
                   recreate the tree of files to run the target executable.
    storage: an isolateserver.Storage object to retrieve remote objects. This
             object has a reference to an isolateserver.StorageApi, which does
             the actual I/O.
    cache: an isolateserver.LocalCache to keep from retrieving the same objects
           constantly by caching the objects retrieved. Can be on-disk or
           in-memory.
    extra_args: optional arguments to add to the command stated in the .isolate
                file.
  """
  run_dir = make_temp_dir('run_tha_test', cache.cache_dir)
  out_dir = unicode(make_temp_dir('isolated_out', cache.cache_dir))
  result = 0
  try:
    try:
      settings = isolateserver.fetch_isolated(
          isolated_hash=isolated_hash,
          storage=storage,
          cache=cache,
          outdir=run_dir,
          require_command=True)
    except isolateserver.ConfigError:
      on_error.report(None)
      return 1

    change_tree_read_only(run_dir, settings.read_only)
    cwd = os.path.normpath(os.path.join(run_dir, settings.relative_cwd))
    command = settings.command + extra_args

    # subprocess.call doesn't consider 'cwd' when searching for executable.
    # Yet isolate can specify command relative to 'cwd'. Convert it to absolute
    # path if necessary.
    if not os.path.isabs(command[0]):
      command[0] = os.path.abspath(os.path.join(cwd, command[0]))
    command = process_command(command, out_dir)
    logging.info('Running %s, cwd=%s' % (command, cwd))

    # TODO(csharp): This should be specified somewhere else.
    # TODO(vadimsh): Pass it via 'env_vars' in manifest.
    # Add a rotating log file if one doesn't already exist.
    env = os.environ.copy()
    if MAIN_DIR:
      env.setdefault('RUN_TEST_CASES_LOG_FILE',
          os.path.join(MAIN_DIR, RUN_TEST_CASES_LOG))
    try:
      sys.stdout.flush()
      with tools.Profiler('RunTest'):
        result = subprocess.call(command, cwd=cwd, env=env)
        logging.info(
            'Command finished with exit code %d (%s)',
            result, hex(0xffffffff & result))
    except OSError:
      on_error.report('Failed to run %s; cwd=%s' % (command, cwd))
      result = 1

  finally:
    try:
      try:
        if not rmtree(run_dir):
          print >> sys.stderr, (
              'Failed to delete the temporary directory, forcibly failing the\n'
              'task because of it. No zombie process can outlive a successful\n'
              'task run and still be marked as successful. Fix your stuff.')
          result = result or 1
      except OSError:
        logging.warning('Leaking %s', run_dir)
        result = 1

      # HACK(vadimsh): On Windows rmtree(run_dir) call above has
      # a synchronization effect: it finishes only when all task child processes
      # terminate (since a running process locks *.exe file). Examine out_dir
      # only after that call completes (since child processes may
      # write to out_dir too and we need to wait for them to finish).

      # Upload out_dir and generate a .isolated file out of this directory.
      # It is only done if files were written in the directory.
      if os.listdir(out_dir):
        with tools.Profiler('ArchiveOutput'):
          results = isolateserver.archive_files_to_storage(
              storage, [out_dir], None)
        # TODO(maruel): Implement side-channel to publish this information.
        output_data = {
          'hash': results[0][0],
          'namespace': storage.namespace,
          'storage': storage.location,
        }
        sys.stdout.flush()
        print(
            '[run_isolated_out_hack]%s[/run_isolated_out_hack]' %
            tools.format_json(output_data, dense=True))

    finally:
      try:
        if os.path.isdir(out_dir) and not rmtree(out_dir):
          result = result or 1
      except OSError:
        # The error was already printed out. Report it but that's it.
        on_error.report(None)
        result = 1

  return result


def main(args):
  tools.disable_buffering()
  parser = tools.OptionParserWithLogging(
      usage='%prog <options>',
      version=__version__,
      log_file=RUN_ISOLATED_LOG_FILE)

  data_group = optparse.OptionGroup(parser, 'Data source')
  data_group.add_option(
      '-s', '--isolated',
      metavar='FILE',
      help='File/url describing what to map or run')
  data_group.add_option(
      '-H', '--hash',
      help='Hash of the .isolated to grab from the hash table')
  isolateserver.add_isolate_server_options(data_group, True)
  parser.add_option_group(data_group)

  cache_group = optparse.OptionGroup(parser, 'Cache management')
  cache_group.add_option(
      '--cache',
      default='cache',
      metavar='DIR',
      help='Cache directory, default=%default')
  cache_group.add_option(
      '--max-cache-size',
      type='int',
      metavar='NNN',
      default=20*1024*1024*1024,
      help='Trim if the cache gets larger than this value, default=%default')
  cache_group.add_option(
      '--min-free-space',
      type='int',
      metavar='NNN',
      default=2*1024*1024*1024,
      help='Trim if disk free space becomes lower than this value, '
           'default=%default')
  cache_group.add_option(
      '--max-items',
      type='int',
      metavar='NNN',
      default=100000,
      help='Trim if more than this number of items are in the cache '
           'default=%default')
  parser.add_option_group(cache_group)

  auth.add_auth_options(parser)
  options, args = parser.parse_args(args)
  auth.process_auth_options(parser, options)
  isolateserver.process_isolate_server_options(data_group, options)

  if bool(options.isolated) == bool(options.hash):
    logging.debug('One and only one of --isolated or --hash is required.')
    parser.error('One and only one of --isolated or --hash is required.')

  options.cache = os.path.abspath(options.cache)
  policies = CachePolicies(
      options.max_cache_size, options.min_free_space, options.max_items)

  # |options.cache| path may not exist until DiskCache() instance is created.
  cache = DiskCache(
      options.cache, policies, isolateserver.get_hash_algo(options.namespace))

  remote = options.isolate_server or options.indir
  if file_path.is_url(remote):
    auth.ensure_logged_in(remote)

  with isolateserver.get_storage(remote, options.namespace) as storage:
    # Hashing schemes used by |storage| and |cache| MUST match.
    assert storage.hash_algo == cache.hash_algo
    return run_tha_test(
        options.isolated or options.hash, storage, cache, args)


if __name__ == '__main__':
  # Ensure that we are always running with the correct encoding.
  fix_encoding.fix_encoding()
  sys.exit(main(sys.argv[1:]))
