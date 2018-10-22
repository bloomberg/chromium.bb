#!/usr/bin/env python
# Copyright 2013 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

"""Archives a set of files or directories to an Isolate Server."""

__version__ = '0.8.5'

import errno
import functools
import logging
import optparse
import os
import re
import signal
import stat
import sys
import tarfile
import tempfile
import time
import zlib

from third_party import colorama
from third_party.depot_tools import fix_encoding
from third_party.depot_tools import subcommand

from utils import file_path
from utils import fs
from utils import logging_utils
from utils import net
from utils import on_error
from utils import subprocess42
from utils import threading_utils
from utils import tools

import auth
import isolated_format
import isolate_storage
import local_caching


# Version of isolate protocol passed to the server in /handshake request.
ISOLATE_PROTOCOL_VERSION = '1.0'


# Maximum expected delay (in seconds) between successive file fetches or uploads
# in Storage. If it takes longer than that, a deadlock might be happening
# and all stack frames for all threads are dumped to log.
DEADLOCK_TIMEOUT = 5 * 60


# The number of files to check the isolate server per /pre-upload query.
# All files are sorted by likelihood of a change in the file content
# (currently file size is used to estimate this: larger the file -> larger the
# possibility it has changed). Then first ITEMS_PER_CONTAINS_QUERIES[0] files
# are taken and send to '/pre-upload', then next ITEMS_PER_CONTAINS_QUERIES[1],
# and so on. Numbers here is a trade-off; the more per request, the lower the
# effect of HTTP round trip latency and TCP-level chattiness. On the other hand,
# larger values cause longer lookups, increasing the initial latency to start
# uploading, which is especially an issue for large files. This value is
# optimized for the "few thousands files to look up with minimal number of large
# files missing" case.
ITEMS_PER_CONTAINS_QUERIES = (20, 20, 50, 50, 50, 100)


# A list of already compressed extension types that should not receive any
# compression before being uploaded.
ALREADY_COMPRESSED_TYPES = [
    '7z', 'avi', 'cur', 'gif', 'h264', 'jar', 'jpeg', 'jpg', 'mp4', 'pdf',
    'png', 'wav', 'zip',
]


# The delay (in seconds) to wait between logging statements when retrieving
# the required files. This is intended to let the user (or buildbot) know that
# the program is still running.
DELAY_BETWEEN_UPDATES_IN_SECS = 30


DEFAULT_BLACKLIST = (
  # Temporary vim or python files.
  r'^.+\.(?:pyc|swp)$',
  # .git or .svn directory.
  r'^(?:.+' + re.escape(os.path.sep) + r'|)\.(?:git|svn)$',
)


class Error(Exception):
  """Generic runtime error."""
  pass


class Aborted(Error):
  """Operation aborted."""
  pass


class AlreadyExists(Error):
  """File already exists."""


def file_read(path, chunk_size=isolated_format.DISK_FILE_CHUNK, offset=0):
  """Yields file content in chunks of |chunk_size| starting from |offset|."""
  with fs.open(path, 'rb') as f:
    if offset:
      f.seek(offset)
    while True:
      data = f.read(chunk_size)
      if not data:
        break
      yield data


def fileobj_path(fileobj):
  """Return file system path for file like object or None.

  The returned path is guaranteed to exist and can be passed to file system
  operations like copy.
  """
  name = getattr(fileobj, 'name', None)
  if name is None:
    return

  # If the file like object was created using something like open("test.txt")
  # name will end up being a str (such as a function outside our control, like
  # the standard library). We want all our paths to be unicode objects, so we
  # decode it.
  if not isinstance(name, unicode):
    # We incorrectly assume that UTF-8 is used everywhere.
    name = name.decode('utf-8')

  # fs.exists requires an absolute path, otherwise it will fail with an
  # assertion error.
  if not os.path.isabs(name):
      return

  if fs.exists(name):
    return name


# TODO(tansell): Replace fileobj_copy with shutil.copyfileobj once proper file
# wrappers have been created.
def fileobj_copy(
    dstfileobj, srcfileobj, size=-1,
    chunk_size=isolated_format.DISK_FILE_CHUNK):
  """Copy data from srcfileobj to dstfileobj.

  Providing size means exactly that amount of data will be copied (if there
  isn't enough data, an IOError exception is thrown). Otherwise all data until
  the EOF marker will be copied.
  """
  if size == -1 and hasattr(srcfileobj, 'tell'):
    if srcfileobj.tell() != 0:
      raise IOError('partial file but not using size')

  written = 0
  while written != size:
    readsize = chunk_size
    if size > 0:
      readsize = min(readsize, size-written)
    data = srcfileobj.read(readsize)
    if not data:
      if size == -1:
        break
      raise IOError('partial file, got %s, wanted %s' % (written, size))
    dstfileobj.write(data)
    written += len(data)


def putfile(srcfileobj, dstpath, file_mode=None, size=-1, use_symlink=False):
  """Put srcfileobj at the given dstpath with given mode.

  The function aims to do this as efficiently as possible while still allowing
  any possible file like object be given.

  Creating a tree of hardlinks has a few drawbacks:
  - tmpfs cannot be used for the scratch space. The tree has to be on the same
    partition as the cache.
  - involves a write to the inode, which advances ctime, cause a metadata
    writeback (causing disk seeking).
  - cache ctime cannot be used to detect modifications / corruption.
  - Some file systems (NTFS) have a 64k limit on the number of hardlink per
    partition. This is why the function automatically fallbacks to copying the
    file content.
  - /proc/sys/fs/protected_hardlinks causes an additional check to ensure the
    same owner is for all hardlinks.
  - Anecdotal report that ext2 is known to be potentially faulty on high rate
    of hardlink creation.

  Creating a tree of symlinks has a few drawbacks:
  - Tasks running the equivalent of os.path.realpath() will get the naked path
    and may fail.
  - Windows:
    - Symlinks are reparse points:
      https://msdn.microsoft.com/library/windows/desktop/aa365460.aspx
      https://msdn.microsoft.com/library/windows/desktop/aa363940.aspx
    - Symbolic links are Win32 paths, not NT paths.
      https://googleprojectzero.blogspot.com/2016/02/the-definitive-guide-on-win32-to-nt.html
    - Symbolic links are supported on Windows 7 and later only.
    - SeCreateSymbolicLinkPrivilege is needed, which is not present by
      default.
    - SeCreateSymbolicLinkPrivilege is *stripped off* by UAC when a restricted
      RID is present in the token;
      https://msdn.microsoft.com/en-us/library/bb530410.aspx
  """
  srcpath = fileobj_path(srcfileobj)
  if srcpath and size == -1:
    readonly = file_mode is None or (
        file_mode & (stat.S_IWUSR | stat.S_IWGRP | stat.S_IWOTH))

    if readonly:
      # If the file is read only we can link the file
      if use_symlink:
        link_mode = file_path.SYMLINK_WITH_FALLBACK
      else:
        link_mode = file_path.HARDLINK_WITH_FALLBACK
    else:
      # If not read only, we must copy the file
      link_mode = file_path.COPY

    file_path.link_file(dstpath, srcpath, link_mode)
  else:
    # Need to write out the file
    with fs.open(dstpath, 'wb') as dstfileobj:
      fileobj_copy(dstfileobj, srcfileobj, size)

  assert fs.exists(dstpath)

  # file_mode of 0 is actually valid, so need explicit check.
  if file_mode is not None:
    fs.chmod(dstpath, file_mode)


def zip_compress(content_generator, level=7):
  """Reads chunks from |content_generator| and yields zip compressed chunks."""
  compressor = zlib.compressobj(level)
  for chunk in content_generator:
    compressed = compressor.compress(chunk)
    if compressed:
      yield compressed
  tail = compressor.flush(zlib.Z_FINISH)
  if tail:
    yield tail


def zip_decompress(
    content_generator, chunk_size=isolated_format.DISK_FILE_CHUNK):
  """Reads zipped data from |content_generator| and yields decompressed data.

  Decompresses data in small chunks (no larger than |chunk_size|) so that
  zip bomb file doesn't cause zlib to preallocate huge amount of memory.

  Raises IOError if data is corrupted or incomplete.
  """
  decompressor = zlib.decompressobj()
  compressed_size = 0
  try:
    for chunk in content_generator:
      compressed_size += len(chunk)
      data = decompressor.decompress(chunk, chunk_size)
      if data:
        yield data
      while decompressor.unconsumed_tail:
        data = decompressor.decompress(decompressor.unconsumed_tail, chunk_size)
        if data:
          yield data
    tail = decompressor.flush()
    if tail:
      yield tail
  except zlib.error as e:
    raise IOError(
        'Corrupted zip stream (read %d bytes) - %s' % (compressed_size, e))
  # Ensure all data was read and decompressed.
  if decompressor.unused_data or decompressor.unconsumed_tail:
    raise IOError('Not all data was decompressed')


def get_zip_compression_level(filename):
  """Given a filename calculates the ideal zip compression level to use."""
  file_ext = os.path.splitext(filename)[1].lower()
  # TODO(csharp): Profile to find what compression level works best.
  return 0 if file_ext in ALREADY_COMPRESSED_TYPES else 7


def create_directories(base_directory, files):
  """Creates the directory structure needed by the given list of files."""
  logging.debug('create_directories(%s, %d)', base_directory, len(files))
  # Creates the tree of directories to create.
  directories = set(os.path.dirname(f) for f in files)
  for item in list(directories):
    while item:
      directories.add(item)
      item = os.path.dirname(item)
  for d in sorted(directories):
    if d:
      abs_d = os.path.join(base_directory, d)
      if not fs.isdir(abs_d):
        fs.mkdir(abs_d)


def create_symlinks(base_directory, files):
  """Creates any symlinks needed by the given set of files."""
  for filepath, properties in files:
    if 'l' not in properties:
      continue
    if sys.platform == 'win32':
      # TODO(maruel): Create symlink via the win32 api.
      logging.warning('Ignoring symlink %s', filepath)
      continue
    outfile = os.path.join(base_directory, filepath)
    try:
      os.symlink(properties['l'], outfile)  # pylint: disable=E1101
    except OSError as e:
      if e.errno == errno.EEXIST:
        raise AlreadyExists('File %s already exists.' % outfile)
      raise


class FileItem(isolate_storage.Item):
  """A file to push to Storage.

  Its digest and size may be provided in advance, if known. Otherwise they will
  be derived from the file content.
  """

  def __init__(self, path, digest=None, size=None, high_priority=False):
    super(FileItem, self).__init__(
        digest,
        size if size is not None else fs.stat(path).st_size,
        high_priority)
    self.path = path
    self.compression_level = get_zip_compression_level(path)

  def content(self):
    return file_read(self.path)


class BufferItem(isolate_storage.Item):
  """A byte buffer to push to Storage."""

  def __init__(self, buf, high_priority=False):
    super(BufferItem, self).__init__(None, len(buf), high_priority)
    self.buffer = buf

  def content(self):
    return [self.buffer]


class Storage(object):
  """Efficiently downloads or uploads large set of files via StorageApi.

  Implements compression support, parallel 'contains' checks, parallel uploads
  and more.

  Works only within single namespace (and thus hashing algorithm and compression
  scheme are fixed).

  Spawns multiple internal threads. Thread safe, but not fork safe. Modifies
  signal handlers table to handle Ctrl+C.
  """

  def __init__(self, storage_api):
    self._storage_api = storage_api
    self._use_zip = isolated_format.is_namespace_with_compression(
        storage_api.namespace) and not storage_api.internal_compression
    self._hash_algo = isolated_format.get_hash_algo(storage_api.namespace)
    self._cpu_thread_pool = None
    self._net_thread_pool = None
    self._aborted = False
    self._prev_sig_handlers = {}

  @property
  def hash_algo(self):
    """Hashing algorithm used to name files in storage based on their content.

    Defined by |namespace|. See also isolated_format.get_hash_algo().
    """
    return self._hash_algo

  @property
  def location(self):
    """URL of the backing store that this class is using."""
    return self._storage_api.location

  @property
  def namespace(self):
    """Isolate namespace used by this storage.

    Indirectly defines hashing scheme and compression method used.
    """
    return self._storage_api.namespace

  @property
  def cpu_thread_pool(self):
    """ThreadPool for CPU-bound tasks like zipping."""
    if self._cpu_thread_pool is None:
      threads = max(threading_utils.num_processors(), 2)
      if sys.maxsize <= 2L**32:
        # On 32 bits userland, do not try to use more than 16 threads.
        threads = min(threads, 16)
      self._cpu_thread_pool = threading_utils.ThreadPool(2, threads, 0, 'zip')
    return self._cpu_thread_pool

  @property
  def net_thread_pool(self):
    """AutoRetryThreadPool for IO-bound tasks, retries IOError."""
    if self._net_thread_pool is None:
      self._net_thread_pool = threading_utils.IOAutoRetryThreadPool()
    return self._net_thread_pool

  def close(self):
    """Waits for all pending tasks to finish."""
    logging.info('Waiting for all threads to die...')
    if self._cpu_thread_pool:
      self._cpu_thread_pool.join()
      self._cpu_thread_pool.close()
      self._cpu_thread_pool = None
    if self._net_thread_pool:
      self._net_thread_pool.join()
      self._net_thread_pool.close()
      self._net_thread_pool = None
    logging.info('Done.')

  def abort(self):
    """Cancels any pending or future operations."""
    # This is not strictly theadsafe, but in the worst case the logging message
    # will be printed twice. Not a big deal. In other places it is assumed that
    # unprotected reads and writes to _aborted are serializable (it is true
    # for python) and thus no locking is used.
    if not self._aborted:
      logging.warning('Aborting... It can take a while.')
      self._aborted = True

  def __enter__(self):
    """Context manager interface."""
    assert not self._prev_sig_handlers, self._prev_sig_handlers
    for s in (signal.SIGINT, signal.SIGTERM):
      self._prev_sig_handlers[s] = signal.signal(s, lambda *_args: self.abort())
    return self

  def __exit__(self, _exc_type, _exc_value, _traceback):
    """Context manager interface."""
    self.close()
    while self._prev_sig_handlers:
      s, h = self._prev_sig_handlers.popitem()
      signal.signal(s, h)
    return False

  def upload_items(self, items):
    """Uploads a bunch of items to the isolate server.

    It figures out what items are missing from the server and uploads only them.

    Arguments:
      items: list of isolate_storage.Item instances that represents data to
             upload.

    Returns:
      List of items that were uploaded. All other items are already there.
    """
    logging.info('upload_items(items=%d)', len(items))

    # Ensure all digests are calculated.
    for item in items:
      item.prepare(self._hash_algo)

    # For each digest keep only first isolate_storage.Item that matches it. All
    # other items are just indistinguishable copies from the point of view of
    # isolate server (it doesn't care about paths at all, only content and
    # digests).
    seen = {}
    duplicates = 0
    for item in items:
      if seen.setdefault(item.digest, item) is not item:
        duplicates += 1
    items = seen.values()
    if duplicates:
      logging.info('Skipped %d files with duplicated content', duplicates)

    # Enqueue all upload tasks.
    missing = set()
    uploaded = []
    channel = threading_utils.TaskChannel()
    for missing_item, push_state in self.get_missing_items(items):
      missing.add(missing_item)
      self.async_push(channel, missing_item, push_state)

    # No need to spawn deadlock detector thread if there's nothing to upload.
    if missing:
      with threading_utils.DeadlockDetector(DEADLOCK_TIMEOUT) as detector:
        # Wait for all started uploads to finish.
        while len(uploaded) != len(missing):
          detector.ping()
          item = channel.pull()
          uploaded.append(item)
          logging.debug(
              'Uploaded %d / %d: %s', len(uploaded), len(missing), item.digest)
    logging.info('All files are uploaded')

    # Print stats.
    total = len(items)
    total_size = sum(f.size for f in items)
    logging.info(
        'Total:      %6d, %9.1fkb',
        total,
        total_size / 1024.)
    cache_hit = set(items) - missing
    cache_hit_size = sum(f.size for f in cache_hit)
    logging.info(
        'cache hit:  %6d, %9.1fkb, %6.2f%% files, %6.2f%% size',
        len(cache_hit),
        cache_hit_size / 1024.,
        len(cache_hit) * 100. / total,
        cache_hit_size * 100. / total_size if total_size else 0)
    cache_miss = missing
    cache_miss_size = sum(f.size for f in cache_miss)
    logging.info(
        'cache miss: %6d, %9.1fkb, %6.2f%% files, %6.2f%% size',
        len(cache_miss),
        cache_miss_size / 1024.,
        len(cache_miss) * 100. / total,
        cache_miss_size * 100. / total_size if total_size else 0)

    return uploaded

  def async_push(self, channel, item, push_state):
    """Starts asynchronous push to the server in a parallel thread.

    Can be used only after |item| was checked for presence on a server with
    'get_missing_items' call. 'get_missing_items' returns |push_state| object
    that contains storage specific information describing how to upload
    the item (for example in case of cloud storage, it is signed upload URLs).

    Arguments:
      channel: TaskChannel that receives back |item| when upload ends.
      item: item to upload as instance of isolate_storage.Item class.
      push_state: push state returned by 'get_missing_items' call for |item|.

    Returns:
      None, but |channel| later receives back |item| when upload ends.
    """
    # Thread pool task priority.
    priority = (
        threading_utils.PRIORITY_HIGH if item.high_priority
        else threading_utils.PRIORITY_MED)

    def push(content):
      """Pushes an isolate_storage.Item and returns it to |channel|."""
      if self._aborted:
        raise Aborted()
      item.prepare(self._hash_algo)
      self._storage_api.push(item, push_state, content)
      return item

    # If zipping is not required, just start a push task. Don't pass 'content'
    # so that it can create a new generator when it retries on failures.
    if not self._use_zip:
      self.net_thread_pool.add_task_with_channel(channel, priority, push, None)
      return

    # If zipping is enabled, zip in a separate thread.
    def zip_and_push():
      # TODO(vadimsh): Implement streaming uploads. Before it's done, assemble
      # content right here. It will block until all file is zipped.
      try:
        if self._aborted:
          raise Aborted()
        stream = zip_compress(item.content(), item.compression_level)
        data = ''.join(stream)
      except Exception as exc:
        logging.error('Failed to zip \'%s\': %s', item, exc)
        channel.send_exception()
        return
      # Pass '[data]' explicitly because the compressed data is not same as the
      # one provided by 'item'. Since '[data]' is a list, it can safely be
      # reused during retries.
      self.net_thread_pool.add_task_with_channel(
          channel, priority, push, [data])
    self.cpu_thread_pool.add_task(priority, zip_and_push)

  def push(self, item, push_state):
    """Synchronously pushes a single item to the server.

    If you need to push many items at once, consider using 'upload_items' or
    'async_push' with instance of TaskChannel.

    Arguments:
      item: item to upload as instance of isolate_storage.Item class.
      push_state: push state returned by 'get_missing_items' call for |item|.

    Returns:
      Pushed item (same object as |item|).
    """
    channel = threading_utils.TaskChannel()
    with threading_utils.DeadlockDetector(DEADLOCK_TIMEOUT):
      self.async_push(channel, item, push_state)
      pushed = channel.pull()
      assert pushed is item
    return item

  def async_fetch(self, channel, priority, digest, size, sink):
    """Starts asynchronous fetch from the server in a parallel thread.

    Arguments:
      channel: TaskChannel that receives back |digest| when download ends.
      priority: thread pool task priority for the fetch.
      digest: hex digest of an item to download.
      size: expected size of the item (after decompression).
      sink: function that will be called as sink(generator).
    """
    def fetch():
      try:
        # Prepare reading pipeline.
        stream = self._storage_api.fetch(digest, size, 0)
        if self._use_zip:
          stream = zip_decompress(stream, isolated_format.DISK_FILE_CHUNK)
        # Run |stream| through verifier that will assert its size.
        verifier = FetchStreamVerifier(stream, self._hash_algo, digest, size)
        # Verified stream goes to |sink|.
        sink(verifier.run())
      except Exception as err:
        logging.error('Failed to fetch %s: %s', digest, err)
        raise
      return digest

    # Don't bother with zip_thread_pool for decompression. Decompression is
    # really fast and most probably IO bound anyway.
    self.net_thread_pool.add_task_with_channel(channel, priority, fetch)

  def get_missing_items(self, items):
    """Yields items that are missing from the server.

    Issues multiple parallel queries via StorageApi's 'contains' method.

    Arguments:
      items: a list of isolate_storage.Item objects to check.

    Yields:
      For each missing item it yields a pair (item, push_state), where:
        * item - isolate_storage.Item object that is missing (one of |items|).
        * push_state - opaque object that contains storage specific information
            describing how to upload the item (for example in case of cloud
            storage, it is signed upload URLs). It can later be passed to
            'async_push'.
    """
    channel = threading_utils.TaskChannel()
    pending = 0

    # Ensure all digests are calculated.
    for item in items:
      item.prepare(self._hash_algo)

    def contains(batch):
      if self._aborted:
        raise Aborted()
      return self._storage_api.contains(batch)

    # Enqueue all requests.
    for batch in batch_items_for_check(items):
      self.net_thread_pool.add_task_with_channel(
          channel, threading_utils.PRIORITY_HIGH, contains, batch)
      pending += 1

    # Yield results as they come in.
    for _ in xrange(pending):
      for missing_item, push_state in channel.pull().iteritems():
        yield missing_item, push_state


def batch_items_for_check(items):
  """Splits list of items to check for existence on the server into batches.

  Each batch corresponds to a single 'exists?' query to the server via a call
  to StorageApi's 'contains' method.

  Arguments:
    items: a list of isolate_storage.Item objects.

  Yields:
    Batches of items to query for existence in a single operation,
    each batch is a list of isolate_storage.Item objects.
  """
  batch_count = 0
  batch_size_limit = ITEMS_PER_CONTAINS_QUERIES[0]
  next_queries = []
  for item in sorted(items, key=lambda x: x.size, reverse=True):
    next_queries.append(item)
    if len(next_queries) == batch_size_limit:
      yield next_queries
      next_queries = []
      batch_count += 1
      batch_size_limit = ITEMS_PER_CONTAINS_QUERIES[
          min(batch_count, len(ITEMS_PER_CONTAINS_QUERIES) - 1)]
  if next_queries:
    yield next_queries


class FetchQueue(object):
  """Fetches items from Storage and places them into ContentAddressedCache.

  It manages multiple concurrent fetch operations. Acts as a bridge between
  Storage and ContentAddressedCache so that Storage and ContentAddressedCache
  don't depend on each other at all.
  """

  def __init__(self, storage, cache):
    self.storage = storage
    self.cache = cache
    self._channel = threading_utils.TaskChannel()
    self._pending = set()
    self._accessed = set()
    self._fetched = set(cache)
    # Pending digests that the caller waits for, see wait_on()/wait().
    self._waiting_on = set()
    # Already fetched digests the caller waits for which are not yet returned by
    # wait().
    self._waiting_on_ready = set()

  def add(
      self,
      digest,
      size=local_caching.UNKNOWN_FILE_SIZE,
      priority=threading_utils.PRIORITY_MED):
    """Starts asynchronous fetch of item |digest|."""
    # Fetching it now?
    if digest in self._pending:
      return

    # Mark this file as in use, verify_all_cached will later ensure it is still
    # in cache.
    self._accessed.add(digest)

    # Already fetched? Notify cache to update item's LRU position.
    if digest in self._fetched:
      # 'touch' returns True if item is in cache and not corrupted.
      if self.cache.touch(digest, size):
        return
      logging.error('%s is corrupted', digest)
      self._fetched.remove(digest)

    # TODO(maruel): It should look at the free disk space, the current cache
    # size and the size of the new item on every new item:
    # - Trim the cache as more entries are listed when free disk space is low,
    #   otherwise if the amount of data downloaded during the run > free disk
    #   space, it'll crash.
    # - Make sure there's enough free disk space to fit all dependencies of
    #   this run! If not, abort early.

    # Start fetching.
    self._pending.add(digest)
    self.storage.async_fetch(
        self._channel, priority, digest, size,
        functools.partial(self.cache.write, digest))

  def wait_on(self, digest):
    """Updates digests to be waited on by 'wait'."""
    # Calculate once the already fetched items. These will be retrieved first.
    if digest in self._fetched:
      self._waiting_on_ready.add(digest)
    else:
      self._waiting_on.add(digest)

  def wait(self):
    """Waits until any of waited-on items is retrieved.

    Once this happens, it is remove from the waited-on set and returned.

    This function is called in two waves. The first wave it is done for HIGH
    priority items, the isolated files themselves. The second wave it is called
    for all the files.

    If the waited-on set is empty, raises RuntimeError.
    """
    # Flush any already fetched items.
    if self._waiting_on_ready:
      return self._waiting_on_ready.pop()

    assert self._waiting_on, 'Needs items to wait on'

    # Wait for one waited-on item to be fetched.
    while self._pending:
      digest = self._channel.pull()
      self._pending.remove(digest)
      self._fetched.add(digest)
      if digest in self._waiting_on:
        self._waiting_on.remove(digest)
        return digest

    # Should never reach this point due to assert above.
    raise RuntimeError('Impossible state')

  @property
  def wait_queue_empty(self):
    """Returns True if there is no digest left for wait() to return."""
    return not self._waiting_on and not self._waiting_on_ready

  def inject_local_file(self, path, algo):
    """Adds local file to the cache as if it was fetched from storage."""
    with fs.open(path, 'rb') as f:
      data = f.read()
    digest = algo(data).hexdigest()
    self.cache.write(digest, [data])
    self._fetched.add(digest)
    return digest

  @property
  def pending_count(self):
    """Returns number of items to be fetched."""
    return len(self._pending)

  def verify_all_cached(self):
    """True if all accessed items are in cache."""
    # Not thread safe, but called after all work is done.
    return self._accessed.issubset(self.cache)


class FetchStreamVerifier(object):
  """Verifies that fetched file is valid before passing it to the
  ContentAddressedCache.
  """

  def __init__(self, stream, hasher, expected_digest, expected_size):
    """Initializes the verifier.

    Arguments:
    * stream: an iterable yielding chunks of content
    * hasher: an object from hashlib that supports update() and hexdigest()
      (eg, hashlib.sha1).
    * expected_digest: if the entire stream is piped through hasher and then
      summarized via hexdigest(), this should be the result. That is, it
      should be a hex string like 'abc123'.
    * expected_size: either the expected size of the stream, or
      local_caching.UNKNOWN_FILE_SIZE.
    """
    assert stream is not None
    self.stream = stream
    self.expected_digest = expected_digest
    self.expected_size = expected_size
    self.current_size = 0
    self.rolling_hash = hasher()

  def run(self):
    """Generator that yields same items as |stream|.

    Verifies |stream| is complete before yielding a last chunk to consumer.

    Also wraps IOError produced by consumer into MappingError exceptions since
    otherwise Storage will retry fetch on unrelated local cache errors.
    """
    # Read one chunk ahead, keep it in |stored|.
    # That way a complete stream can be verified before pushing last chunk
    # to consumer.
    stored = None
    for chunk in self.stream:
      assert chunk is not None
      if stored is not None:
        self._inspect_chunk(stored, is_last=False)
        try:
          yield stored
        except IOError as exc:
          raise isolated_format.MappingError(
              'Failed to store an item in cache: %s' % exc)
      stored = chunk
    if stored is not None:
      self._inspect_chunk(stored, is_last=True)
      try:
        yield stored
      except IOError as exc:
        raise isolated_format.MappingError(
            'Failed to store an item in cache: %s' % exc)

  def _inspect_chunk(self, chunk, is_last):
    """Called for each fetched chunk before passing it to consumer."""
    self.current_size += len(chunk)
    self.rolling_hash.update(chunk)
    if not is_last:
      return

    if ((self.expected_size != local_caching.UNKNOWN_FILE_SIZE) and
        (self.expected_size != self.current_size)):
      msg = 'Incorrect file size: want %d, got %d' % (
          self.expected_size, self.current_size)
      raise IOError(msg)

    actual_digest = self.rolling_hash.hexdigest()
    if self.expected_digest != actual_digest:
      msg = 'Incorrect digest: want %s, got %s' % (
          self.expected_digest, actual_digest)
      raise IOError(msg)


class IsolatedBundle(object):
  """Fetched and parsed .isolated file with all dependencies."""

  def __init__(self):
    self.command = []
    self.files = {}
    self.read_only = None
    self.relative_cwd = None
    # The main .isolated file, a IsolatedFile instance.
    self.root = None

  def fetch(self, fetch_queue, root_isolated_hash, algo):
    """Fetches the .isolated and all the included .isolated.

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

    As a side effect this method starts asynchronous fetch of all data files
    by adding them to |fetch_queue|. It doesn't wait for data files to finish
    fetching though.
    """
    self.root = isolated_format.IsolatedFile(root_isolated_hash, algo)

    # Isolated files being retrieved now: hash -> IsolatedFile instance.
    pending = {}
    # Set of hashes of already retrieved items to refuse recursive includes.
    seen = set()
    # Set of IsolatedFile's whose data files have already being fetched.
    processed = set()

    def retrieve_async(isolated_file):
      """Retrieves an isolated file included by the root bundle."""
      h = isolated_file.obj_hash
      if h in seen:
        raise isolated_format.IsolatedError(
            'IsolatedFile %s is retrieved recursively' % h)
      assert h not in pending
      seen.add(h)
      pending[h] = isolated_file
      # This isolated item is being added dynamically, notify FetchQueue.
      fetch_queue.wait_on(h)
      fetch_queue.add(h, priority=threading_utils.PRIORITY_HIGH)

    # Start fetching root *.isolated file (single file, not the whole bundle).
    retrieve_async(self.root)

    while pending:
      # Wait until some *.isolated file is fetched, parse it.
      item_hash = fetch_queue.wait()
      item = pending.pop(item_hash)
      with fetch_queue.cache.getfileobj(item_hash) as f:
        item.load(f.read())

      # Start fetching included *.isolated files.
      for new_child in item.children:
        retrieve_async(new_child)

      # Always fetch *.isolated files in traversal order, waiting if necessary
      # until next to-be-processed node loads. "Waiting" is done by yielding
      # back to the outer loop, that waits until some *.isolated is loaded.
      for node in isolated_format.walk_includes(self.root):
        if node not in processed:
          # Not visited, and not yet loaded -> wait for it to load.
          if not node.is_loaded:
            break
          # Not visited and loaded -> process it and continue the traversal.
          self._start_fetching_files(node, fetch_queue)
          processed.add(node)

    # All *.isolated files should be processed by now and only them.
    all_isolateds = set(isolated_format.walk_includes(self.root))
    assert all_isolateds == processed, (all_isolateds, processed)
    assert fetch_queue.wait_queue_empty, 'FetchQueue should have been emptied'

    # Extract 'command' and other bundle properties.
    for node in isolated_format.walk_includes(self.root):
      self._update_self(node)
    self.relative_cwd = self.relative_cwd or ''

  def _start_fetching_files(self, isolated, fetch_queue):
    """Starts fetching files from |isolated| that are not yet being fetched.

    Modifies self.files.
    """
    files = isolated.data.get('files', {})
    logging.debug('fetch_files(%s, %d)', isolated.obj_hash, len(files))
    for filepath, properties in files.iteritems():
      # Root isolated has priority on the files being mapped. In particular,
      # overridden files must not be fetched.
      if filepath not in self.files:
        self.files[filepath] = properties

        # Make sure if the isolated is read only, the mode doesn't have write
        # bits.
        if 'm' in properties and self.read_only:
          properties['m'] &= ~(stat.S_IWUSR | stat.S_IWGRP | stat.S_IWOTH)

        # Preemptively request hashed files.
        if 'h' in properties:
          fetch_queue.add(
              properties['h'], properties['s'], threading_utils.PRIORITY_MED)

  def _update_self(self, node):
    """Extracts bundle global parameters from loaded *.isolated file.

    Will be called with each loaded *.isolated file in order of traversal of
    isolated include graph (see isolated_format.walk_includes).
    """
    # Grabs properties.
    if not self.command and node.data.get('command'):
      # Ensure paths are correctly separated on windows.
      self.command = node.data['command']
      if self.command:
        self.command[0] = self.command[0].replace('/', os.path.sep)
    if self.read_only is None and node.data.get('read_only') is not None:
      self.read_only = node.data['read_only']
    if (self.relative_cwd is None and
        node.data.get('relative_cwd') is not None):
      self.relative_cwd = node.data['relative_cwd']


def get_storage(url, namespace):
  """Returns Storage class that can upload and download from |namespace|.

  Arguments:
    url: URL of isolate service to use shared cloud based storage.
    namespace: isolate namespace to operate in, also defines hashing and
        compression scheme used, i.e. namespace names that end with '-gzip'
        store compressed data.

  Returns:
    Instance of Storage.
  """
  return Storage(isolate_storage.get_storage_api(url, namespace))


def upload_tree(base_url, infiles, namespace):
  """Uploads the given tree to the given url.

  Arguments:
    base_url:  The url of the isolate server to upload to.
    infiles:   iterable of pairs (absolute path, metadata dict) of files.
    namespace: The namespace to use on the server.
  """
  # Convert |infiles| into a list of FileItem objects, skip duplicates.
  # Filter out symlinks, since they are not represented by items on isolate
  # server side.
  items = []
  seen = set()
  skipped = 0
  for filepath, metadata in infiles:
    assert isinstance(filepath, unicode), filepath
    if 'l' not in metadata and filepath not in seen:
      seen.add(filepath)
      item = FileItem(
          path=filepath,
          digest=metadata['h'],
          size=metadata['s'],
          high_priority=metadata.get('priority') == '0')
      items.append(item)
    else:
      skipped += 1

  logging.info('Skipped %d duplicated entries', skipped)
  with get_storage(base_url, namespace) as storage:
    return storage.upload_items(items)


def fetch_isolated(isolated_hash, storage, cache, outdir, use_symlinks):
  """Aggressively downloads the .isolated file(s), then download all the files.

  Arguments:
    isolated_hash: hash of the root *.isolated file.
    storage: Storage class that communicates with isolate storage.
    cache: ContentAddressedCache class that knows how to store and map files
           locally.
    outdir: Output directory to map file tree to.
    use_symlinks: Use symlinks instead of hardlinks when True.

  Returns:
    IsolatedBundle object that holds details about loaded *.isolated file.
  """
  logging.debug(
      'fetch_isolated(%s, %s, %s, %s, %s)',
      isolated_hash, storage, cache, outdir, use_symlinks)
  # Hash algorithm to use, defined by namespace |storage| is using.
  algo = storage.hash_algo
  fetch_queue = FetchQueue(storage, cache)
  bundle = IsolatedBundle()

  with tools.Profiler('GetIsolateds'):
    # Optionally support local files by manually adding them to cache.
    if not isolated_format.is_valid_hash(isolated_hash, algo):
      logging.debug('%s is not a valid hash, assuming a file '
                    '(algo was %s, hash size was %d)',
                    isolated_hash, algo(), algo().digest_size)
      path = unicode(os.path.abspath(isolated_hash))
      try:
        isolated_hash = fetch_queue.inject_local_file(path, algo)
      except IOError as e:
        raise isolated_format.MappingError(
            '%s doesn\'t seem to be a valid file. Did you intent to pass a '
            'valid hash (error: %s)?' % (isolated_hash, e))

    # Load all *.isolated and start loading rest of the files.
    bundle.fetch(fetch_queue, isolated_hash, algo)

  with tools.Profiler('GetRest'):
    # Create file system hierarchy.
    file_path.ensure_tree(outdir)
    create_directories(outdir, bundle.files)
    create_symlinks(outdir, bundle.files.iteritems())

    # Ensure working directory exists.
    cwd = os.path.normpath(os.path.join(outdir, bundle.relative_cwd))
    file_path.ensure_tree(cwd)

    # Multimap: digest -> list of pairs (path, props).
    remaining = {}
    for filepath, props in bundle.files.iteritems():
      if 'h' in props:
        remaining.setdefault(props['h'], []).append((filepath, props))
        fetch_queue.wait_on(props['h'])

    # Now block on the remaining files to be downloaded and mapped.
    logging.info('Retrieving remaining files (%d of them)...',
        fetch_queue.pending_count)
    last_update = time.time()
    with threading_utils.DeadlockDetector(DEADLOCK_TIMEOUT) as detector:
      while remaining:
        detector.ping()

        # Wait for any item to finish fetching to cache.
        digest = fetch_queue.wait()

        # Create the files in the destination using item in cache as the
        # source.
        for filepath, props in remaining.pop(digest):
          fullpath = os.path.join(outdir, filepath)

          with cache.getfileobj(digest) as srcfileobj:
            filetype = props.get('t', 'basic')

            if filetype == 'basic':
              # Ignore all bits apart from the user.
              file_mode = (props.get('m') or 0500) & 0700
              if bundle.read_only:
                # Enforce read-only if the root bundle does.
                file_mode &= 0500
              putfile(
                  srcfileobj, fullpath, file_mode,
                  use_symlink=use_symlinks)

            elif filetype == 'tar':
              basedir = os.path.dirname(fullpath)
              with tarfile.TarFile(fileobj=srcfileobj, encoding='utf-8') as t:
                for ti in t:
                  if not ti.isfile():
                    logging.warning(
                        'Path(%r) is nonfile (%s), skipped',
                        ti.name, ti.type)
                    continue
                  # Handle files created on Windows fetched on POSIX and the
                  # reverse.
                  other_sep = '/' if os.path.sep == '\\' else '\\'
                  name = ti.name.replace(other_sep, os.path.sep)
                  fp = os.path.normpath(os.path.join(basedir, name))
                  if not fp.startswith(basedir):
                    logging.error(
                        'Path(%r) is outside root directory',
                        fp)
                  ifd = t.extractfile(ti)
                  file_path.ensure_tree(os.path.dirname(fp))
                  file_mode = ti.mode & 0700
                  if bundle.read_only:
                    # Enforce read-only if the root bundle does.
                    file_mode &= 0500
                  putfile(ifd, fp, file_mode, ti.size)

            else:
              raise isolated_format.IsolatedError(
                    'Unknown file type %r', filetype)

        # Report progress.
        duration = time.time() - last_update
        if duration > DELAY_BETWEEN_UPDATES_IN_SECS:
          msg = '%d files remaining...' % len(remaining)
          sys.stdout.write(msg + '\n')
          sys.stdout.flush()
          logging.info(msg)
          last_update = time.time()
    assert fetch_queue.wait_queue_empty, 'FetchQueue should have been emptied'

  # Cache could evict some items we just tried to fetch, it's a fatal error.
  cache.trim()
  if not fetch_queue.verify_all_cached():
    free_disk = file_path.get_free_space(cache.cache_dir)
    msg = (
        'Cache is too small to hold all requested files.\n'
        '  %s\n  cache=%dbytes, %d items; %sb free_space') % (
          cache.policies, cache.total_size, len(cache), free_disk)
    raise isolated_format.MappingError(msg)
  return bundle


def directory_to_metadata(root, algo, blacklist):
  """Returns the FileItem list and .isolated metadata for a directory."""
  root = file_path.get_native_path_case(root)
  paths = isolated_format.expand_directory_and_symlink(
      root, u'.' + os.path.sep, blacklist, sys.platform != 'win32')
  metadata = {
    relpath: isolated_format.file_to_metadata(
        os.path.join(root, relpath), {}, 0, algo, False)
    for relpath in paths
  }
  for v in metadata.itervalues():
    v.pop('t')
  items = [
      FileItem(
          path=os.path.join(root, relpath),
          digest=meta['h'],
          size=meta['s'],
          high_priority=relpath.endswith('.isolated'))
      for relpath, meta in metadata.iteritems() if 'h' in meta
  ]
  return items, metadata


def archive_files_to_storage(storage, files, blacklist):
  """Stores every entries and returns the relevant data.

  Arguments:
    storage: a Storage object that communicates with the remote object store.
    files: list of file paths to upload. If a directory is specified, a
           .isolated file is created and its hash is returned.
    blacklist: function that returns True if a file should be omitted.

  Returns:
    tuple(list(tuple(hash, path)), list(FileItem cold), list(FileItem hot)).
    The first file in the first item is always the isolated file.
  """
  assert all(isinstance(i, unicode) for i in files), files
  if len(files) != len(set(map(os.path.abspath, files))):
    raise AlreadyExists('Duplicate entries found.')

  # List of tuple(hash, path).
  results = []
  # The temporary directory is only created as needed.
  tempdir = None
  try:
    # TODO(maruel): Yield the files to a worker thread.
    items_to_upload = []
    for f in files:
      try:
        filepath = os.path.abspath(f)
        if fs.isdir(filepath):
          # Uploading a whole directory.
          items, metadata = directory_to_metadata(
              filepath, storage.hash_algo, blacklist)

          # Create the .isolated file.
          if not tempdir:
            tempdir = tempfile.mkdtemp(prefix=u'isolateserver')
          handle, isolated = tempfile.mkstemp(dir=tempdir, suffix=u'.isolated')
          os.close(handle)
          data = {
              'algo':
                  isolated_format.SUPPORTED_ALGOS_REVERSE[storage.hash_algo],
              'files': metadata,
              'version': isolated_format.ISOLATED_FILE_VERSION,
          }
          isolated_format.save_isolated(isolated, data)
          h = isolated_format.hash_file(isolated, storage.hash_algo)
          items_to_upload.extend(items)
          items_to_upload.append(
              FileItem(
                  path=isolated,
                  digest=h,
                  size=fs.stat(isolated).st_size,
                  high_priority=True))
          results.append((h, f))

        elif fs.isfile(filepath):
          h = isolated_format.hash_file(filepath, storage.hash_algo)
          items_to_upload.append(
            FileItem(
                path=filepath,
                digest=h,
                size=fs.stat(filepath).st_size,
                high_priority=f.endswith('.isolated')))
          results.append((h, f))
        else:
          raise Error('%s is neither a file or directory.' % f)
      except OSError:
        raise Error('Failed to process %s.' % f)
    uploaded = storage.upload_items(items_to_upload)
    cold = [i for i in items_to_upload if i in uploaded]
    hot = [i for i in items_to_upload if i not in uploaded]
    return results, cold, hot
  finally:
    if tempdir and fs.isdir(tempdir):
      file_path.rmtree(tempdir)


def archive(out, namespace, files, blacklist):
  if files == ['-']:
    files = sys.stdin.readlines()

  if not files:
    raise Error('Nothing to upload')

  files = [f.decode('utf-8') for f in files]
  blacklist = tools.gen_blacklist(blacklist)
  with get_storage(out, namespace) as storage:
    # Ignore stats.
    results = archive_files_to_storage(storage, files, blacklist)[0]
  print('\n'.join('%s %s' % (r[0], r[1]) for r in results))


@subcommand.usage('<file1..fileN> or - to read from stdin')
def CMDarchive(parser, args):
  """Archives data to the server.

  If a directory is specified, a .isolated file is created the whole directory
  is uploaded. Then this .isolated file can be included in another one to run
  commands.

  The commands output each file that was processed with its content hash. For
  directories, the .isolated generated for the directory is listed as the
  directory entry itself.
  """
  add_isolate_server_options(parser)
  add_archive_options(parser)
  options, files = parser.parse_args(args)
  process_isolate_server_options(parser, options, True, True)
  try:
    archive(options.isolate_server, options.namespace, files, options.blacklist)
  except (Error, local_caching.NoMoreSpace) as e:
    parser.error(e.args[0])
  return 0


def CMDdownload(parser, args):
  """Download data from the server.

  It can either download individual files or a complete tree from a .isolated
  file.
  """
  add_isolate_server_options(parser)
  parser.add_option(
      '-s', '--isolated', metavar='HASH',
      help='hash of an isolated file, .isolated file content is discarded, use '
           '--file if you need it')
  parser.add_option(
      '-f', '--file', metavar='HASH DEST', default=[], action='append', nargs=2,
      help='hash and destination of a file, can be used multiple times')
  parser.add_option(
      '-t', '--target', metavar='DIR', default='download',
      help='destination directory')
  parser.add_option(
      '--use-symlinks', action='store_true',
      help='Use symlinks instead of hardlinks')
  add_cache_options(parser)
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported arguments: %s' % args)
  if not file_path.enable_symlink():
    logging.error('Symlink support is not enabled')

  process_isolate_server_options(parser, options, True, True)
  if bool(options.isolated) == bool(options.file):
    parser.error('Use one of --isolated or --file, and only one.')
  if not options.cache and options.use_symlinks:
    parser.error('--use-symlinks require the use of a cache with --cache')

  cache = process_cache_options(options, trim=True)
  cache.cleanup()
  options.target = unicode(os.path.abspath(options.target))
  if options.isolated:
    if (fs.isfile(options.target) or
        (fs.isdir(options.target) and fs.listdir(options.target))):
      parser.error(
          '--target \'%s\' exists, please use another target' % options.target)
  with get_storage(options.isolate_server, options.namespace) as storage:
    # Fetching individual files.
    if options.file:
      # TODO(maruel): Enable cache in this case too.
      channel = threading_utils.TaskChannel()
      pending = {}
      for digest, dest in options.file:
        dest = unicode(dest)
        pending[digest] = dest
        storage.async_fetch(
            channel,
            threading_utils.PRIORITY_MED,
            digest,
            local_caching.UNKNOWN_FILE_SIZE,
            functools.partial(
                local_caching.file_write, os.path.join(options.target, dest)))
      while pending:
        fetched = channel.pull()
        dest = pending.pop(fetched)
        logging.info('%s: %s', fetched, dest)

    # Fetching whole isolated tree.
    if options.isolated:
      bundle = fetch_isolated(
          isolated_hash=options.isolated,
          storage=storage,
          cache=cache,
          outdir=options.target,
          use_symlinks=options.use_symlinks)
      cache.trim()
      if bundle.command:
        rel = os.path.join(options.target, bundle.relative_cwd)
        print('To run this test please run from the directory %s:' %
              os.path.join(options.target, rel))
        print('  ' + ' '.join(bundle.command))

  return 0


def add_archive_options(parser):
  parser.add_option(
      '--blacklist',
      action='append', default=list(DEFAULT_BLACKLIST),
      help='List of regexp to use as blacklist filter when uploading '
           'directories')


def add_isolate_server_options(parser):
  """Adds --isolate-server and --namespace options to parser."""
  parser.add_option(
      '-I', '--isolate-server',
      metavar='URL', default=os.environ.get('ISOLATE_SERVER', ''),
      help='URL of the Isolate Server to use. Defaults to the environment '
           'variable ISOLATE_SERVER if set. No need to specify https://, this '
           'is assumed.')
  parser.add_option(
      '--grpc-proxy', help='gRPC proxy by which to communicate to Isolate')
  parser.add_option(
      '--namespace', default='default-gzip',
      help='The namespace to use on the Isolate Server, default: %default')


def process_isolate_server_options(
    parser, options, set_exception_handler, required):
  """Processes the --isolate-server option.

  Returns the identity as determined by the server.
  """
  if not options.isolate_server:
    if required:
      parser.error('--isolate-server is required.')
    return

  if options.grpc_proxy:
    isolate_storage.set_grpc_proxy(options.grpc_proxy)
  else:
    try:
      options.isolate_server = net.fix_url(options.isolate_server)
    except ValueError as e:
      parser.error('--isolate-server %s' % e)
  if set_exception_handler:
    on_error.report_on_exception_exit(options.isolate_server)
  try:
    return auth.ensure_logged_in(options.isolate_server)
  except ValueError as e:
    parser.error(str(e))


def add_cache_options(parser):
  cache_group = optparse.OptionGroup(parser, 'Cache management')
  cache_group.add_option(
      '--cache', metavar='DIR', default='cache',
      help='Directory to keep a local cache of the files. Accelerates download '
           'by reusing already downloaded files. Default=%default')
  cache_group.add_option(
      '--max-cache-size',
      type='int',
      metavar='NNN',
      default=50*1024*1024*1024,
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


def process_cache_options(options, trim, **kwargs):
  if options.cache:
    policies = local_caching.CachePolicies(
        options.max_cache_size,
        options.min_free_space,
        options.max_items,
        # 3 weeks.
        max_age_secs=21*24*60*60)

    # |options.cache| path may not exist until DiskContentAddressedCache()
    # instance is created.
    return local_caching.DiskContentAddressedCache(
        unicode(os.path.abspath(options.cache)),
        policies,
        isolated_format.get_hash_algo(options.namespace),
        trim,
        **kwargs)
  else:
    return local_caching.MemoryContentAddressedCache()


class OptionParserIsolateServer(logging_utils.OptionParserWithLogging):
  def __init__(self, **kwargs):
    logging_utils.OptionParserWithLogging.__init__(
        self,
        version=__version__,
        prog=os.path.basename(sys.modules[__name__].__file__),
        **kwargs)
    auth.add_auth_options(self)

  def parse_args(self, *args, **kwargs):
    options, args = logging_utils.OptionParserWithLogging.parse_args(
        self, *args, **kwargs)
    auth.process_auth_options(self, options)
    return options, args


def main(args):
  dispatcher = subcommand.CommandDispatcher(__name__)
  return dispatcher.execute(OptionParserIsolateServer(), args)


if __name__ == '__main__':
  subprocess42.inhibit_os_error_reporting()
  fix_encoding.fix_encoding()
  tools.disable_buffering()
  colorama.init()
  sys.exit(main(sys.argv[1:]))
