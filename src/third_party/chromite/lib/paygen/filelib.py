# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common local file interface library."""

from __future__ import print_function

import base64
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
  with open(file_path, mode='rb') as file_fobj:
    for block in ReadBlock(file_fobj):
      sha1.update(block)
      sha256.update(block)

  # Encode in base 64 string.  Other bases could be supported here.
  sha1_hex = base64.b64encode(sha1.digest()).decode('utf-8')
  sha256_hex = base64.b64encode(sha256.digest()).decode('utf-8')

  return sha1_hex, sha256_hex


def CopyFileSegment(in_file, in_mode, in_len, out_file, out_mode, in_seek=0):
  """Simulates a `dd` operation with seeks.

  Args:
    in_file: The input file
    in_mode: The mode to open the input file
    in_len: The length to copy
    out_file: The output file
    out_mode: The mode to open the output file
    in_seek: How many bytes to seek from the |in_file|
  """
  with open(in_file, in_mode) as in_stream, \
       open(out_file, out_mode) as out_stream:
    in_stream.seek(in_seek)
    remaining = in_len
    while remaining:
      chunk = in_stream.read(min(8192 * 1024, remaining))
      remaining -= len(chunk)
      out_stream.write(chunk)
