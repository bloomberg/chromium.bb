#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Reads a manifest, creates a tree of hardlinks and runs the test.

Keeps a local cache.
"""

import ctypes
import hashlib
import json
import logging
import optparse
import os
import Queue
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import threading
import time
import urllib


# Types of action accepted by recreate_tree().
HARDLINK, SYMLINK, COPY = range(1, 4)

RE_IS_SHA1 = re.compile(r'^[a-fA-F0-9]{40}$')


class ConfigError(ValueError):
  """Generic failure to load a manifest."""
  pass


class MappingError(OSError):
  """Failed to recreate the tree."""
  pass


def get_flavor():
  """Returns the system default flavor. Copied from gyp/pylib/gyp/common.py."""
  flavors = {
    'cygwin': 'win',
    'win32': 'win',
    'darwin': 'mac',
    'sunos5': 'solaris',
    'freebsd7': 'freebsd',
    'freebsd8': 'freebsd',
  }
  return flavors.get(sys.platform, 'linux')


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
  shutil.copy(infile, outfile)
  read_enabled_mode = (os.stat(outfile).st_mode | stat.S_IRUSR |
                       stat.S_IRGRP | stat.S_IROTH)
  os.chmod(outfile, read_enabled_mode)


def link_file(outfile, infile, action):
  """Links a file. The type of link depends on |action|."""
  logging.debug('Mapping %s to %s' % (infile, outfile))
  if action not in (HARDLINK, SYMLINK, COPY):
    raise ValueError('Unknown mapping action %s' % action)
  if not os.path.isfile(infile):
    raise MappingError('%s is missing' % infile)
  if os.path.isfile(outfile):
    raise MappingError(
        '%s already exist; insize:%d; outsize:%d' %
        (outfile, os.stat(infile).st_size, os.stat(outfile).st_size))

  if action == COPY:
    readable_copy(outfile, infile)
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


def get_free_space(path):
  """Returns the number of free bytes."""
  if sys.platform == 'win32':
    free_bytes = ctypes.c_ulonglong(0)
    ctypes.windll.kernel32.GetDiskFreeSpaceExW(
        ctypes.c_wchar_p(path), None, None, ctypes.pointer(free_bytes))
    return free_bytes.value
  f = os.statvfs(path)
  return f.f_bfree * f.f_frsize


def make_temp_dir(prefix, root_dir):
  """Returns a temporary directory on the same file system as root_dir."""
  base_temp_dir = None
  if not is_same_filesystem(root_dir, tempfile.gettempdir()):
    base_temp_dir = os.path.dirname(root_dir)
  return tempfile.mkdtemp(prefix=prefix, dir=base_temp_dir)


def load_manifest(content):
  """Verifies the manifest is valid and loads this object with the json data.
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
          if subsubkey == 'link':
            if not isinstance(subsubvalue, basestring):
              raise ConfigError('Expected string, got %r' % subsubvalue)
          elif subsubkey == 'mode':
            if not isinstance(subsubvalue, int):
              raise ConfigError('Expected int, got %r' % subsubvalue)
          elif subsubkey == 'sha-1':
            if not RE_IS_SHA1.match(subsubvalue):
              raise ConfigError('Expected sha-1, got %r' % subsubvalue)
          elif subsubkey == 'size':
            if not isinstance(subsubvalue, int):
              raise ConfigError('Expected int, got %r' % subsubvalue)
          elif subsubkey == 'timestamp':
            if not isinstance(subsubvalue, int):
              raise ConfigError('Expected int, got %r' % subsubvalue)
          elif subsubkey == 'touched_only':
            if not isinstance(subsubvalue, bool):
              raise ConfigError('Expected bool, got %r' % subsubvalue)
          else:
            raise ConfigError('Unknown key %s' % subsubkey)
        if bool('sha-1' in subvalue) and bool('link' in subvalue):
          raise ConfigError(
              'Did not expect both \'sha-1\' and \'link\', got: %r' % subvalue)

    elif key == 'includes':
      if not isinstance(value, list):
        raise ConfigError('Expected list, got %r' % value)
      for subvalue in value:
        if not RE_IS_SHA1.match(subvalue):
          raise ConfigError('Expected sha-1, got %r' % subvalue)

    elif key == 'read_only':
      if not isinstance(value, bool):
        raise ConfigError('Expected bool, got %r' % value)

    elif key == 'relative_cwd':
      if not isinstance(value, basestring):
        raise ConfigError('Expected string, got %r' % value)

    elif key == 'os':
      if value != get_flavor():
        raise ConfigError(
            'Expected \'os\' to be \'%s\' but got \'%s\'' %
            (get_flavor(), value))

    else:
      raise ConfigError('Unknown key %s' % subkey)

  return data


def fix_python_path(cmd):
  """Returns the fixed command line to call the right python executable."""
  out = cmd[:]
  if out[0] == 'python':
    out[0] = sys.executable
  elif out[0].endswith('.py'):
    out.insert(0, sys.executable)
  return out


class Profiler(object):
  def __init__(self, name):
    self.name = name
    self.start_time = None

  def __enter__(self):
    self.start_time = time.time()
    return self

  def __exit__(self, _exc_type, _exec_value, _traceback):
    time_taken = time.time() - self.start_time
    logging.info('Profiling: Section %s took %3.3f seconds',
                 self.name, time_taken)


class Remote(object):
  """Priority based worker queue to fetch files from a content-address server.

  Supports local file system, CIFS or http remotes.

  When the priority of items is equals, works in strict FIFO mode.
  """
  # Initial and maximum number of worker threads.
  INITIAL_WORKERS = 2
  MAX_WORKERS = 16
  # Priorities.
  LOW, MED, HIGH = (1<<8, 2<<8, 3<<8)
  INTERNAL_PRIORITY_BITS = (1<<8) - 1
  RETRIES = 5

  def __init__(self, file_or_url):
    # Function to fetch a remote object.
    self._do_item = self._get_remote_fetcher(file_or_url)
    # Contains tuple(priority, index, obj, destination).
    self._queue = Queue.PriorityQueue()
    # Contains tuple(priority, index, obj).
    self._done = Queue.PriorityQueue()

    # To keep FIFO ordering in self._queue. It is assumed xrange's iterator is
    # thread-safe.
    self._next_index = xrange(0, 1<<30).__iter__().next

    # Control access to the following member.
    self._ready_lock = threading.Lock()
    # Number of threads in wait state.
    self._ready = 0

    # Control access to the following member.
    self._workers_lock = threading.Lock()
    self._workers = []
    for _ in range(self.INITIAL_WORKERS):
      self._add_worker()

  def fetch_item(self, priority, obj, dest):
    """Retrieves an object from the remote data store.

    The smaller |priority| gets fetched first.

    Thread-safe.
    """
    assert (priority & self.INTERNAL_PRIORITY_BITS) == 0
    self._fetch(priority, obj, dest)

  def get_result(self):
    """Returns the next file that was successfully fetched."""
    r = self._done.get()
    if r[0] == '-1':
      # It's an exception.
      raise r[2][0], r[2][1], r[2][2]
    return r[2]

  def _fetch(self, priority, obj, dest):
    with self._ready_lock:
      start_new_worker = not self._ready
    self._queue.put((priority, self._next_index(), obj, dest))
    if start_new_worker:
      self._add_worker()

  def _add_worker(self):
    """Add one worker thread if there isn't too many. Thread-safe."""
    with self._workers_lock:
      if len(self._workers) >= self.MAX_WORKERS:
        return False
      worker = threading.Thread(target=self._run)
      self._workers.append(worker)
    worker.daemon = True
    worker.start()

  def _run(self):
    """Worker thread loop."""
    while True:
      try:
        with self._ready_lock:
          self._ready += 1
        item = self._queue.get()
      finally:
        with self._ready_lock:
          self._ready -= 1
      if not item:
        return
      priority, index, obj, dest = item
      try:
        self._do_item(obj, dest)
      except IOError:
        # Retry a few times, lowering the priority.
        if (priority & self.INTERNAL_PRIORITY_BITS) < self.RETRIES:
          self._fetch(priority + 1, obj, dest)
          continue
        # Transfers the exception back. It has maximum priority.
        self._done.put((-1, 0, sys.exc_info()))
      except:
        # Transfers the exception back. It has maximum priority.
        self._done.put((-1, 0, sys.exc_info()))
      else:
        self._done.put((priority, index, obj))

  @staticmethod
  def _get_remote_fetcher(file_or_url):
    """Returns a object to retrieve objects from a remote."""
    if re.match(r'^https?://.+$', file_or_url):
      file_or_url = file_or_url.rstrip('/') + '/'
      def download_file(item, dest):
        # TODO(maruel): Reuse HTTP connections. The stdlib doesn't make this
        # easy.
        source = file_or_url + item
        logging.debug('download_file(%s, %s)', source, dest)
        urllib.urlretrieve(source, dest)
      return download_file

    def copy_file(item, dest):
      source = os.path.join(file_or_url, item)
      logging.debug('copy_file(%s, %s)', source, dest)
      shutil.copy(source, dest)
    return copy_file


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


class Cache(object):
  """Stateful LRU cache.

  Saves its state as json file.
  """
  STATE_FILE = 'state.json'

  def __init__(self, cache_dir, remote, policies):
    """
    Arguments:
    - cache_dir: Directory where to place the cache.
    - remote: Remote where to fetch items from.
    - policies: cache retention policies.
    """
    self.cache_dir = cache_dir
    self.remote = remote
    self.policies = policies
    self.state_file = os.path.join(cache_dir, self.STATE_FILE)
    # The tuple(file, size) are kept as an array in a LRU style. E.g.
    # self.state[0] is the oldest item.
    self.state = []
    # A lookup map to speed up searching.
    self._lookup = {}
    self._dirty = False

    # Items currently being fetched. Keep it local to reduce lock contention.
    self._pending_queue = set()

    # Profiling values.
    self._added = []
    self._removed = []
    self._free_disk = 0

    if not os.path.isdir(self.cache_dir):
      os.makedirs(self.cache_dir)
    if os.path.isfile(self.state_file):
      try:
        self.state = json.load(open(self.state_file, 'r'))
      except (IOError, ValueError), e:
        # Too bad. The file will be overwritten and the cache cleared.
        logging.error(
            'Broken state file %s, ignoring.\n%s' % (self.STATE_FILE, e))
      if (not isinstance(self.state, list) or
          not all(
            isinstance(i, (list, tuple)) and len(i) == 2 for i in self.state)):
        # Discard.
        self.state = []
        self._dirty = True

    # Ensure that all files listed in the state still exist and add new ones.
    previous = set(filename for filename, _ in self.state)
    if len(previous) != len(self.state):
      logging.warn('Cache state is corrupted')
      self._dirty = True
      self.state = []
    else:
      added = 0
      for filename in os.listdir(self.cache_dir):
        if filename == self.STATE_FILE:
          continue
        if filename in previous:
          previous.remove(filename)
          continue
        # An untracked file.
        self._dirty = True
        if not RE_IS_SHA1.match(filename):
          logging.warn('Removing unknown file %s from cache', filename)
          os.remove(self.path(filename))
        else:
          # Insert as the oldest file. It will be deleted eventually if not
          # accessed.
          self._add(filename, False)
          added += 1
      if added:
        logging.warn('Added back %d unknown files', added)
      self.state = [
        (filename, size) for filename, size in self.state
        if filename not in previous
      ]
      self._update_lookup()

    with Profiler('SetupTrimming'):
      self.trim()

  def __enter__(self):
    return self

  def __exit__(self, _exc_type, _exec_value, _traceback):
    with Profiler('CleanupTrimming'):
      self.trim()

    logging.info(
        '%4d (%7dkb) added', len(self._added), sum(self._added) / 1024)
    logging.info(
        '%4d (%7dkb) current',
        len(self.state),
        sum(i[1] for i in self.state) / 1024)
    logging.info(
        '%4d (%7dkb) removed', len(self._removed), sum(self._removed) / 1024)
    logging.info('%7dkb free', self._free_disk / 1024)

  def remove_lru_file(self):
    """Removes the last recently used file."""
    try:
      filename, size = self.state.pop(0)
      del self._lookup[filename]
      self._removed.append(size)
      os.remove(self.path(filename))
      self._dirty = True
    except OSError as e:
      logging.error('Error attempting to delete a file\n%s' % e)

  def trim(self):
    """Trims anything we don't know, make sure enough free space exists."""
    # Ensure maximum cache size.
    if self.policies.max_cache_size and self.state:
      while sum(i[1] for i in self.state) > self.policies.max_cache_size:
        self.remove_lru_file()

    # Ensure maximum number of items in the cache.
    if self.policies.max_items and self.state:
      while len(self.state) > self.policies.max_items:
        self.remove_lru_file()

    # Ensure enough free space.
    self._free_disk = get_free_space(self.cache_dir)
    while (
        self.policies.min_free_space and
        self.state and
        self._free_disk < self.policies.min_free_space):
      self.remove_lru_file()
      self._free_disk = get_free_space(self.cache_dir)

    self.save()

  def retrieve(self, priority, item):
    """Retrieves a file from the remote, if not already cached, and adds it to
    the cache.
    """
    assert not '/' in item
    path = self.path(item)
    index = self._lookup.get(item)
    if index is None:
      if item in self._pending_queue:
        # Already pending. The same object could be referenced multiple times.
        return
      self.remote.fetch_item(priority, item, path)
      self._pending_queue.add(item)
    else:
      if index != len(self.state) - 1:
        # Was already in cache. Update it's LRU value by putting it at the end.
        self.state.append(self.state.pop(index))
        self._dirty = True
        self._update_lookup()

  def add(self, filepath, obj):
    """Forcibly adds a file to the cache."""
    if not obj in self._lookup:
      link_file(self.path(obj), filepath, HARDLINK)
      self._add(obj, True)

  def path(self, item):
    """Returns the path to one item."""
    return os.path.join(self.cache_dir, item)

  def save(self):
    """Saves the LRU ordering."""
    json.dump(self.state, open(self.state_file, 'wb'), separators=(',',':'))

  def wait_for(self, items):
    """Starts a loop that waits for at least one of |items| to be retrieved.

    Returns the first item retrieved.
    """
    # Flush items already present.
    for item in items:
      if item in self._lookup:
        return item

    assert all(i in self._pending_queue for i in items), (
        items, self._pending_queue)
    # Note that:
    #   len(self._pending_queue) ==
    #   ( len(self.remote._workers) - self.remote._ready +
    #     len(self._remote._queue) + len(self._remote.done))
    # There is no lock-free way to verify that.
    while self._pending_queue:
      item = self.remote.get_result()
      self._pending_queue.remove(item)
      self._add(item, True)
      if item in items:
        return item

  def _add(self, item, at_end):
    """Adds an item in the internal state.

    If |at_end| is False, self._lookup becomes inconsistent and
    self._update_lookup() must be called.
    """
    size = os.stat(self.path(item)).st_size
    self._added.append(size)
    if at_end:
      self.state.append((item, size))
      self._lookup[item] = len(self.state) - 1
    else:
      self.state.insert(0, (item, size))
    self._dirty = True

  def _update_lookup(self):
    self._lookup = dict(
        (filename, index) for index, (filename, _) in enumerate(self.state))



class Manifest(object):
  """Represents a single parsed manifest, e.g. a .results file."""
  def __init__(self, obj_hash):
    """|obj_hash| is really the sha-1 of the file."""
    logging.debug('Manifest(%s)' % obj_hash)
    self.obj_hash = obj_hash
    # Set once all the left-side of the tree is parsed. 'Tree' here means the
    # manifest and all the manifest recursively included by it with 'includes'
    # key. The order of each manifest sha-1 in 'includes' is important, as the
    # later ones are not processed until the firsts are retrieved and read.
    self.can_fetch = False

    # Raw data.
    self.data = {}
    # A Manifest instance, one per object in self.includes.
    self.children = []

    # Set once the manifest is loaded.
    self._manifest_parsed = False
    # Set once the files are fetched.
    self.files_fetched = False

  def load(self, content):
    """Verifies the manifest is valid and loads this object with the json data.
    """
    logging.debug('Manifest.load(%s)' % self.obj_hash)
    assert not self._manifest_parsed
    self.data = load_manifest(content)
    self.children = [Manifest(i) for i in self.data.get('includes', [])]
    self._manifest_parsed = True

  def fetch_files(self, cache, files):
    """Adds files in this manifest not present in files dictionary.

    Preemptively request files.

    Note that |files| is modified by this function.
    """
    assert self.can_fetch
    if not self._manifest_parsed or self.files_fetched:
      return
    logging.debug('fetch_files(%s)' % self.obj_hash)
    for filepath, properties in self.data.get('files', {}).iteritems():
      # Root manifest has priority on the files being mapped. In particular,
      # overriden files must not be fetched.
      if filepath not in files:
        files[filepath] = properties
        if 'sha-1' in properties:
          # Preemptively request files.
          logging.debug('fetching %s' % filepath)
          cache.retrieve(Remote.MED, properties['sha-1'])
    self.files_fetched = True


class Settings(object):
  """Results of a completely parsed manifest."""
  def __init__(self):
    self.command = []
    self.files = {}
    self.read_only = None
    self.relative_cwd = None
    # The main manifest.
    self.root = None
    logging.debug('Settings')

  def load(self, cache, root_manifest_hash):
    """Loads the manifest and all the included manifests asynchronously.

    It enables support for included manifest. They are processed in strict order
    but fetched asynchronously from the cache. This is important so that a file
    in an included manifest that is overridden by an embedding manifest is not
    fetched neededlessly. The includes are fetched in one pass and the files are
    fetched as soon as all the manifests on the left-side of the tree were
    fetched.

    The prioritization is very important here for nested manifests. 'includes'
    have the highest priority and the algorithm is optimized for both deep and
    wide manifests. A deep one is a long link of manifest referenced one at a
    time by one item in 'includes'. A wide one has a large number of 'includes'
    in a single manifest. 'left' is defined as an included manifest earlier in
    the 'includes' list. So the order of the elements in 'includes' is
    important.
    """
    self.root = Manifest(root_manifest_hash)
    cache.retrieve(Remote.HIGH, root_manifest_hash)
    pending = {root_manifest_hash: self.root}
    # Keeps the list of retrieved items to refuse recursive includes.
    retrieved = [root_manifest_hash]

    def update_self(node):
      node.fetch_files(cache, self.files)
      # Grabs properties.
      if not self.command and node.data.get('command'):
        self.command = node.data['command']
      if self.read_only is None and node.data.get('read_only') is not None:
        self.read_only = node.data['read_only']
      if (self.relative_cwd is None and
          node.data.get('relative_cwd') is not None):
        self.relative_cwd = node.data['relative_cwd']

    def traverse_tree(node):
      if node.can_fetch:
        if not node.files_fetched:
          update_self(node)
        will_break = False
        for i in node.children:
          if not i.can_fetch:
            if will_break:
              break
            # Automatically mark the first one as fetcheable.
            i.can_fetch = True
            will_break = True
          traverse_tree(i)

    while pending:
      item_hash = cache.wait_for(pending)
      item = pending.pop(item_hash)
      item.load(open(cache.path(item_hash), 'r').read())
      if item_hash == root_manifest_hash:
        # It's the root item.
        item.can_fetch = True

      for new_child in item.children:
        h = new_child.obj_hash
        if h in retrieved:
          raise ConfigError('Manifest %s is retrieved recursively' % h)
        pending[h] = new_child
        cache.retrieve(Remote.HIGH, h)

      # Traverse the whole tree to see if files can now be fetched.
      traverse_tree(self.root)
    def check(n):
      return all(check(x) for x in n.children) and n.files_fetched
    assert check(self.root)
    self.relative_cwd = self.relative_cwd or ''
    self.read_only = self.read_only or False


def run_tha_test(manifest_hash, cache_dir, remote, policies):
  """Downloads the dependencies in the cache, hardlinks them into a temporary
  directory and runs the executable.
  """
  settings = Settings()
  with Cache(cache_dir, Remote(remote), policies) as cache:
    outdir = make_temp_dir('run_tha_test', cache_dir)
    try:
      # Initiate all the files download.
      with Profiler('GetManifests') as _prof:
        # Optionally support local files.
        if not RE_IS_SHA1.match(manifest_hash):
          # Adds it in the cache. While not strictly necessary, this simplifies
          # the rest.
          h = hashlib.sha1(open(manifest_hash, 'r').read()).hexdigest()
          cache.add(manifest_hash, h)
          manifest_hash = h
        settings.load(cache, manifest_hash)

      if not settings.command:
        print >> sys.stderr, 'No command to run'
        return 1

      with Profiler('GetRest') as _prof:
        logging.debug('Creating directories')
        # Creates the tree of directories to create.
        directories = set(os.path.dirname(f) for f in settings.files)
        for item in list(directories):
          while item:
            directories.add(item)
            item = os.path.dirname(item)
        for d in sorted(directories):
          if d:
            os.mkdir(os.path.join(outdir, d))

        # Creates the links if necessary.
        for filepath, properties in settings.files.iteritems():
          if 'link' not in properties:
            continue
          outfile = os.path.join(outdir, filepath)
          os.symlink(properties['link'], outfile)
          if 'mode' in properties:
            # It's not set on Windows.
            os.chmod(outfile, properties['mode'])

        # Remaining files to be processed.
        # Note that files could still be not be downloaded yet here.
        remaining = dict()
        for filepath, props in settings.files.iteritems():
          if 'sha-1' in props:
            remaining.setdefault(props['sha-1'], []).append((filepath, props))

        # Do bookkeeping while files are being downloaded in the background.
        cwd = os.path.join(outdir, settings.relative_cwd)
        if not os.path.isdir(cwd):
          os.makedirs(cwd)
        cmd = settings.command[:]
        # Ensure paths are correctly separated on windows.
        cmd[0] = cmd[0].replace('/', os.path.sep)
        cmd = fix_python_path(cmd)

        # Now block on the remaining files to be downloaded and mapped.
        while remaining:
          obj = cache.wait_for(remaining)
          for filepath, properties in remaining.pop(obj):
            outfile = os.path.join(outdir, filepath)
            link_file(outfile, cache.path(obj), HARDLINK)
            if 'mode' in properties:
              # It's not set on Windows.
              os.chmod(outfile, properties['mode'])

      if settings.read_only:
        make_writable(outdir, True)
      logging.info('Running %s, cwd=%s' % (cmd, cwd))
      try:
        with Profiler('RunTest') as _prof:
          return subprocess.call(cmd, cwd=cwd)
      except OSError:
        print >> sys.stderr, 'Failed to run %s; cwd=%s' % (cmd, cwd)
        raise
    finally:
      rmtree(outdir)


def main():
  parser = optparse.OptionParser(
      usage='%prog <options>', description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-v', '--verbose', action='count', default=0, help='Use multiple times')
  parser.add_option('--no-run', action='store_true', help='Skip the run part')

  group = optparse.OptionGroup(parser, 'Data source')
  group.add_option(
      '-m', '--manifest',
      metavar='FILE',
      help='File/url describing what to map or run')
  group.add_option(
      '-H', '--hash',
      help='Hash of the manifest to grab from the hash table')
  parser.add_option_group(group)

  group.add_option(
      '-r', '--remote', metavar='URL', help='Remote where to get the items')
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
      default=1*1024*1024*1024,
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
  level = [logging.ERROR, logging.INFO, logging.DEBUG][min(2, options.verbose)]
  logging.basicConfig(
      level=level,
      format='%(levelname)5s %(module)15s(%(lineno)3d): %(message)s')

  if bool(options.manifest) == bool(options.hash):
    parser.error('One and only one of --manifest or --hash is required.')
  if not options.remote:
    parser.error('--remote is required.')
  if args:
    parser.error('Unsupported args %s' % ' '.join(args))

  policies = CachePolicies(
      options.max_cache_size, options.min_free_space, options.max_items)
  try:
    return run_tha_test(
        options.manifest or options.hash,
        os.path.abspath(options.cache),
        options.remote,
        policies)
  except (ConfigError, MappingError), e:
    print >> sys.stderr, str(e)
    return 1


if __name__ == '__main__':
  sys.exit(main())
