# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for handling Chrome OS partition."""

from __future__ import print_function

import os
import tempfile

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging

from chromite.lib.paygen import filelib


def ExtractPartition(filename, partition, out_part):
  """Extracts partition from an image file.

  Args:
    filename: The image file.
    partition: The partition name. e.g. ROOT or KERNEL.
    out_part: The output partition file.
  """
  parts = cros_build_lib.GetImageDiskPartitionInfo(filename)
  part_info = [p for p in parts if p.name == partition][0]

  offset = int(part_info.start)
  length = int(part_info.size)

  filelib.CopyFileSegment(filename, 'r', length, out_part, 'w', in_seek=offset)


def Ext2FileSystemSize(ext2_file):
  """Return the size of an ext2 filesystem in bytes.

  Args:
    ext2_file: The path to the ext2 file.
  """
  # dumpe2fs is normally installed in /sbin but doesn't require root.
  dump = cros_build_lib.RunCommand(['/sbin/dumpe2fs', '-h', ext2_file],
                                   print_cmd=False, capture_output=True).output
  fs_blocks = 0
  fs_blocksize = 0
  for line in dump.split('\n'):
    if not line:
      continue
    label, data = line.split(':')[:2]
    if label == 'Block count':
      fs_blocks = int(data)
    elif label == 'Block size':
      fs_blocksize = int(data)

  return fs_blocks * fs_blocksize


def PatchKernel(image, kern_file):
  """Patches a kernel with vblock from a stateful partition.

  Args:
    image: The stateful partition image.
    kern_file: The kernel file.
  """

  with tempfile.NamedTemporaryFile(prefix='stateful') as state_out, \
       tempfile.NamedTemporaryFile(prefix='vmlinuz_hd.vblock') as vblock:
    ExtractPartition(image, constants.PART_STATE, state_out)
    cros_build_lib.RunCommand(
        ['e2cp', '%s:/vmlinuz_hd.vblock' % state_out, vblock])
    filelib.CopyFileSegment(
        vblock, 'r', os.path.getsize(vblock), kern_file, 'r+')


def ExtractKernel(image, kern_out):
  """Extracts the kernel from the given image.

  Args:
    image: The image containing the kernel partition.
    kern_out: The output kernel file.
  """
  ExtractPartition(image, constants.PART_KERN_B, kern_out)
  with open(kern_out, 'r') as kern:
    if not any(kern.read(65536)):
      logging.warn('%s: Kernel B is empty, patching kernel A.', image)
      ExtractPartition(image, constants.PART_KERN_A, kern_out)
      PatchKernel(image, kern_out)


def ExtractRoot(image, root_out, truncate=True):
  """Extract the rootfs partition from a gpt image.

  Args:
    image: The input image file.
    root_out: The output root partition file.
    truncate: If true, truncate the partition to the file system size.
  """
  ExtractPartition(image, constants.PART_ROOT_A, root_out)

  if not truncate:
    return

  # We only update the filesystem part of the partition, which is stored in the
  # gpt script. So we need to truncated it to the file system size if asked for.
  root_out_size = Ext2FileSystemSize(root_out)
  if root_out_size:
    with open(root_out, 'a') as root:
      root.truncate(root_out_size)
    logging.info('Truncated root to %d bytes.', root_out_size)
  else:
    raise IOError('Error truncating the rootfs to filesystem size.')
