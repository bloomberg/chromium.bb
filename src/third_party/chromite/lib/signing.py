# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""All things Chrome OS signing related"""

from __future__ import print_function

import os

from six.moves import configparser

from chromite.lib import constants


SIGNING_DIR = os.path.join(constants.CHROMITE_DIR, 'signing')
INPUT_INSN_DIR_REL = os.path.join('crostools', 'signer_instructions')
TEST_INPUT_INSN_DIR = os.path.join(SIGNING_DIR, 'signer_instructions')

CROS_SIGNING_BASE_DIR = os.path.join(constants.SOURCE_ROOT, 'src', 'platform',
                                     'signing', 'signer-production')
CROS_SIGNING_CONFIG = os.path.join(CROS_SIGNING_BASE_DIR,
                                   'signer/configs/cros_common.config')
SECURITY_BASELINES_DIR = os.path.join(CROS_SIGNING_BASE_DIR,
                                      'security_test_baselines')
CROS_SIGNING_BIN_DIR = os.path.join(CROS_SIGNING_BASE_DIR,
                                    'signer/signingtools-bin')


def GetDefaultVbootStableHash(config_file=None):
  """Get the default signer vboot_stable_hash config value."""
  config = configparser.ConfigParser()
  config.read(config_file or CROS_SIGNING_CONFIG)
  try:
    return config.get('signer', 'vboot_stable_hash')
  except configparser.Error:
    return None
