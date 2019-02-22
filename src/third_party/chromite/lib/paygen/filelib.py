# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common local file interface library."""

from __future__ import print_function

import base64
import fnmatch
import hashlib
import os
import shutil

from chromite.lib import osutils


def Copy(src_path, dest_path):
  """Copy one path to another.

  Automatically create the directory for dest_path, if necessary.

  Args:
    src_path: Path to local file to copy from.
    dest_path: Path to local file to copy to.
  """
  dest_dir = os.path.dirname(dest_path)
  if dest_dir and not os.path.isdir(dest_dir):
    osutils.SafeMakedirs(dest_dir)

  shutil.copy2(src_path, dest_path)


def ListFiles(root_path, recurse=False, filepattern=None, sort=False):
  """Return list of full file paths under given root path.

  Directories are intentionally excluded.

  Args:
    root_path: e.g. /some/path/to/dir
    recurse: Look for files in subdirectories, as well
    filepattern: glob pattern to match against basename of file
    sort: If True then do a default sort on paths.

  Returns:
    List of paths to files that matched
  """
  # Smoothly accept trailing '/' in root_path.
  root_path = root_path.rstrip('/')

  paths = []

  if recurse:
    # Recursively walk paths starting at root_path, filter for files.
    for entry in os.walk(root_path):
      dir_path, _, files = entry
      for file_entry in files:
        paths.append(os.path.join(dir_path, file_entry))

  else:
    # List paths directly in root_path, filter for files.
    for filename in os.listdir(root_path):
      path = os.path.join(root_path, filename)
      if os.path.isfile(path):
        paths.append(path)

  # Filter by filepattern, if specified.
  if filepattern:
    paths = [p for p in paths
             if fnmatch.fnmatch(os.path.basename(p), filepattern)]

  # Sort results, if specified.
  if sort:
    paths = sorted(paths)

  return paths


def MD5Sum(file_path):
  """Computer the MD5Sum of a file.

  Args:
    file_path: The full path to the file to compute the sum.

  Returns:
    A string of the md5sum if the file exists or
    None if the file does not exist or is actually a directory.
  """
  # For some reason pylint refuses to accept that md5 is a function in
  # the hashlib module, hence this pylint disable.
  if not os.path.exists(file_path):
    return None

  if os.path.isdir(file_path):
    return None

  # Note that there is anecdotal evidence in other code that not using the
  # binary flag with this open (open(file_path, 'rb')) can malfunction.  The
  # problem has not shown up here, but be aware.
  md5_hash = hashlib.md5()
  with open(file_path) as file_fobj:
    for line in file_fobj:
      md5_hash.update(line)

  return md5_hash.hexdigest()


def ReadBlock(file_obj, size=1024):
  """Generator function to Read and return a specificed number of bytes.

  Args:
    file_obj: The file object to read data from
    size: The size in bytes to read in at a time.

  Yields:
    The block of data that was read.
  """
  while True:
    data = file_obj.read(size)
    if not data:
      break

    yield data


def ShaSums(file_path):
  """Calculate the SHA1 and SHA256 checksum of a file.

  Args:
    file_path: The full path to the file.

  Returns:
    A tuple of base64 encoded sha1 and sha256 hashes.
  """
  sha1 = hashlib.sha1()
  sha256 = hashlib.sha256()
  with open(file_path, mode='r') as file_fobj:
    for block in ReadBlock(file_fobj):
      sha1.update(block)
      sha256.update(block)

  # Encode in base 64 string.  Other bases could be supported here.
  sha1_hex = base64.b64encode(sha1.digest())
  sha256_hex = base64.b64encode(sha256.digest())

  return sha1_hex, sha256_hex
