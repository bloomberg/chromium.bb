# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a tree of hardlinks, symlinks or copy the inputs files."""

import ctypes
import logging
import os
import shutil
import sys


# Types of action accepted by recreate_tree().
DRY_RUN, HARDLINK, SYMLINK, COPY = range(5)[1:]


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


def _process_item(outdir, indir, relfile, action, blacklist):
  """Processes an input file.

  Returns True if processed.
  """
  logging.debug(
      '_process_item(%s, %s, %s, %s, %s)' % (
        outdir, indir, relfile, action, blacklist))
  if blacklist and blacklist(relfile):
    return False
  infile = os.path.normpath(os.path.join(indir, relfile))
  if not os.path.isfile(infile):
    raise MappingError('%s doesn\'t exist' % infile)

  if action == DRY_RUN:
    logging.info('Verified input: %s' % infile)
    return True

  outfile = os.path.normpath(os.path.join(outdir, relfile))
  logging.debug('Mapping %s to %s' % (infile, outfile))
  outsubdir = os.path.dirname(outfile)
  if not os.path.isdir(outsubdir):
    os.makedirs(outsubdir)

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
  return True


def _recurse_dir(outdir, indir, subdir, action, blacklist):
  """Processes a directory and all its decendents."""
  logging.debug(
      '_recurse_dir(%s, %s, %s, %s, %s)' % (
        outdir, indir, subdir, action, blacklist))
  root = os.path.join(indir, subdir)
  for dirpath, dirnames, filenames in os.walk(root):
    # Convert the absolute path to subdir + relative subdirectory.
    relpath = dirpath[len(indir)+1:]

    for filename in filenames:
      relfile = os.path.join(relpath, filename)
      _process_item(outdir, indir, relfile, action, blacklist)

    for index, dirname in enumerate(dirnames):
      # Do not process blacklisted directories.
      if blacklist(os.path.normpath(os.path.join(relpath, dirname))):
        del dirnames[index]


def recreate_tree(outdir, indir, infiles, action, blacklist):
  """Creates a new tree with only the input files in it.

  Arguments:
    outdir:    Temporary directory to create the files in.
    indir:     Root directory the infiles are based in.
    infiles:   List of files to map from |indir| to |outdir|.
    action:    See assert below.
    blacklist: Files to unconditionally ignore.
  """
  logging.debug(
      'recreate_tree(%s, %s, %s, %s, %s)' % (
        outdir, indir, infiles, action, blacklist))
  logging.info('Mapping from %s to %s' % (indir, outdir))

  assert action in (DRY_RUN, HARDLINK, SYMLINK, COPY)
  # Both need to be a local path.
  indir = os.path.normpath(indir)
  if not os.path.isdir(indir):
    raise MappingError('%s is not a directory' % indir)

  # Do not call abspath until it was verified the directory exists.
  indir = os.path.abspath(indir)

  if action != DRY_RUN:
    outdir = os.path.normpath(outdir)
    if not os.path.isdir(outdir):
      logging.info ('Creating %s' % outdir)
      os.makedirs(outdir)
    # Do not call abspath until the directory exists.
    outdir = os.path.abspath(outdir)

  for relfile in infiles:
    if os.path.isabs(relfile):
      raise MappingError('Can\'t map absolute path %s' % relfile)
    infile = os.path.normpath(os.path.join(indir, relfile))
    if not infile.startswith(indir):
      raise MappingError('Can\'t map file %s outside %s' % (infile, indir))

    isdir = os.path.isdir(infile)
    if isdir and not relfile.endswith('/'):
      raise MappingError(
          'Input directory %s must have a trailing slash' % infile)
    if not isdir and relfile.endswith('/'):
      raise MappingError(
          'Input file %s must not have a trailing slash' % infile)
    if isdir:
      _recurse_dir(outdir, indir, relfile, action, blacklist)
    else:
      _process_item(outdir, indir, relfile, action, blacklist)


def _set_write_bit(path, read_only):
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
