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


def preprocess_inputs(indir, infiles, blacklist):
  """Reads the infiles and expands the directories and applies the blacklist.

  Returns the normalized indir and infiles. Converts infiles with a trailing
  slash as the list of its files.
  """
  logging.debug('preprocess_inputs(%s, %s, %s)' % (indir, infiles, blacklist))
  # Both need to be a local path.
  indir = os.path.normpath(indir)
  if not os.path.isdir(indir):
    raise MappingError('%s is not a directory' % indir)

  # Do not call abspath until it was verified the directory exists.
  indir = os.path.abspath(indir)
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
  return outfiles, indir


def process_file(outfile, infile, action):
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
    outdir:    Temporary directory to create the files in.
    indir:     Root directory the infiles are based in.
    infiles:   List of files to map from |indir| to |outdir|. Must have been
               sanitized with preprocess_inputs().
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
    process_file(outfile, infile, action)


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
