# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ChromeOS firmware Signers"""

from __future__ import print_function

from chromite.signing.lib.signer import FutilitySigner


class BiosSigner(FutilitySigner):
  """Sign bios.bin file using futility."""

  _required_keys_private = ('firmware',)
  _required_keys_public = ('kernel',)
  _required_keyblocks = ('firmware',)

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
    fw_key = keyset.GetKey('firmware')
    fw_keyblock = keyset.GetKeyblock('firmware')


    kernel_key = keyset.keys['kernel']

    args = ['sign',
            '--type', 'bios',
            '--signprivate', fw_key.private,
            '--keyblock', fw_keyblock.filename,
            '--kernelkey', kernel_key.public,
            '--version', str(self.version)]

    # Add developer key arguments
    dev_fw_key = keyset.GetKey('dev_firmware')
    dev_fw_keyblock = keyset.GetKeyblock('dev_firmware')

    if dev_fw_key is not None and dev_fw_keyblock is not None:
      args += ['--devsign', dev_fw_key.private,
               '--devkeyblock', dev_fw_keyblock.filename]
    else:
      # Fallback to fw_key if device key not found (legacy, still needed?)
      args += ['--devsign', fw_key.private,
               '--devkeyblock', fw_keyblock.filename]

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
