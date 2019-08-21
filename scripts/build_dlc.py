# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate a DLC (Downloadable Content) artifact."""

from __future__ import division
from __future__ import print_function

import hashlib
import json
import math
import os
import shutil

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils

from chromite.scripts import cros_set_lsb_release

DLC_META_DIR = 'opt/google/dlc/'
DLC_IMAGE_DIR = 'build/rootfs/dlc/'
LSB_RELEASE = 'etc/lsb-release'

# This file has major and minor version numbers that the update_engine client
# supports. These values are needed for generating a delta/full payload.
UPDATE_ENGINE_CONF = 'etc/update_engine.conf'

_EXTRA_RESOURCES = (
    UPDATE_ENGINE_CONF,
)

DLC_ID_KEY = 'DLC_ID'
DLC_PACKAGE_KEY = 'DLC_PACKAGE'
DLC_NAME_KEY = 'DLC_NAME'
DLC_APPID_KEY = 'DLC_RELEASE_APPID'

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


class DlcGenerator(object):
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

  def __init__(self, src_dir, sysroot, install_root_dir, fs_type,
               pre_allocated_blocks, version, dlc_id, dlc_package, name):
    """Object initializer.

    Args:
      src_dir: (str) path to the DLC source root directory.
      sysroot: (str) The path to the build root directory.
      install_root_dir: (str) The path to the root installation directory.
      fs_type: (str) file system type.
      pre_allocated_blocks: (int) number of blocks pre-allocated on device.
      version: (str) DLC version.
      dlc_id: (str) DLC ID.
      dlc_package: (str) DLC Package.
      name: (str) DLC name.
    """
    self.src_dir = src_dir
    self.sysroot = sysroot
    self.install_root_dir = install_root_dir
    self.fs_type = fs_type
    self.pre_allocated_blocks = pre_allocated_blocks
    self.version = version
    self.dlc_id = dlc_id
    self.dlc_package = dlc_package
    self.name = name

    self.meta_dir = os.path.join(self.install_root_dir, DLC_META_DIR,
                                 self.dlc_id, self.dlc_package)
    self.image_dir = os.path.join(self.install_root_dir, DLC_IMAGE_DIR,
                                  self.dlc_id, self.dlc_package)
    osutils.SafeMakedirs(self.meta_dir)
    osutils.SafeMakedirs(self.image_dir)

    # Create path for all final artifacts.
    self.dest_image = os.path.join(self.image_dir, 'dlc.img')
    self.dest_table = os.path.join(self.meta_dir, 'table')
    self.dest_imageloader_json = os.path.join(self.meta_dir, 'imageloader.json')

    # Log out the member variable values initially set.
    logging.debug('Initial internal values of DlcGenerator: %s',
                  json.dumps(self.__dict__, sort_keys=True))

  def SquashOwnerships(self, path):
    """Squash the owernships & permissions for files.

    Args:
      path: (str) path that contains all files to be processed.
    """
    cros_build_lib.sudo_run(['chown', '-R', '0:0', path])
    cros_build_lib.sudo_run(
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
      cros_build_lib.run(
          ['/sbin/mkfs.ext4', '-b', str(self._BLOCK_SIZE), '-O',
           '^has_journal', self.dest_image], capture_output=True)
      # Create the mount_point directory.
      osutils.SafeMakedirs(mount_point)
      # Mount the ext4 image.
      osutils.MountDir(self.dest_image, mount_point, mount_opts=('loop', 'rw'))

      try:
        self.SetupDlcImageFiles(mount_point)
      finally:
        # Unmount the ext4 image.
        osutils.UmountDir(mount_point)
      # Shrink to minimum size.
      cros_build_lib.run(
          ['/sbin/e2fsck', '-y', '-f', self.dest_image], capture_output=True)
      cros_build_lib.run(
          ['/sbin/resize2fs', '-M', self.dest_image], capture_output=True)

  def CreateSquashfsImage(self):
    """Create a squashfs image."""
    with osutils.TempDir(prefix='dlc_') as temp_dir:
      squashfs_root = os.path.join(temp_dir, 'squashfs-root')
      self.SetupDlcImageFiles(squashfs_root)

      cros_build_lib.run(['mksquashfs', squashfs_root, self.dest_image,
                          '-4k-align', '-noappend'], capture_output=True)

      # We changed the ownership and permissions of the squashfs_root
      # directory. Now we need to remove it manually.
      osutils.RmDir(squashfs_root, sudo=True)

  def SetupDlcImageFiles(self, dlc_dir):
    """Prepares the directory dlc_dir with all the files a DLC needs.

    Args:
      dlc_dir: (str) The path to where to setup files inside the DLC.
    """
    dlc_root_dir = os.path.join(dlc_dir, self._DLC_ROOT_DIR)
    osutils.SafeMakedirs(dlc_root_dir)
    osutils.CopyDirContents(self.src_dir, dlc_root_dir, symlinks=True)
    self.PrepareLsbRelease(dlc_dir)
    self.CollectExtraResources(dlc_dir)
    self.SquashOwnerships(dlc_dir)

  def PrepareLsbRelease(self, dlc_dir):
    """Prepare the file /etc/lsb-release in the DLC module.

    This file is used dropping some identification parameters for the DLC.

    Args:
      dlc_dir: (str) The path to root directory of the DLC. e.g. mounted point
          when we are creating the image.
    """
    # Reading the platform APPID and creating the DLC APPID.
    platform_lsb_release = osutils.ReadFile(os.path.join(self.sysroot,
                                                         LSB_RELEASE))
    app_id = None
    for line in platform_lsb_release.split('\n'):
      if line.startswith(cros_set_lsb_release.LSB_KEY_APPID_RELEASE):
        app_id = line.split('=')[1]
    if app_id is None:
      raise Exception('%s does not have a valid key %s' %
                      (platform_lsb_release,
                       cros_set_lsb_release.LSB_KEY_APPID_RELEASE))

    fields = (
        (DLC_ID_KEY, self.dlc_id),
        (DLC_PACKAGE_KEY, self.dlc_package),
        (DLC_NAME_KEY, self.name),
        # The DLC appid is generated by concatenating the platform appid with
        # the DLC ID using an underscore. This pattern should never be changed
        # once set otherwise it can break a lot of things!
        (DLC_APPID_KEY, '%s_%s' % (app_id, self.dlc_id)),
    )

    lsb_release = os.path.join(dlc_dir, LSB_RELEASE)
    osutils.SafeMakedirs(os.path.dirname(lsb_release))
    content = ''.join('%s=%s\n' % (k, v) for k, v in fields)
    osutils.WriteFile(lsb_release, content)

  def CollectExtraResources(self, dlc_dir):
    """Collect the extra resources needed by the DLC module.

    Look at the documentation around _EXTRA_RESOURCES.

    Args:
      dlc_dir: (str) The path to root directory of the DLC. e.g. mounted point
          when we are creating the image.
    """
    for r in _EXTRA_RESOURCES:
      source_path = os.path.join(self.sysroot, r)
      target_path = os.path.join(dlc_dir, r)
      osutils.SafeMakedirs(os.path.dirname(target_path))
      shutil.copyfile(source_path, target_path)

  def CreateImage(self):
    """Create the image and copy the DLC files to it."""
    logging.info('Creating the DLC image.')
    if self.fs_type == _EXT4_TYPE:
      self.CreateExt4Image()
    elif self.fs_type == _SQUASHFS_TYPE:
      self.CreateSquashfsImage()
    else:
      raise ValueError('Wrong fs type: %s used:' % self.fs_type)

  def VerifyImageSize(self):
    """Verify the image can fit to the reserved file."""
    logging.info('Verifying the DLC image size.')
    image_bytes = os.path.getsize(self.dest_image)
    preallocated_bytes = self.pre_allocated_blocks * self._BLOCK_SIZE
    # Verifies the actual size of the DLC image is NOT smaller than the
    # preallocated space.
    if preallocated_bytes < image_bytes:
      raise ValueError(
          'The DLC_PREALLOC_BLOCKS (%s) value set in DLC ebuild resulted in a '
          'max size of DLC_PREALLOC_BLOCKS * 4K (%s) bytes the DLC image is '
          'allowed to occupy. The value is smaller than the actual image size '
          '(%s) required. Increase DLC_PREALLOC_BLOCKS in your ebuild to at '
          'least %d.' % (
              self.pre_allocated_blocks, preallocated_bytes, image_bytes,
              self.GetOptimalImageBlockSize(image_bytes)))

  def GetOptimalImageBlockSize(self, image_bytes):
    """Given the image bytes, get the least amount of blocks required."""
    return int(math.ceil(image_bytes / self._BLOCK_SIZE))

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
        'package': self.dlc_package,
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
    logging.info('Generating DLC image verity.')
    with osutils.TempDir(prefix='dlc_') as temp_dir:
      hash_tree = os.path.join(temp_dir, 'hash_tree')
      # Get blocks in the image.
      blocks = math.ceil(
          os.path.getsize(self.dest_image) / self._BLOCK_SIZE)
      result = cros_build_lib.run(
          ['verity', 'mode=create', 'alg=sha256', 'payload=' + self.dest_image,
           'payload_blocks=' + str(blocks), 'hashtree=' + hash_tree,
           'salt=random'], capture_output=True)
      table = result.output

      # Append the merkle tree to the image.
      osutils.WriteFile(self.dest_image, osutils.ReadFile(hash_tree, mode='rb'),
                        mode='a+b')

      # Write verity parameter to table file.
      osutils.WriteFile(self.dest_table, table, mode='wb')

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
    # Verify the image created is within preallocated size.
    self.VerifyImageSize()
    # Generate hash tree and other metadata.
    self.GenerateVerity()


def CopyAllDlcs(sysroot, install_root_dir):
  """Copies all DLC image files into the images directory.

  Copies the DLC image files in the given build directory into the given DLC
  image directory. If the DLC build directory does not exist, or there is no DLC
  for that board, this function does nothing.

  Args:
    sysroot: Path to directory containing DLC images, e.g /build/<board>.
    install_root_dir: Path to DLC output directory,
        e.g. src/build/images/<board>/<version>.
  """
  build_dir = os.path.join(sysroot, DLC_IMAGE_DIR)
  output_dir = os.path.join(install_root_dir, 'dlc')

  if not os.path.exists(build_dir):
    logging.info('DLC build directory (%s) does not exists, ignorning.',
                 build_dir)
    return

  if not os.listdir(build_dir):
    logging.info('There are no DLC(s) to copy to output, ignoring.')
    return

  logging.info('Copying all DLC images from %s to %s.', build_dir, output_dir)
  logging.info('Detected the following DLCs: %s',
               ', '.join(os.listdir(build_dir)))

  osutils.SafeMakedirs(output_dir)
  osutils.CopyDirContents(build_dir, output_dir)

  logging.info('Done copying the DLCs to their destination.')

def GetParser():
  """Creates an argument parser and returns it."""
  parser = commandline.ArgumentParser(description=__doc__)
  # This script is used both for building an individual DLC or copying all final
  # DLCs images to their final destination nearby chromiumsos_test_image.bin,
  # etc. These two arguments are required in both cases.
  parser.add_argument('--sysroot', type='path', metavar='DIR', required=True,
                      help="The root path to the board's build root, e.g. "
                      '/build/eve')
  parser.add_argument('--install-root-dir', type='path', metavar='DIR',
                      required=True,
                      help='If building a specific DLC, it is the root path to'
                      ' install DLC images (%s) and metadata (%s). Otherwise it'
                      ' is the target directory where the Chrome OS images gets'
                      ' dropped in build_image, e.g. '
                      'src/build/images/<board>/latest.' % (DLC_IMAGE_DIR,
                                                            DLC_META_DIR))

  one_dlc = parser.add_argument_group('Arguments required for building only '
                                      'one DLC')
  one_dlc.add_argument('--src-dir', type='path', metavar='SRC_DIR_PATH',
                       help='Root directory path that contains all DLC files '
                       'to be packed.')
  one_dlc.add_argument('--pre-allocated-blocks', type=int,
                       metavar='PREALLOCATEDBLOCKS',
                       help='Number of blocks (block size is 4k) that need to'
                       'be pre-allocated on device.')
  one_dlc.add_argument('--version', metavar='VERSION', help='DLC Version.')
  one_dlc.add_argument('--id', metavar='ID', help='DLC ID (unique per DLC).')
  one_dlc.add_argument('--package', metavar='PACKAGE',
                       help='The package ID that is unique within a DLC, One'
                       ' DLC cannot have duplicate package IDs.')
  one_dlc.add_argument('--name', metavar='NAME',
                       help='A human-readable name for the DLC.')
  one_dlc.add_argument('--fs-type', metavar='FS_TYPE', default=_SQUASHFS_TYPE,
                       choices=(_SQUASHFS_TYPE, _EXT4_TYPE),
                       help='File system type of the image.')
  return parser


def ValidateArguments(opts):
  """Validates the correctness of the passed arguments.

  Args:
    opts: Parsed arguments.
  """
  # Make sure if the intention is to build one DLC, all the required arguments
  # are passed.
  per_dlc_req_args = ('src_dir', 'pre_allocated_blocks', 'version', 'id',
                      'package', 'name')
  if (opts.id and
      not all(vars(opts)[arg] is not None for arg in per_dlc_req_args)):
    raise Exception('If the intention is to build only one DLC, all the flags'
                    '%s required for it should be passed .' % per_dlc_req_args)

  if opts.fs_type == _EXT4_TYPE:
    raise Exception('ext4 unsupported for DLC, see https://crbug.com/890060')


def main(argv):
  opts = GetParser().parse_args(argv)
  opts.Freeze()

  ValidateArguments(opts)

  if opts.id:
    logging.info('Building DLC %s', opts.id)
    dlc_generator = DlcGenerator(src_dir=opts.src_dir,
                                 sysroot=opts.sysroot,
                                 install_root_dir=opts.install_root_dir,
                                 fs_type=opts.fs_type,
                                 pre_allocated_blocks=opts.pre_allocated_blocks,
                                 version=opts.version,
                                 dlc_id=opts.id,
                                 dlc_package=opts.package,
                                 name=opts.name)
    dlc_generator.GenerateDLC()
  else:
    CopyAllDlcs(opts.sysroot, opts.install_root_dir)
