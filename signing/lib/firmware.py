# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ChromeOS firmware Signers"""

from __future__ import print_function

import os
import re
import shutil
import tempfile

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.signing.lib import signer


class BiosSigner(signer.FutilitySigner):
  """Sign bios.bin file using futility."""

  _required_keys_private = ('firmware_data_key',)
  _required_keys_public = ('kernel_subkey',)
  _required_keyblocks = ('firmware_data_key',)

  def __init__(self, sig_id='', sig_dir=''):
    """Init BiosSigner

    Args:
      sig_id: Signature ID (aka loem id)
      sig_dir: Signature Output Directory (i.e shellball/keyset)
    """
    self.sig_id = sig_id
    self.sig_dir = sig_dir

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

    # Add loem related arguments
    if self.sig_id and self.sig_dir:
      args += ['--loemdir', self.sig_dir,
               '--loemid', self.sig_id]

    # Add final input/output arguments
    args += [input_name, output_name]

    return args


class ECSigner(signer.BaseSigner):
  """Sign EC bin file."""

  _required_keys_private = ('key_ec_efs',)

  def IsROSigned(self, firmware_image):
    """Returns True if the given firmware has RO signed ec."""

    # Check fmap for KEY_RO
    fmap = cros_build_lib.RunCommand(['futility', 'dump_fmap', firmware_image],
                                     capture_output=True)

    return re.search('KEY_RO', fmap.output) is not None

  def Sign(self, keyset, input_name, output_name):
    """"Sign EC image

    Args:
      keyset: keyset used for this signing step
      input_name: ec image path to be signed (i.e. to ec.bin)
      output_name: bios image path to be updated with new hashes
    """
    # Use absolute paths since we use a temp directory
    ec_path = os.path.abspath(input_name)
    bios_path = os.path.abspath(output_name)

    if self.IsROSigned(bios_path):
      # Only sign if not read-only, nothing to do
      return True

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
        return False

    return True


class GBBSigner(signer.FutilitySigner):
  """Sign GBB"""
  _required_keys_public = ('recovery_key',)
  _required_keys_private = ('root_key',)

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
