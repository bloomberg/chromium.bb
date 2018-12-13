# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome OS Image file signing."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import image_lib
from chromite.lib import osutils
from chromite.signing.lib import firmware
from chromite.signing.lib import keys


class Error(Exception):
  """Base exception for all exceptions in this module."""


class SignImageError(Error):
  """Error occurred within SignImage"""


def _PathForVbootSigningScripts(path=None):
  """Get extra_env for finding vboot_reference scripts.

  Args:
    path: path to vboot_reference/scripts/image_signing.

  Returns:
    Dictionary to pass to RunCommand's extra_env so that it finds the scripts.
  """
  if not path:
    path = os.path.join(constants.SOURCE_ROOT,
                        'src/platform/vboot_reference/scripts/image_signing')
  current_path = os.environ.get('PATH', '').split(':')
  if path not in current_path:
    current_path.insert(0, path)
  return {'PATH': ':'.join(current_path)}


def SignImage(image_type, input_file, output_file, kernel_part_num, keydir,
              keyA_prefix='', vboot_path=None):
  """Sign the image file.

  A Chromium OS image file (INPUT) always contains 2 partitions (kernel A & B).
  This function will rebuild hash data by DM_PARTNO, resign kernel partitions by
  their KEYBLOCK and PRIVKEY files, and then write to OUTPUT file. Note some
  special images (specified by IMAGE_TYPE, like 'recovery' or 'factory_install')
  may have additional steps (ex, tweaking verity hash or not stripping files)
  when generating output file.

  Args:
    image_type: Type of image (e.g., 'factory', 'recovery')
    input_file: image to sign. (read-only: copied to output_file)
    output_file: signed image. (file is created here)
    kernel_part_num: partition number (or name) for the kernel (usually 2,
        sometimes 4)
    keydir: path of keyset dir to use.
    keyA_prefix: prefix for kernA key (e.g., 'recovery_')
    vboot_path: optional: vboot_reference/scripts/image_signing dir path

  Raises SignImageException
  """
  extra_env = _PathForVbootSigningScripts(vboot_path)
  # TODO(lamontjones): consider extending osutils to have a sparse file copy
  # function.
  logging.info('Preparing %s image...', image_type)
  cros_build_lib.RunCommand(
      ['cp', '--sparse=always', input_file, output_file])

  keyset = keys.Keyset(keydir)

  firmware.ResignImageFirmware(output_file, keyset)

  with osutils.TempDir() as dest_dir:
    with image_lib.LoopbackPartitions(output_file, dest_dir) as image:
      rootfs_dir = image.Mount(('ROOT-A',))[0]
      SignAndroidImage(rootfs_dir, keyset, vboot_path=vboot_path)
      SignUefiBinaries(image, rootfs_dir, keyset, vboot_path=vboot_path)
      image.Unmount(('ROOT-A',))

      # TODO(lamontjones): From this point on, all we really want at the moment
      # is the loopback devicefile names, but that may change as we implement
      # more of the shell functions.  We do not actually want to have any
      # filesystems mounted at this point.

      loop_kernA = image.GetPartitionDevName('KERN-A')
      loop_rootfs = image.GetPartitionDevName('ROOT-A')
      kernA_config = cros_build_lib.SudoRunCommand(
          ['dump_kernel_config', loop_kernA],
          print_cmd=False, capture_output=True).output.split()
      if (image_type != 'factory_install' and
          'cros_legacy' not in kernA_config and
          'cros_efi' not in kernA_config):
        cros_build_lib.RunCommand(
            ['strip_boot_from_image.sh', '--image', loop_rootfs],
            extra_env=extra_env)

  # TODO(lamontjones): several remaining steps, that all need to be added:
  # UpdateRootfsHash, UpdateStatefulPartitionVblock, UpdateRecoveryKernelHash,
  # UpdateLegacyBootloader
  logging.error('Not completely implemented.')
  logging.debug('kernel_part_num=%d keyA_prefix=%s',
                kernel_part_num, keyA_prefix)
  logging.info('Signed %s image written to %s', image_type, output_file)
  return True


def SignAndroidImage(rootfs_dir, keyset, vboot_path=None):
  """If there is an android image, sign it."""
  system_img = os.path.join(
      rootfs_dir, 'opt/google/containers/android/system.raw.img')
  if not os.path.exists(system_img):
    logging.info('ARC image not found.  Not signing Android APKs.')
    return

  arc_version = cros_build_lib.LoadKeyValueFile(os.path.join(
      rootfs_dir, 'etc/lsb-release')).get('CHROMEOS_ARC_VERSION', '')
  if not arc_version:
    logging.warning('CHROMEOS_ARC_VERSION not found in lsb-release. '
                    'Not signing Android APKs.')
    return

  extra_env = _PathForVbootSigningScripts(vboot_path)
  logging.info('Found ARC image version %s, resigning APKs', arc_version)
  # Sign the Android APKs using ${keyset.key_dir}/android keys.
  android_keydir = os.path.join(keyset.key_dir, 'android')
  logging.info('Using %s', android_keydir)

  # TODO(lamontjones) migrate sign_android_image.sh.
  cros_build_lib.RunCommand(
      ['sign_android_image.sh', rootfs_dir, android_keydir],
      extra_env=extra_env)


def SignUefiBinaries(image, rootfs_dir, keyset, vboot_path=None):
  """Sign UEFI binaries if appropriate."""
  # If there are no uefi keys in the keyset, we're done.
  uefi_keydir = os.path.join(keyset.key_dir, 'uefi')
  if not os.path.isdir(uefi_keydir):
    logging.info('No UEFI keys in keyset. Skipping.')
    return

  # Mount the UEFI partition and sign the contents.
  try:
    uefi_fsdir = image.Mount(('EFI-SYSTEM',))[0]
  except KeyError:
    # Image has no EFI-SYSTEM partition.
    logging.info('No EFI-SYSTEM partition found.')
    return

  extra_env = _PathForVbootSigningScripts(vboot_path)
  # Sign the UEFI binaries on the EFI partition using
  # ${keyset.key_dir}/uefi keys.
  # TODO(lamontjones): convert install_gsetup_certs.sh to python.
  cros_build_lib.RunCommand(
      ['install_gsetup_certs.sh', uefi_fsdir, uefi_keydir],
      extra_env=extra_env)
  # TODO(lamontjones): convert sign_uefi.sh to python.
  cros_build_lib.RunCommand(
      ['sign_uefi.sh', uefi_fsdir, uefi_keydir],
      extra_env=extra_env)

  # TODO(lamontjones): convert sign_uefi.sh to python.
  cros_build_lib.RunCommand(
      ['sign_uefi.sh', os.path.join(rootfs_dir, 'boot'), uefi_keydir],
      extra_env=extra_env)

  logging.info('Signed UEFI binaries.')
