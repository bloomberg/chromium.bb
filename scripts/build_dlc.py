# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate a DLC (Downloadable Content) artifact."""

from __future__ import print_function

import hashlib
import json
import math
import os

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import osutils


_SQUASHFS_TYPE = 'squashfs'
_EXT4_TYPE = 'ext4'

def HashFile(file_path):
  """Calculate the sha256 hash of a file.

  Args:
    file_path: (str) path to the file.

  Returns:
    [str]: The sha256 hash of the file.
  """
  sha256 = hashlib.sha256()
  with open(file_path, 'rb') as f:
    for b in iter(lambda: f.read(2048), b''):
      sha256.update(b)
  return sha256.hexdigest()


class DLCGenerator(object):
  """Object to generate DLC artifacts."""
  # Block size for the DLC image.
  # We use 4K for various reasons:
  # 1. it's what imageloader (linux kernel) supports.
  # 2. it's what verity supports.
  _BLOCK_SIZE = 4096
  # Blocks in the initial sparse image.
  _BLOCKS = 500000
  # Version of manifest file.
  _MANIFEST_VERSION = 1

  # The DLC root path inside the DLC module.
  _DLC_ROOT_DIR = 'root'

  def __init__(self, img_dir, meta_dir, src_dir, fs_type, pre_allocated_blocks,
               version, dlc_id, name):
    """Object initializer.

    Args:
      img_dir: (str) path to the DLC image dest root directory.
      meta_dir: (str) path to the DLC metadata dest root directory.
      src_dir: (str) path to the DLC source root directory.
      fs_type: (str) file system type.
      pre_allocated_blocks: (int) number of blocks pre-allocated on device.
      version: (str) DLC version.
      dlc_id: (str) DLC ID.
      name: (str) DLC name.
    """
    self.src_dir = src_dir
    self.fs_type = fs_type
    self.pre_allocated_blocks = pre_allocated_blocks
    self.version = version
    self.dlc_id = dlc_id
    self.name = name
    # Create path for all final artifacts.
    self.dest_image = os.path.join(img_dir, 'dlc.img')
    self.dest_table = os.path.join(meta_dir, 'table')
    self.dest_imageloader_json = os.path.join(meta_dir, 'imageloader.json')

  def SquashOwnerships(self, path):
    """Squash the owernships & permissions for files.

    Args:
      path: (str) path that contains all files to be processed.
    """
    cros_build_lib.SudoRunCommand(['chown', '-R', '0:0', path])
    cros_build_lib.SudoRunCommand(
        ['find', path, '-exec', 'touch', '-h', '-t', '197001010000.00', '{}',
         '+'])

  def CreateExt4Image(self):
    """Create an ext4 image."""
    with osutils.TempDir(prefix='dlc_') as temp_dir:
      mount_point = os.path.join(temp_dir, 'mount_point')
      # Create a raw image file.
      with open(self.dest_image, 'w') as f:
        f.truncate(self._BLOCKS * self._BLOCK_SIZE)
      # Create an ext4 file system on the raw image.
      cros_build_lib.RunCommand(
          ['/sbin/mkfs.ext4', '-b', str(self._BLOCK_SIZE), '-O',
           '^has_journal', self.dest_image], capture_output=True)
      # Create the mount_point directory.
      osutils.SafeMakedirs(mount_point)
      # Mount the ext4 image.
      osutils.MountDir(self.dest_image, mount_point, mount_opts=('loop', 'rw'))

      dlc_root_dir = os.path.join(mount_point, self._DLC_ROOT_DIR)
      osutils.SafeMakedirs(dlc_root_dir)
      try:
        # Copy DLC files over to the image.
        cros_build_lib.SudoRunCommand(['cp', '-a', self.src_dir, dlc_root_dir])
        self.SquashOwnerships(mount_point)
      finally:
        # Unmount the ext4 image.
        osutils.UmountDir(mount_point)
      # Shrink to minimum size.
      cros_build_lib.RunCommand(
          ['/sbin/e2fsck', '-y', '-f', self.dest_image], capture_output=True)
      cros_build_lib.RunCommand(
          ['/sbin/resize2fs', '-M', self.dest_image], capture_output=True)

  def CreateSquashfsImage(self):
    """Create a squashfs image."""
    with osutils.TempDir(prefix='dlc_') as temp_dir:
      squashfs_root = os.path.join(temp_dir, 'squashfs-root')
      dlc_root_dir = os.path.join(squashfs_root, self._DLC_ROOT_DIR)
      osutils.SafeMakedirs(dlc_root_dir)

      cros_build_lib.SudoRunCommand(['cp', '-a', self.src_dir, dlc_root_dir])
      self.SquashOwnerships(squashfs_root)

      cros_build_lib.RunCommand(['mksquashfs', squashfs_root, self.dest_image,
                                 '-4k-align', '-noappend'],
                                capture_output=True)

      # We changed the ownership and permissions of the squashfs_root
      # directory. Now we need to remove it manually.
      osutils.RmDir(squashfs_root, sudo=True)

  def CreateImage(self):
    """Create the image and copy the DLC files to it."""
    if self.fs_type == _EXT4_TYPE:
      self.CreateExt4Image()
    elif self.fs_type == _SQUASHFS_TYPE:
      self.CreateSquashfsImage()
    else:
      raise ValueError('Wrong fs type: %s used:' % self.fs_type)

  def GetImageloaderJsonContent(self, image_hash, table_hash, blocks):
    """Return the content of imageloader.json file.

    Args:
      image_hash: (str) sha256 hash of the DLC image.
      table_hash: (str) sha256 hash of the DLC table file.
      blocks: (int) number of blocks in the DLC image.

    Returns:
      [str]: content of imageloader.json file.
    """
    return {
        'fs-type': self.fs_type,
        'id': self.dlc_id,
        'image-sha256-hash': image_hash,
        'image-type': 'dlc',
        'is-removable': True,
        'manifest-version': self._MANIFEST_VERSION,
        'name': self.name,
        'pre-allocated-size': self.pre_allocated_blocks * self._BLOCK_SIZE,
        'size': blocks * self._BLOCK_SIZE,
        'table-sha256-hash': table_hash,
        'version': self.version,
    }

  def GenerateVerity(self):
    """Generate verity parameters and hashes for the image."""
    with osutils.TempDir(prefix='dlc_') as temp_dir:
      hash_tree = os.path.join(temp_dir, 'hash_tree')
      # Get blocks in the image.
      blocks = math.ceil(
          os.path.getsize(self.dest_image) / self._BLOCK_SIZE)
      result = cros_build_lib.RunCommand(
          ['verity', 'mode=create', 'alg=sha256', 'payload=' + self.dest_image,
           'payload_blocks=' + str(blocks), 'hashtree=' + hash_tree,
           'salt=random'], capture_output=True)
      table = result.output

      # Append the merkle tree to the image.
      osutils.WriteFile(self.dest_image, osutils.ReadFile(hash_tree), 'a+')

      # Write verity parameter to table file.
      osutils.WriteFile(self.dest_table, table)

      # Compute image hash.
      image_hash = HashFile(self.dest_image)
      table_hash = HashFile(self.dest_table)
      # Write image hash to imageloader.json file.
      blocks = math.ceil(
          os.path.getsize(self.dest_image) / self._BLOCK_SIZE)
      imageloader_json_content = self.GetImageloaderJsonContent(
          image_hash, table_hash, int(blocks))
      with open(self.dest_imageloader_json, 'w') as f:
        json.dump(imageloader_json_content, f)

  def GenerateDLC(self):
    """Generate a DLC artifact."""
    # Create the image and copy the DLC files to it.
    self.CreateImage()
    # Generate hash tree and other metadata.
    self.GenerateVerity()


def GetParser():
  parser = commandline.ArgumentParser(description=__doc__)
  # Required arguments:
  required = parser.add_argument_group('Required Arguments')
  required.add_argument('--src-dir', type='path', metavar='SRC_DIR_PATH',
                        required=True,
                        help='Root directory path that contains all DLC files '
                        'to be packed.')
  required.add_argument('--img-dir', type='path', metavar='IMG_DIR_PATH',
                        required=True,
                        help='Root directory path that contains DLC image file '
                        'output.')
  required.add_argument('--meta-dir', type='path', metavar='META_DIR_PATH',
                        required=True,
                        help='Root directory path that contains DLC metadata '
                        'output.')
  required.add_argument('--pre-allocated-blocks', type=int,
                        metavar='PREALLOCATEDBLOCKS', required=True,
                        help='Number of blocks (block size is 4k) that need to'
                        'be pre-allocated on device.')
  required.add_argument('--version', metavar='VERSION', required=True,
                        help='DLC Version.')
  required.add_argument('--id', metavar='ID', required=True,
                        help='DLC ID (unique per DLC).')
  required.add_argument('--name', metavar='NAME', required=True,
                        help='A human-readable name for the DLC.')

  args = parser.add_argument_group('Arguments')
  args.add_argument('--fs-type', metavar='FS_TYPE', default=_SQUASHFS_TYPE,
                    choices=(_SQUASHFS_TYPE, _EXT4_TYPE),
                    help='File system type of the image.')

  return parser


def main(argv):
  opts = GetParser().parse_args(argv)
  opts.Freeze()

  # Generate final DLC files.
  dlc_generator = DLCGenerator(opts.img_dir, opts.meta_dir, opts.src_dir,
                               opts.fs_type, opts.pre_allocated_blocks,
                               opts.version, opts.id, opts.name)
  dlc_generator.GenerateDLC()
