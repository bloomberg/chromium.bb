# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ChromeOS firmware Signers"""

from __future__ import print_function

import os
import shutil
import tempfile

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.signing.lib.signer import FutilitySigner


class BiosSigner(FutilitySigner):
  """Sign bios.bin file using futility."""

  _required_keys_private = ('firmware_data_key',)
  _required_keys_public = ('kernel',)
  _required_keyblocks = ('firmware_data_key',)

  def __init__(self, bios_version=1, sig_dir=None, sig_id=None):
    """Init BiosSigner

    Args:
      bios_version: Version of the bios being signed
      sig_dir: Signature directory (aka loem dir)
      sig_id: Signature ID (aka loem id)
    """
    self.version = bios_version
    self.sig_dir = sig_dir
    self.sig_id = sig_id

  def GetFutilityArgs(self, keyset, input_name, output_name):
    fw_key = keyset.keys['firmware_data_key']


    kernel_key = keyset.keys['kernel']

    args = ['sign',
            '--type', 'bios',
            '--signprivate', fw_key.private,
            '--keyblock', fw_key.keyblock,
            '--kernelkey', kernel_key.public,
            '--version', str(self.version)]

    # Add developer key arguments
    dev_fw_key = keyset.keys.get('dev_firmware_data_key')

    if dev_fw_key is not None:
      args += ['--devsign', dev_fw_key.private,
               '--devkeyblock', dev_fw_key.keyblock]
    else:
      # Fallback to fw_key if device key not found (legacy, still needed?)
      args += ['--devsign', fw_key.private,
               '--devkeyblock', fw_key.keyblock]

    # Add loem related arguments
    if self.sig_dir is not None and self.sig_id is not None:
      args += ['--loemdir', self.sig_dir,
               '--loemid', self.sig_id]

    # Add final input/output arguments
    args += [input_name, output_name]
    return args


class ECSigner(FutilitySigner):
  """Sign EC bin file."""

  _required_keys_private = ('ec',)

  def GetFutilityArgs(self, keyset, input_name, output_name):
    return ['sign',
            '--type', 'rwsig',
            '--prikey', keyset.keys['ec'].private,
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
