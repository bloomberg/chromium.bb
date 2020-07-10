# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome OS Image file signing."""

from __future__ import print_function

import glob
import os
import re
import tempfile

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import image_lib
from chromite.lib import kernel_cmdline
from chromite.lib import osutils
from chromite.signing.lib import firmware
from chromite.signing.lib import keys
from chromite.utils import key_value_store


class Error(Exception):
  """Base exception for all exceptions in this module."""


class SignImageError(Error):
  """Error occurred within SignImage."""


def _PathForVbootSigningScripts(path=None):
  """Get extra_env for finding vboot_reference scripts.

  Args:
    path: path to vboot_reference/scripts/image_signing.

  Returns:
    Dictionary to pass to run's extra_env so that it finds the scripts.
  """
  if not path:
    path = os.path.join(constants.SOURCE_ROOT,
                        'src/platform/vboot_reference/scripts/image_signing')
  current_path = os.environ.get('PATH', '').split(':')
  if path not in current_path:
    current_path.insert(0, path)
  return {'PATH': ':'.join(current_path)}


def GetKernelConfig(loop_kern, check=True):
  """Get the kernel config for |loop_kern|.

  Args:
    loop_kern: Device file for the partition to inspect.
    check: Whether failure to read the command line is acceptable.

  Returns:
    String containing the kernel arguments, or None.
  """
  ret = cros_build_lib.sudo_run(
      ['dump_kernel_config', loop_kern],
      print_cmd=False, capture_output=True,
      check=check, encoding='utf-8')
  if ret.returncode:
    return None
  return ret.output.strip()


def _GetKernelCmdLine(loop_kern, check=True):
  """Get the kernel commandline for |loop_kern|.

  Args:
    loop_kern: Device file for the partition to inspect.
    check: Whether failure to read the command line is acceptable.

  Returns:
    CommandLine() containing the kernel config.
  """
  config = GetKernelConfig(loop_kern, check)
  if config is None:
    return None
  else:
    return kernel_cmdline.CommandLine(config)


def SignImage(image_type, input_file, output_file, kernel_part_id, keydir,
              keyA_prefix='', vboot_path=None):
  """Sign the image file.

  A Chromium OS image file (INPUT) always contains 2 partitions (kernel A & B).
  This function will rebuild hash data by DM_PARTNO, resign kernel partitions by
  their KEYBLOCK and PRIVKEY files, and then write to OUTPUT file. Note some
  special images (specified by IMAGE_TYPE, like 'recovery' or 'factory_install')
  may have additional steps (ex, tweaking verity hash or not stripping files)
  when generating output file.

  Args:
    image_type: Type of image (e.g., 'factory', 'recovery').
    input_file: Image to sign. (read-only: copied to output_file).
    output_file: Signed image. (file is created here)
    kernel_part_id: partition number (or name) for the kernel (usually 2,
        4 on recovery media.)
    keydir: Path of keyset dir to use.
    keyA_prefix: Prefix for kernA key (e.g., 'recovery_').
    vboot_path: Vboot_reference/scripts/image_signing dir path.

  Raises SignImageException
  """
  extra_env = _PathForVbootSigningScripts(vboot_path)
  logging.info('Preparing %s image...', image_type)
  cros_build_lib.run(['cp', '--sparse=always', input_file, output_file])

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
      # more of the shell functions.
      #
      # We do not actually want to have any filesystems mounted at this point.

      loop_kernA = image.GetPartitionDevName('KERN-A')
      loop_rootfs = image.GetPartitionDevName('ROOT-A')
      loop_kern = image.GetPartitionDevName(kernel_part_id)
      kernA_cmd = _GetKernelCmdLine(loop_kernA)
      if (image_type != 'factory_install' and
          not kernA_cmd.GetKernelParameter('cros_legacy') and
          not kernA_cmd.GetKernelParameter('cros_efi')):
        cros_build_lib.run(
            ['strip_boot_from_image.sh', '--image', loop_rootfs],
            extra_env=extra_env)
      ClearResignFlag(image)
      UpdateRootfsHash(image, loop_kern, keyset, keyA_prefix)
      UpdateStatefulPartitionVblock(image, keyset)
      if image_type == 'recovery':
        UpdateRecoveryKernelHash(image, keyset)
      UpdateLegacyBootloader(image, loop_kern)

  logging.info('Signed %s image written to %s', image_type, output_file)


def SignAndroidImage(rootfs_dir, keyset, vboot_path=None):
  """If there is an android image, sign it."""
  system_img = os.path.join(
      rootfs_dir, 'opt/google/containers/android/system.raw.img')
  if not os.path.exists(system_img):
    logging.info('ARC image not found.  Not signing Android APKs.')
    return

  arc_version = key_value_store.LoadFile(os.path.join(
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
  cros_build_lib.run(
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
  cros_build_lib.run(
      ['install_gsetup_certs.sh', uefi_fsdir, uefi_keydir],
      extra_env=extra_env)
  # TODO(lamontjones): convert sign_uefi.sh to python.
  cros_build_lib.run(
      ['sign_uefi.sh', uefi_fsdir, uefi_keydir],
      extra_env=extra_env)

  # TODO(lamontjones): convert sign_uefi.sh to python.
  cros_build_lib.run(
      ['sign_uefi.sh', os.path.join(rootfs_dir, 'boot'), uefi_keydir],
      extra_env=extra_env)

  logging.info('Signed UEFI binaries.')


class CalculateRootfsHash(object):
  """Hash info, and other facts about it, suitable for comparison or copying.

  Instantiating this class causes it to calculate a new DmConfig and
  CommandLine for the given image, as well as creating a file with the
  new hashtree for the image.  The temporary file is deleted along with the
  instance.

  Examples:
    (See UpdateRootfsHash below)
    image = image_lib.LoopbackPartitions(image_path)
    rootfs_hash = CalculateRootfsHash(
        image, kernel_cmdline.CommandLine(image.GetPartitionDevName('KERN-A')))
    <copy or compare updated hashtree, dm_config, kernel_cmdline to the image>
    <do other things, confident that when rootfs_hash is garbage collected, the
    underlying new hashtree file will be deleted.>

  Attributes:
    calculated_dm_config: Updated DmConfig for the kernel
    calculated_kernel_cmdline: New kernel_cmdline.CommandLine
    hashtree_filename: Name of the temporary file containing the new hashtree.
  """

  def __init__(self, image, cmd_line):
    """Create the hash_image for the rootfs.

    Args:
      image: image_lib.LoopbackPartitions() for the image.
      cmd_line: kernel_cmdline.CommandLine for the kernel.
    """
    self.image = image
    self.cmd_line = cmd_line
    loop_rootfs = image.GetPartitionDevName('ROOT-A')
    self._file = tempfile.NamedTemporaryFile(
        dir=image.destination, delete=False)
    dm_config = cmd_line.GetDmConfig()
    if not dm_config:
      logging.warning(
          "Couldn't grab dm_config. Aborting rootfs hash calculation.")
      # TODO(lamontjones): This should probably raise an exception.
      return
    vroot_dev = dm_config.devices['vroot']
    # Get the verity args from the existing DmConfig.
    rootfs_blocks = int(vroot_dev.GetVerityArg('hashstart').value) // 8
    alg = vroot_dev.GetVerityArg('alg').value
    root_dev = vroot_dev.GetVerityArg('payload').value
    hash_dev = vroot_dev.GetVerityArg('hashtree').value
    salt = vroot_dev.GetVerityArg('salt')

    cmd = ['verity', 'mode=create',
           'alg=%s' % alg,
           'payload=%s' % loop_rootfs,
           'payload_blocks=%d' % rootfs_blocks,
           'hashtree=%s' % self._file.name]
    if salt:
      cmd.append('salt=%s' % salt.value)
    verity = cros_build_lib.sudo_run(
        cmd, print_cmd=False, capture_output=True, encoding='utf-8').stdout
    # verity is a templated DmLine string.
    slave = kernel_cmdline.DmLine(
        verity.replace('ROOT_DEV', root_dev).replace('HASH_DEV', hash_dev))
    vroot_dev.rows[0] = slave
    self.calculated_dm_config = dm_config
    self.calculated_kernel_cmdline = self.cmd_line
    self.hashtree_filename = self._file.name

  def __del__(self):
    if getattr(self, '_file', None):
      os.unlink(self._file.name)
      del self._file


def ClearResignFlag(image):
  """Remove any /root/.need_to_be_signed file from the rootfs.

  Args:
    image: image_lib.LoopbackPartitions instance for this image.
  """
  # Check and clear the need_to_resign tag file.
  rootfs_dir = image.Mount(('ROOT-A',), mount_opts=('rw',))[0]
  needs_to_be_signed = os.path.join(rootfs_dir, 'root/.need_to_be_signed')
  if os.path.exists(needs_to_be_signed):
    image.Mount(('ROOT-A',), mount_opts=('remount', 'rw'))
    osutils.SafeUnlink(needs_to_be_signed, sudo=True)
  image.Unmount(('ROOT-A',))


def UpdateRootfsHash(image, loop_kern, keyset, keyA_prefix):
  """Update the root filesystem hash.

  Args:
    image: image_lib.LoopbackPartitions instance for this image.
    loop_kern: Device file name for the kernel partition to hash.
    keyset: Kernel_cmdline.Keyset to use.
    keyA_prefix: Prefix for kernA key (e.g., 'recovery_').
  """
  logging.info(
      'Updating rootfs hash and updating cmdline for kernel partitions')
  logging.info(
      '%s (in %s) keyset=%s keyA_prefix=%s',
      loop_kern, image.path, keyset.key_dir, keyA_prefix)
  cmd_line = _GetKernelCmdLine(loop_kern, check=False)
  if cmd_line:
    dm_config = cmd_line.GetDmConfig()
  if not cmd_line or not dm_config:
    logging.error("Couldn't get dm_config from kernel %s", loop_kern)
    logging.error(' (cmdline: %s)', cmd_line)
    raise SignImageError('Could not get dm_config from kernel')

  loop_rootfs = image.GetPartitionDevName('ROOT-A')
  image.DisableRwMount('ROOT-A')
  rootfs_hash = CalculateRootfsHash(image, cmd_line)
  fsinfo = cros_build_lib.sudo_run(
      ['tune2fs', '-l', image.GetPartitionDevName('ROOT-A')],
      capture_output=True, encoding='utf-8').stdout
  rootfs_blocks = int(re.search(
      r'^Block count: *([0-9]+)$', fsinfo, flags=re.MULTILINE).group(1))
  rootfs_sectors = 8 * rootfs_blocks

  # Overwrite the appended hashes in the rootfs.
  cros_build_lib.sudo_run(
      ['dd', 'if=%s' % rootfs_hash.hashtree_filename, 'of=%s' % loop_rootfs,
       'bs=512', 'seek=%d' % rootfs_sectors, 'conv=notrunc'],
      redirect_stderr=True)

  # Update kernel command lines.
  for kern in ('KERN-A', 'KERN-B'):
    loop_kern = image.GetPartitionDevName(kern)
    new_cmd_line = _GetKernelCmdLine(loop_kern, check=False)
    if not new_cmd_line and kern == 'KERN-B':
      logging.info('Skipping empty KERN-B partition (legacy images).')
      continue
    new_cmd_line.SetDmConfig(rootfs_hash.calculated_dm_config)
    logging.info('New cmdline for %s partition is: %s', kern, new_cmd_line)
    if kern == 'KERN-A':
      key = keyset.keys['%skernel_data_key' % keyA_prefix]
    else:
      key = keyset.keys['kernel_data_key']
    _UpdateKernelConfig(loop_kern, new_cmd_line, key)


def _UpdateKernelConfig(loop_kern, cmdline, key):
  """Update the kernel config for |loop_kern|.

  Args:
    loop_kern: Device file for the partition to inspect.
    cmdline: CommandLine instance to set.
    key: Key to use.
  """
  with tempfile.NamedTemporaryFile() as temp:
    temp.file.write(cmdline.Format().encode('utf-8'))
    temp.file.flush()

    cros_build_lib.sudo_run(
        ['vbutil_kernel', '--repack', loop_kern,
         '--keyblock', key.keyblock,
         '--signprivate', key.private,
         '--version', str(key.version),
         '--oldblob', loop_kern,
         '--config', temp.name])


def UpdateStatefulPartitionVblock(image, keyset):
  """Update the SSD install-able vblock file on stateful partition.

  This is deprecated because all new images should have a SSD boot-able kernel
  in partition 4. However, the signer needs to be able to sign new & old images
  (crbug.com/449450#c13) so we will probably never remove this.

  Args:
    image: image_lib.LoopbackPartitions() for the image.
    keyset: Keyset to use for signing
  """
  with tempfile.NamedTemporaryFile(dir=image.destination) as tmpfile:
    loop_kern = image.GetPartitionDevName('KERN-B')
    ret = _GetKernelCmdLine(loop_kern, check=False)
    if not ret:
      logging.info(
          'Building vmlinuz_hd.vblock from legacy image partition 2.')
      loop_kern = image.GetPartitionDevName(2)

    kernel_key = keyset.keys['kernel_data_key']
    cros_build_lib.sudo_run(['vbutil_kernel', '--repack', tmpfile.name,
                             '--keyblock', kernel_key.keyblock,
                             '--signprivate', kernel_key.private,
                             '--oldblob', loop_kern, '--vblockonly'])
    state_dir = image.Mount(('STATE',), mount_opts=('rw',))[0]
    cros_build_lib.sudo_run(
        ['cp', tmpfile.name, os.path.join(state_dir, 'vmlinuz_hd.vblock')])
    image.Unmount(('STATE',))


def UpdateRecoveryKernelHash(image, keyset):
  """Update the recovery kernel hash."""
  loop_kernA = image.GetPartitionDevName('KERN-A')
  loop_kernB = image.GetPartitionDevName('KERN-B')

  # Update the KERN-B hash in the KERN-A command line.
  kernA_cmd = _GetKernelCmdLine(loop_kernA)
  old_kernB_hash = kernA_cmd.GetKernelParameter('kern_b_hash')

  if old_kernB_hash:
    cmd = 'sha256sum' if len(old_kernB_hash.value) >= 64 else 'sha1sum'
    new_kernB_hash = cros_build_lib.sudo_run(
        [cmd, loop_kernB], redirect_stdout=True,
        encoding='utf-8').stdout.split()[0]
    kernA_cmd.SetKernelParameter('kern_b_hash', new_kernB_hash)
  logging.info('New cmdline for kernel A is %s', str(kernA_cmd))
  recovery_key = keyset.keys['recovery']
  _UpdateKernelConfig(loop_kernA, kernA_cmd, recovery_key)


def UpdateLegacyBootloader(image, loop_kern):
  """Update the legacy bootloader templates in EFI partition."""
  try:
    uefi_dir = image.Mount(('EFI-SYSTEM',))[0]
  except KeyError:
    # Image has no EFI-SYSTEM partition.
    logging.info(
        'Could not mount EFI partition for updating legacy bootloader cfg.')
    raise SignImageError('Could not mount EFI partition')

  root_digest = ''
  cmd_line = _GetKernelCmdLine(loop_kern)
  if cmd_line:
    dm_config = cmd_line.GetDmConfig()
    if dm_config:
      vroot_dev = dm_config.devices['vroot']
      if vroot_dev:
        root_digest = vroot_dev.GetVerityArg('root_hexdigest')
        if root_digest:
          root_digest = root_digest.value
  if not root_digest:
    logging.error('Could not grab root_digest from kernel partition %s',
                  loop_kern)
    logging.error('cmdline: %s', cmd_line)
    raise SignImageError('Could not find root digest')

  files = []
  sys_dir = os.path.join(uefi_dir, 'syslinux')
  if os.path.isdir(sys_dir):
    files += glob.glob(os.path.join(sys_dir, '*.cfg'))
  grub_cfg = os.path.join(uefi_dir, 'efi/boot/grub.cfg')
  if os.path.exists(grub_cfg):
    files.append(grub_cfg)
  if files:
    ret = cros_build_lib.sudo_run(
        ['sed', '-iE',
         r's/\broot_hexdigest=[a-z0-9]+/root_hexdigest=%s/g' % root_digest] +
        files, error_code_ok=True)
    if ret.returncode:
      logging.error('Updating bootloader configs failed: %s', ' '.join(files))
      raise SignImageError('Updating bootloader configs failed')


def DumpConfig(image_file):
  """Dump kernel config for both kernels.

  This implements the necessary logic for bin/dump_config, which is intended
  primarily for debugging of images.

  Args:
    image_file: path to the image file from which to dump kernel configs.
  """
  with image_lib.LoopbackPartitions(image_file) as image:
    for kernel_part in ('KERN-A', 'KERN-B'):
      loop_kern = image.GetPartitionDevName(kernel_part)
      config = GetKernelConfig(loop_kern, check=False)
      if config:
        logging.info('Partition %s', kernel_part)
        logging.info(config)
      else:
        logging.info('Partition %s has no configuration.', kernel_part)
