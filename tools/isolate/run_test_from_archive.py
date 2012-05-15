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
  mode = os.stat(path).st_mode
  if read_only:
    mode = mode & 0500
  else:
    mode = mode | 0200
  if hasattr(os, 'lchmod'):
    os.lchmod(path, mode)  # pylint: disable=E1101
  else:
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
        self.state = json.load(open(self.state_file, 'rb'))
      except ValueError:
        # Too bad. The file will be overwritten and the cache cleared.
        pass
    self.trim()

  def trim(self):
    """Trims anything we don't know, make sure enough free space exists."""
    for f in os.listdir(self.cache_dir):
      if f == self.STATE_FILE or f in self.state:
        continue
      logging.warn('Unknown file %s from cache' % f)
      # Insert as the oldest file. It will be deleted eventually if not
      # accessed.
      self.state.insert(0, f)

    # Ensure enough free space.
    while (
        self.min_free_space and
        self.state and
        get_free_space(self.cache_dir) < self.min_free_space):
      try:
        os.remove(self.path(self.state.pop(0)))
      except OSError as e:
        logging.error('Error attempting to delete a file\n%s' % e)

    # Ensure maximum cache size.
    if self.max_cache_size and self.state:
      sizes = [os.stat(self.path(f)).st_size for f in self.state]
      while sizes and sum(sizes) > self.max_cache_size:
        # Delete the oldest item.
        try:
          os.remove(self.path(self.state.pop(0)))
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
      download_or_copy(os.path.join(self.remote, item), out)
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


def run_tha_test(manifest, cache_dir, remote, max_cache_size, min_free_space):
  """Downloads the dependencies in the cache, hardlinks them into a temporary
  directory and runs the executable.
  """
  cache = Cache(cache_dir, remote, max_cache_size, min_free_space)
  outdir = tempfile.mkdtemp(prefix='run_tha_test')
  try:
    for filepath, properties in manifest['files'].iteritems():
      infile = properties['sha-1']
      outfile = os.path.join(outdir, filepath)
      cache.retrieve(infile)
      outfiledir = os.path.dirname(outfile)
      if not os.path.isdir(outfiledir):
        os.makedirs(outfiledir)
      link_file(outfile, cache.path(infile), HARDLINK)
      if 'mode' in properties:
        # It's not set on Windows.
        os.chmod(outfile, properties['mode'])

    cwd = os.path.join(outdir, manifest['relative_cwd'])
    if not os.path.isdir(cwd):
      os.makedirs(cwd)
    if manifest.get('read_only'):
      make_writable(outdir, True)
    cmd = manifest['command']
    logging.info('Running %s, cwd=%s' % (cmd, cwd))
    return subprocess.call(cmd, cwd=cwd)
  finally:
    # Save first, in case an exception occur in the following lines, then clean
    # up.
    cache.save()
    rmtree(outdir)
    cache.trim()


def main():
  parser = optparse.OptionParser(
      usage='%prog <options>', description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-v', '--verbose', action='count', default=0, help='Use multiple times')
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
