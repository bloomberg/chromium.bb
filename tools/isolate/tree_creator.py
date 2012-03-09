# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""File related utility functions.

Creates a tree of hardlinks, symlinks or copy the inputs files. Calculate files
hash.
"""

import ctypes
import hashlib
import logging
import os
import shutil
import stat
import sys
import time


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


def expand_directories(indir, infiles, blacklist):
  """Expands the directories, applies the blacklist and verifies files exist."""
  logging.debug('expand_directories(%s, %s, %s)' % (indir, infiles, blacklist))
  outfiles = []
  for relfile in infiles:
    if os.path.isabs(relfile):
      raise MappingError('Can\'t map absolute path %s' % relfile)
    infile = os.path.normpath(os.path.join(indir, relfile))
    if not infile.startswith(indir):
      raise MappingError('Can\'t map file %s outside %s' % (infile, indir))

    if relfile.endswith('/'):
      if not os.path.isdir(infile):
        raise MappingError(
            'Input directory %s must have a trailing slash' % infile)
      for dirpath, dirnames, filenames in os.walk(infile):
        # Convert the absolute path to subdir + relative subdirectory.
        relpath = dirpath[len(indir)+1:]
        outfiles.extend(os.path.join(relpath, f) for f in filenames)
        for index, dirname in enumerate(dirnames):
          # Do not process blacklisted directories.
          if blacklist(os.path.join(relpath, dirname)):
            del dirnames[index]
    else:
      if not os.path.isfile(infile):
        raise MappingError('Input file %s doesn\'t exist' % infile)
      outfiles.append(relfile)
  return outfiles


def process_inputs(indir, infiles, need_hash, read_only):
  """Returns a dictionary of input files, populated with the files' mode and
  hash.

  The file mode is manipulated if read_only is True. In practice, we only save
  one of 4 modes: 0755 (rwx), 0644 (rw), 0555 (rx), 0444 (r).
  """
  outdict = {}
  for infile in infiles:
    filepath = os.path.join(indir, infile)
    filemode = stat.S_IMODE(os.stat(filepath).st_mode)
    # Remove write access for non-owner.
    filemode &= ~(stat.S_IWGRP | stat.S_IWOTH)
    if read_only:
      filemode &= ~stat.S_IWUSR
    if filemode & stat.S_IXUSR:
      filemode |= (stat.S_IXGRP | stat.S_IXOTH)
    else:
      filemode &= ~(stat.S_IXGRP | stat.S_IXOTH)
    outdict[infile] = {
      'mode': filemode,
    }
    if need_hash:
      h = hashlib.sha1()
      with open(filepath, 'rb') as f:
        h.update(f.read())
      outdict[infile]['sha-1'] = h.hexdigest()
  return outdict


def link_file(outfile, infile, action):
  """Links a file. The type of link depends on |action|."""
  logging.debug('Mapping %s to %s' % (infile, outfile))
  if os.path.isfile(outfile):
    raise MappingError('%s already exist' % outfile)

  if action == COPY:
    shutil.copy(infile, outfile)
  elif action == SYMLINK and sys.platform != 'win32':
    os.symlink(infile, outfile)
  elif action == HARDLINK:
    try:
      os_link(infile, outfile)
    except OSError:
      # Probably a different file system.
      logging.warn(
          'Failed to hardlink, failing back to copy %s to %s' % (
            infile, outfile))
      shutil.copy(infile, outfile)
  else:
    raise ValueError('Unknown mapping action %s' % action)


def recreate_tree(outdir, indir, infiles, action):
  """Creates a new tree with only the input files in it.

  Arguments:
    outdir:    Output directory to create the files in.
    indir:     Root directory the infiles are based in.
    infiles:   List of files to map from |indir| to |outdir|.
    action:    See assert below.
  """
  logging.debug(
      'recreate_tree(%s, %s, %s, %s)' % (outdir, indir, infiles, action))
  logging.info('Mapping from %s to %s' % (indir, outdir))

  assert action in (HARDLINK, SYMLINK, COPY)
  outdir = os.path.normpath(outdir)
  if not os.path.isdir(outdir):
    logging.info ('Creating %s' % outdir)
    os.makedirs(outdir)
  # Do not call abspath until the directory exists.
  outdir = os.path.abspath(outdir)

  for relfile in infiles:
    infile = os.path.join(indir, relfile)
    outfile = os.path.join(outdir, relfile)
    outsubdir = os.path.dirname(outfile)
    if not os.path.isdir(outsubdir):
      os.makedirs(outsubdir)
    link_file(outfile, infile, action)


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
