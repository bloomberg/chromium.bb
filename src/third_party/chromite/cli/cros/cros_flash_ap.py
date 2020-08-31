# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""flash-ap command."""

from __future__ import print_function

import os
import sys

from chromite.cli import command
from chromite.lib import build_target_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib.firmware import ap_firmware


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


@command.CommandDecorator('flash-ap')
class FlashApCommand(command.CliCommand):
  """Update the AP Firmware on a device."""

  EPILOG = """Command to flash the AP firmware onto a DUT.

To flash your zork DUT with an IP of 1.1.1.1 via SSH:
  cros flash-ap -b zork -i /path/to/image.bin -d ssh://1.1.1.1

To flash your volteer DUT via SERVO on the default port (9999):
  cros flash-ap -d servo:port -b volteer -i /path/to/image.bin

To flash your volteer DUT via SERVO on port 1234:
  cros flash-ap -d servo:port:1234 -b volteer -i /path/to/image.bin
"""

  @classmethod
  def AddParser(cls, parser):
    super(FlashApCommand, cls).AddParser(parser)
    cls.AddDeviceArgument(
        parser,
        schemes=[
            commandline.DEVICE_SCHEME_SSH, commandline.DEVICE_SCHEME_SERVO
        ])

    parser.add_argument(
        '-i',
        '--image',
        required=True,
        type='path',
        help='/path/to/BIOS_image.bin')
    parser.add_argument(
        '-b',
        '--build-target',
        default=cros_build_lib.GetDefaultBoard(),
        dest='build_target',
        help='The name of the build target.')
    parser.add_argument(
        '-v',
        '--verbose',
        action='store_true',
        help='Increase output verbosity of the flash commands.')
    parser.add_argument(
        '--flashrom',
        action='store_true',
        help='Use flashrom to flash instead of futility.')
    parser.add_argument(
        '--fast',
        action='store_true',
        help='Speed up flashing by not validating flash.')
    parser.add_argument(
        '-n', '--dry-run',
        action='store_true',
        help='Execute a dry-run. Print the commands that would be run instead '
             'of running them.')

  def validate_options(self):
    """Parse the arguments."""
    if not os.path.exists(self.options.image):
      logging.error('%s does not exist, verify the path of your build and try '
                    'again.', self.options.image)

    self.options.Freeze()

  def Run(self):
    """Perform the cros flash-ap command."""
    commandline.RunInsideChroot(self)
    self.validate_options()

    build_target = build_target_lib.BuildTarget(self.options.build_target)
    try:
      ap_firmware.deploy(
          build_target,
          self.options.image,
          self.options.device,
          flashrom=self.options.flashrom,
          fast=self.options.fast,
          verbose=self.options.verbose,
          dryrun=self.options.dry_run)
    except ap_firmware.Error as e:
      cros_build_lib.Die(e)
