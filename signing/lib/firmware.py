# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ChromeOS firmware Signers"""

from __future__ import print_function

import csv
import glob
import os
import re
import shutil
import tempfile

from chromite.lib import cgpt
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.signing.lib import signer


class BiosSigner(signer.FutilitySigner):
  """Sign bios.bin file using futility."""

  required_keys_private = ('firmware_data_key',)
  required_keys_public = ('kernel_subkey',)
  required_keyblocks = ('firmware_data_key',)

  def __init__(self, sig_id='', sig_dir='', preamble_flags=None):
    """Init BiosSigner

    Args:
      sig_id: Signature ID (aka loem id)
      sig_dir: Signature Output Directory (i.e shellball/keyset)
      preamble_flags: preamble flags passed to futility
    """
    self.sig_id = sig_id
    self.sig_dir = sig_dir
    self.preamble_flags = preamble_flags

  def GetFutilityArgs(self, keyset, input_name, output_name):
    """Returns futility arguments for signing bios

    Args:
      keyset: keyset used for signing
      input_name: bios image
      output_name: output firmware file
    """
    fw_key = keyset.keys['firmware_data_key']
    kernel_key = keyset.keys['kernel_subkey']
    dev_fw_key = keyset.keys.get('dev_firmware_data_key', fw_key)

    args = ['sign',
            '--type', 'bios',
            '--signprivate', fw_key.private,
            '--keyblock', fw_key.keyblock,
            '--kernelkey', kernel_key.public,
            '--version', fw_key.version,
            '--devsign', dev_fw_key.private,
            '--devkeyblock', dev_fw_key.keyblock]

    if self.preamble_flags is not None:
      args += ['--flags', str(self.preamble_flags)]

    # Add loem related arguments
    if self.sig_id and self.sig_dir:
      args += ['--loemdir', self.sig_dir,
               '--loemid', self.sig_id]

    # Add final input/output arguments
    args += [input_name, output_name]

    return args


class ECSigner(signer.BaseSigner):
  """Sign EC bin file."""

  required_keys_private = ('key_ec_efs',)

  def IsROSigned(self, ec_image):
    """Returns True if the given ec.bin is RO signed"""

    # Check fmap for KEY_RO
    fmap = cros_build_lib.RunCommand(['futility', 'dump_fmap', '-p', ec_image],
                                     capture_output=True)

    return re.search('KEY_RO', fmap.output) is not None

  def Sign(self, keyset, input_name, output_name):
    """"Sign EC image

    Args:
      keyset: keyset used for this signing step
      input_name: ec image path to be signed (i.e. to ec.bin)
      output_name: bios image path to be updated with new hashes

    Raises:
      SigningFailedError: if a signing fails
    """
    # Use absolute paths since we use a temp directory
    ec_path = os.path.abspath(input_name)
    bios_path = os.path.abspath(output_name)

    if self.IsROSigned(ec_path):
      # Only sign if not read-only, nothing to do
      return

    logging.info('Signing EC %s', ec_path)

    # Run futility in temp_dir to avoid cwd artifacts
    with osutils.TempDir() as temp_dir:
      ec_rw_bin = os.path.join(temp_dir, 'EC_RW.bin')
      ec_rw_hash = os.path.join(temp_dir, 'EC_RW.hash')
      try:
        cros_build_lib.RunCommand(['futility', 'sign',
                                   '--type', 'rwsig',
                                   '--prikey',
                                   keyset.keys['key_ec_efs'].private,
                                   ec_path],
                                  cwd=temp_dir)

        cros_build_lib.RunCommand(['openssl', 'dgst', '-sha256', '-binary',
                                   ec_rw_bin],
                                  log_stdout_to_file=ec_rw_hash,
                                  cwd=temp_dir)

        cros_build_lib.RunCommand(['store_file_in_cbfs', bios_path,
                                   ec_rw_bin, 'ecrw'])

        cros_build_lib.RunCommand(['store_file_in_cbfs', bios_path,
                                   ec_rw_hash, 'ecrw.hash'])

      except cros_build_lib.RunCommandError as err:
        logging.warning('Signing EC failed: %s', str(err))
        raise signer.SigningFailedError('Signing EC failed')


class GBBSigner(signer.FutilitySigner):
  """Sign GBB"""
  required_keys_public = ('recovery_key',)
  required_keys_private = ('root_key',)

  def GetFutilityArgs(self, keyset, input_name, output_name):
    """Return args for signing GBB

    Args:
      keyset: Keyset used for signing
      input_name: Firmware image
      output_name: Bios path (i.e. tobios.bin)
    """
    return ['gbb',
            '--set',
            '--recoverykey=' + keyset.keys['recovery_key'].public,
            input_name,
            output_name]


class FirmwareSigner(signer.BaseSigner):
  """Signs all firmware related to the given configuration."""

  required_keys_private = (BiosSigner.required_keys_private +
                           GBBSigner.required_keys_private)

  required_keys_public = (BiosSigner.required_keys_public +
                          GBBSigner.required_keys_public)

  required_keyblocks = (BiosSigner.required_keyblocks +
                        GBBSigner.required_keyblocks)

  def SignOne(self, keyset, shellball_dir, bios_image, ec_image='',
              model_name='', key_id='', keyset_out_dir='keyset'):
    """Perform one signing based on the given args.

    Args:
      keyset: master keyset used for signing,
      shellball_dir: location of extracted shellball
      bios_image: relitive path of bios.bin in shellball
      ec_image: relative path of ec.bin in shellball
      model_name: name of target's model_name as define in signer_config.csv
      key_id: subkey id to be used for signing
      keyset_out_dir: relative path of keyset output dir in shellball

    Raises:
      SigningFailedError: if a signing fails
    """

    if key_id:
      keyset = keyset.GetSubKeyset(key_id)

    shellball_keydir = os.path.join(shellball_dir, keyset_out_dir)
    osutils.SafeMakedirs(shellball_keydir)

    if model_name:
      shutil.copy(keyset.keys['root_key'].public,
                  os.path.join(shellball_dir, 'rootkey.' + model_name))

    bios_path = os.path.join(shellball_dir, bios_image)

    if ec_image:
      ec_path = os.path.join(shellball_dir, ec_image)
      logging.info('Signing EC: %s', ec_path)
      ECSigner().Sign(keyset, ec_path, bios_path)

    logging.info('Signing BIOS: %s', bios_path)
    with tempfile.NamedTemporaryFile() as temp_fw:
      bios_signer = BiosSigner(sig_id=model_name, sig_dir=shellball_keydir)
      bios_signer.Sign(keyset, bios_path, temp_fw.name)

      GBBSigner().Sign(keyset, temp_fw.name, bios_path)

  def Sign(self, keyset, input_name, output_name):
    """Sign Firmware shellball.

    Signing is based on if 'signer_config.csv', then all rows defined in file
    are signed. Else all bios*.bin in shellball will be signed.

    Args:
      keyset: master keyset, with subkeys[key_id] if defined
      input_name: location of extracted shellball
      output_name: unused

    Raises:
      SigningFailedError: if a signing step fails
    """
    shellball_dir = input_name
    signerconfig_csv = os.path.join(shellball_dir, 'signer_config.csv')
    if os.path.exists(signerconfig_csv):
      with open(signerconfig_csv) as csv_file:
        signerconfigs = SignerConfigsFromCSV(csv_file)

      for signerconfig in signerconfigs:
        self.SignOne(keyset,
                     shellball_dir,
                     signerconfig['firmware_image'],
                     ec_image=signerconfig['ec_image'],
                     model_name=signerconfig['model_name'],
                     key_id=signerconfig['key_id'])
    else:
      # Sign all ./bios*.bin
      for bios_path in glob.glob(os.path.join(input_name, 'bios*.bin')):
        key_id_match = re.match(r'.*bios\.(\w+)\.bin', bios_path)
        key_id = key_id_match.group(1) if key_id_match else ''
        keyset_out_dir = 'keyset.' + key_id if key_id else 'keyset'
        self.SignOne(keyset,
                     shellball_dir,
                     bios_path,
                     model_name=key_id,
                     key_id=key_id,
                     keyset_out_dir=keyset_out_dir)


class ShellballError(Exception):
  """Error occurred with firmware shellball"""


class ShellballExtractError(ShellballError):
  """Raised when extracting fails."""


class ShellballRepackError(ShellballError):
  """Raised when repacking fails."""


class Shellball(object):
  """Firmware shellball image created from pack_firmware.

  Can be called as a Context Manager which will extract itself to a temp
  directory and repack itself on exit.

  https://sites.google.com/a/google.com/chromeos-partner/platforms/creating-a-firmware-updater
  """

  def __init__(self, filename):
    """Initial Shellball, no disk changes.

    Args:
      filename: filename of shellball
    """
    self.filename = filename

    self._extract_dir = None

  def __enter__(self):
    """Extract the shellball to a temp directory, returns directory."""
    self._extract_dir = osutils.TempDir()
    self.Extract(self._extract_dir.tempdir)
    return self._extract_dir.tempdir

  def __exit__(self, exc_type, exc_value, traceback):
    """Repack shellball and delete temp directory."""
    try:
      if exc_type is None:
        self.Repack(self._extract_dir.tempdir)

    finally:
      if self._extract_dir:
        # Always clear up temp directory
        self._extract_dir.Cleanup()

  def Extract(self, out_dir):
    """Extract self to given directory, raises ExtractFail on fail"""
    try:
      self._Run('--sb_extract', out_dir)

    except cros_build_lib.RunCommandError as err:
      logging.error("Extracting firmware shellball failed")
      raise ShellballExtractError(err.msg)

  def Repack(self, src_dir):
    """Repack shellball with the given directory, raises RepackFailed on fail.

    Only supports shellballs that honor '--sb_repack' which should include
    everything that has been signed since 2014
    """
    with tempfile.NamedTemporaryFile(delete=False) as tmp_file:
      orig_file = self.filename
      self.filename = tmp_file.name
      try:
        shutil.copy(orig_file, tmp_file.name)
        self._Run('--sb_repack', src_dir)
        shutil.move(tmp_file.name, orig_file)

      except cros_build_lib.RunCommandError as err:
        logging.error("Repacking firmware shellball failed")
        raise ShellballRepackError(err.msg)

      finally:
        self.filename = orig_file

        # Clean up file if still exists
        if os.path.exists(tmp_file.name):
          os.remove(tmp_file.name)


  def _Run(self, *args):
    """Execute shellball with given arguments."""
    cmd = [os.path.realpath(self.filename)]
    cmd += args
    cros_build_lib.RunCommand(cmd)


def ResignImageFirmware(image_file, keyset):
  """Resign the given firmware image.

  Raises SignerFailedError
  """
  image_disk = cgpt.Disk(image_file)

  with osutils.TempDir() as rootfs_dir:
    with osutils.MountImagePartition(image_file,
                                     image_disk.GetPartitionByLabel('ROOT-A'),
                                     rootfs_dir):
      sb_file = os.path.join(rootfs_dir, 'usr/sbin/chromeos-firmware')
      if os.path.exists(sb_file):
        logging.info("Found firmware, signing")
        with Shellball(sb_file) as sb_dir:
          fw_signer = FirmwareSigner()
          if not fw_signer.Sign(keyset, sb_dir, None):
            raise signer.SigningFailedError('Signing Firmware Image Failed: %s'
                                            % sb_file)

          version_signer_path = os.path.join(sb_dir, 'VERSION.signer')
          with open(version_signer_path, 'w') as version_signer:
            WriteSignerNotes(keyset, version_signer)
      else:
        logging.warning("No firmware found in image. Not signing firmware")


def SignerConfigsFromCSV(signer_config_file):
  """Returns list of SignerConfigs from a signer_config.csv file

  CSV should have a header with fields model_name, firmware_image, key_id, and
  ec_image

  go/cros-unibuild-signing
  """
  csv_reader = csv.DictReader(signer_config_file)

  for field in ('model_name', 'firmware_image', 'key_id', 'ec_image'):
    if field not in csv_reader.fieldnames:
      raise csv.Error('Missing field: ' + field)

  return list(csv_reader)


def WriteSignerNotes(keyset, outfile):
  """Writes signer notes (a.k.a. VERSION.signer) to file.

  Args:
    keyset: keyset used for generating signer file.
    outfile: file object that signer notes are written to.
  """
  recovery_key = keyset.keys['recovery_key']
  outfile.write('Signed with keyset in %s\n' % recovery_key.keydir)
  outfile.write('recovery: %s\n' % recovery_key.GetSHA1sum())

  root_key = keyset.keys['root_key']
  if root_key.subkeys:
    outfile.write('List sha1sum of all loem/model\'s signatures:\n')
    for key_id, key in root_key.subkeys.items():
      outfile.write('%s: %s\n' % (key_id, key.GetSHA1sum()))
  else:
    outfile.write('root: %s\n' % root_key.GetSHA1sum())
