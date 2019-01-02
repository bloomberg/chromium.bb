# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate a Chromium OS update for use by the update engine.

If a source .bin is specified, the update is assumed to be a delta update.
"""

from __future__ import print_function

import os
import shutil
import tempfile

from chromite.lib import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils


_DELTA_GENERATOR = 'delta_generator'


# TODO(tbrindus): move this to paygen/filelib.py.
def CopyFileSegment(in_file, in_mode, in_len, out_file, out_mode, in_seek=0):
  """Simulates a `dd` operation with seeks."""
  with open(in_file, in_mode) as in_stream, \
       open(out_file, out_mode) as out_stream:
    in_stream.seek(in_seek)
    remaining = in_len
    while remaining:
      chunk = in_stream.read(min(8192 * 1024, remaining))
      remaining -= len(chunk)
      out_stream.write(chunk)


# TODO(tbrindus): move this to paygen/filelib.py.
def ExtractPartitionToTempFile(filename, partition, temp_file=None):
  """Extracts |partition| from |filename| into |temp_file|.

  If |temp_file| is not specified, an arbitrary file is used.

  Returns the location of the extracted partition.
  """
  if temp_file is None:
    temp_file = tempfile.mktemp(prefix='cros_generate_update_payload')

  parts = cros_build_lib.GetImageDiskPartitionInfo(filename, unit='B')
  part_info = parts[partition]

  offset = int(part_info.start)
  length = int(part_info.size)

  CopyFileSegment(filename, 'r', length, temp_file, 'w', in_seek=offset)

  return temp_file


def PatchKernel(image, kern_file):
  """Patches kernel |kern_file| with vblock from |image| stateful partition."""
  state_out = ExtractPartitionToTempFile(image, constants.PART_STATE)
  vblock = tempfile.mktemp(prefix='vmlinuz_hd.vblock')
  cros_build_lib.RunCommand(['e2cp', '%s:/vmlinuz_hd.vblock' % state_out,
                             vblock])

  CopyFileSegment(vblock, 'r', os.path.getsize(vblock), kern_file, 'r+')

  osutils.SafeUnlink(state_out)
  osutils.SafeUnlink(vblock)


def ExtractKernel(bin_file, kern_out):
  """Extracts the kernel from the given |bin_file|, into |kern_out|."""
  kern_out = ExtractPartitionToTempFile(bin_file, constants.PART_KERN_B,
                                        kern_out)
  with open(kern_out, 'r') as kern:
    if not any(kern.read(65536)):
      logging.warn('%s: Kernel B is empty, patching kernel A.', bin_file)
      ExtractPartitionToTempFile(bin_file, constants.PART_KERN_A, kern_out)
      PatchKernel(bin_file, kern_out)

  return kern_out


def Ext2FileSystemSize(rootfs):
  """Return the size in bytes of the ext2 filesystem passed in |rootfs|."""
  # dumpe2fs is normally installed in /sbin but doesn't require root.
  dump = cros_build_lib.RunCommand(['/sbin/dumpe2fs', '-h', rootfs],
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


def ExtractRoot(bin_file, root_out, root_pretruncate=None):
  """Extract the rootfs partition from the gpt image |bin_file|.

  Stores it in |root_out|. If |root_out| is empty, a new temp file will be used.
  If |root_pretruncate| is non-empty, saves the pretruncated rootfs partition
  there.
  """
  root_out = ExtractPartitionToTempFile(bin_file, constants.PART_ROOT_A,
                                        root_out)

  if root_pretruncate:
    logging.info('Saving pre-truncated root as %s.', root_pretruncate)
    shutil.copyfile(root_out, root_pretruncate)

  # We only update the filesystem part of the partition, which is stored in the
  # gpt script.
  root_out_size = Ext2FileSystemSize(root_out)
  if root_out_size:
    with open(root_out, 'a') as root:
      logging.info('Root size currently %d bytes.', os.path.getsize(root_out))
      root.truncate(root_out_size)
    logging.info('Truncated root to %d bytes.', root_out_size)
  else:
    raise IOError('Error truncating the rootfs to filesystem size.')

  return root_out


def GenerateUpdatePayload(opts):
  """Generates the output files for the given commandline |opts|."""
  # TODO(tbrindus): we can support calling outside of chroot easily with
  #                 RunCommand, so we can remove this restriction.
  if opts.src_image and not opts.outside_chroot:
    # We need to be in the chroot for generating delta images. By specifying
    # --outside_chroot you can choose not to assert this, which will allow us to
    # run this script outside chroot. Running this script outside chroot
    # requires copying a delta_generator binary and some related shared
    # libraries.
    cros_build_lib.AssertInsideChroot()
  try:
    src_kernel_path = src_root_path = dst_kernel_path = dst_root_path = ''

    if opts.extract:
      if opts.src_image:
        src_kernel_path = opts.src_kern_path or 'old_kern.dat'
        src_root_path = opts.src_root_path or 'old_root.dat'
        ExtractKernel(opts.src_image, src_kernel_path)
        ExtractRoot(opts.src_image, src_root_path)
      if opts.image:
        dst_kernel_path = opts.kern_path or 'new_kern.dat'
        dst_root_path = opts.root_path or 'new_root.dat'
        ExtractKernel(opts.image, dst_kernel_path)
        ExtractRoot(opts.image, dst_root_path, opts.root_pretruncate_path)
      logging.info('Done extracting kernel/root')
      return

    delta, payload_type = (True, 'delta') if opts.src_image else (False, 'full')

    logging.info('Generating %s update', payload_type)

    if delta:
      if not opts.full_kernel:
        src_kernel_path = ExtractKernel(opts.src_image, opts.src_kern_path)
      else:
        logging.info('Generating full kernel update')

      src_root_path = ExtractRoot(opts.src_image, opts.src_root_path)

    dst_kernel_path = ExtractKernel(opts.image, opts.kern_path)
    dst_root_path = ExtractRoot(opts.image, opts.root_path,
                                opts.root_pretruncate_path)

    # TODO(tbrindus): delta_generator should be called with partition lists
    #                 to support major version 2 more easily.
    generator_args = [
        # Common payload args:
        '--major_version=1',
        '--out_file=' + opts.output,
        '--private_key=' + (opts.private_key or ''),
        '--out_metadata_size_file=' + (opts.out_metadata_size_file or ''),
        # Target image args:
        '--new_image=' + dst_root_path,
        '--new_kernel=' + dst_kernel_path,
        '--new_channel=' + opts.channel,
        '--new_board=' + opts.board,
        '--new_version=' + opts.version,
        '--new_key=' + opts.key,
        '--new_build_channel=' + opts.build_channel,
        '--new_build_version=' + opts.build_version,
    ]

    if delta:
      generator_args += [
          # Source image args:
          '--old_image=' + src_root_path,
          '--old_kernel=' + src_kernel_path,
          '--old_channel=' + opts.src_channel,
          '--old_board=' + opts.src_board,
          '--old_version=' + opts.src_version,
          '--old_key=' + opts.src_key,
          '--old_build_channel=' + opts.src_build_channel,
          '--old_build_version=' + opts.src_build_version,
      ]

      # The passed chunk_size is only used for delta payload. Use
      # delta_generator's default if no value is provided.
      if opts.chunk_size:
        logging.info('Forcing chunk size to %d', opts.chunk_size)
        generator_args.append('--chunk_size=%d' % opts.chunk_size)

    # Add partition size. Only *required* for minor_version=1.
    # TODO(tbrindus): deprecate this when we deprecate minor version 1.
    dst_root_part_size = cros_build_lib.GetImageDiskPartitionInfo(
        opts.image, unit='B')[constants.PART_ROOT_A].size

    if dst_root_part_size:
      logging.info('Using rootfs partition size: %d', dst_root_part_size)
      generator_args.append('--rootfs_partition_size=%d' % dst_root_part_size)
    else:
      logging.info('Using the default partition size')

    cros_build_lib.RunCommand([_DELTA_GENERATOR] + generator_args)

    if opts.out_payload_hash_file or opts.out_metadata_hash_file:
      # The out_metadata_hash_file flag requires out_hash_file flag to be set
      # in delta_generator, if caller doesn't provide it, we set it to
      # /dev/null.
      out_payload_hash_file = opts.out_payload_hash_file or '/dev/null'

      # The manifest - unfortunately - contains two fields called
      # signature_offset and signature_size with data about how the manifest is
      # signed. This means we have to pass the signature size used. The value
      # 256 is the number of bytes the SHA-256 hash value of the manifest signed
      # with a 2048-bit RSA key occupies.
      generator_args = [
          '--in_file=' + opts.output,
          '--signature_size=256',
          '--out_hash_file=' + out_payload_hash_file,
          '--out_metadata_hash_file=' + (opts.out_metadata_hash_file or ''),
      ]
      cros_build_lib.RunCommand([_DELTA_GENERATOR] + generator_args)

    logging.info('Done generating %s update', payload_type)
  finally:
    if not opts.src_kern_path:
      osutils.SafeUnlink(src_kernel_path)
    if not opts.src_root_path:
      osutils.SafeUnlink(src_root_path)
    if not opts.kern_path:
      osutils.SafeUnlink(dst_kernel_path)
    if not opts.root_path:
      osutils.SafeUnlink(dst_root_path)


def ParseArguments(argv):
  """Returns a namespace for the CLI arguments."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--image', type='path',
                      help='The image that should be sent to clients.')
  parser.add_argument('--src_image', type='path',
                      help='A source image. If specified, this makes a delta '
                           'update.')
  parser.add_argument('--output', type='path', help='Output file.')
  parser.add_argument('--outside_chroot', action='store_true',
                      help='Running outside of chroot.')
  parser.add_argument('--private_key', type='path',
                      help='Path to private key in .pem format.')
  parser.add_argument('--out_payload_hash_file', type='path',
                      help='Path to output payload hash file.')
  parser.add_argument('--out_metadata_hash_file', type='path',
                      help='Path to output metadata hash file.')
  parser.add_argument('--out_metadata_size_file', type='path',
                      help='Path to output metadata size file.')
  parser.add_argument('--extract', action='store_true',
                      help='If set, extract old/new kernel/rootfs to '
                           '[old|new]_[kern|root].dat. Useful for debugging.')
  parser.add_argument('--full_kernel', action='store_true',
                      help='Generate a full kernel update even if generating a '
                           'delta update.')
  parser.add_argument('--chunk_size', type=int,
                      help='Delta payload chunk size (-1 means whole files).')

  # TODO(tbrindus): Remove --build_version and --src_build_version flags.
  src_group = parser.add_argument_group('Source options')
  src_group.add_argument('--src_channel', type=str, default='',
                         help='Channel of the src image.')
  src_group.add_argument('--src_board', type=str, default='',
                         help='Board of the src image.')
  src_group.add_argument('--src_version', type=str, default='',
                         help='Version of the src image.')
  src_group.add_argument('--src_key', type=str, default='',
                         help='Key of the src image.')
  src_group.add_argument('--src_build_channel', type=str, default='',
                         help='Channel of the build of the src image.')
  src_group.add_argument('--src_build_version', type=str, default='',
                         help='Channel of the build of the the src image.')

  dst_group = parser.add_argument_group('Target options')
  dst_group.add_argument('--channel', type=str, default='',
                         help='Channel of the target image.')
  dst_group.add_argument('--board', type=str, default='',
                         help='Board of the target image.')
  dst_group.add_argument('--version', type=str, default='',
                         help='Version of the target image.')
  dst_group.add_argument('--key', type=str, default='',
                         help='Key of the target image.')
  dst_group.add_argument('--build_channel', type=str, default='',
                         help='Channel of the build of the target image.')
  dst_group.add_argument('--build_version', type=str, default='',
                         help='Channel of the build of the the target image.')

  # Because we archive/call old versions of this script, we can't easily remove
  # command line options, even if we ignore this one now.
  parser.add_argument('--patch_kernel', action='store_true',
                      help='Ignored. Present for compatibility.')

  # Specifying any of the following will cause it to not be cleaned up on exit.
  parser.add_argument('--kern_path', type='path',
                      help='File path for extracting the kernel partition.')
  parser.add_argument('--root_path', type='path',
                      help='File path for extracting the rootfs partition.')
  parser.add_argument('--root_pretruncate_path', type='path',
                      help='File path for extracting the rootfs partition, '
                           'pre-truncation.')
  parser.add_argument('--src_kern_path', type='path',
                      help='File path for extracting the source kernel '
                           'partition.')
  parser.add_argument('--src_root_path', type='path',
                      help='File path for extracting the source root '
                           'partition.')

  opts = parser.parse_args(argv)
  opts.Freeze()

  if not opts.extract and not opts.output:
    parser.error('You must specify an output filename with --output FILENAME')

  return opts


def main(argv):
  opts = ParseArguments(argv)

  return GenerateUpdatePayload(opts)
