# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome OS Image file signing."""

from __future__ import print_function

import collections

from chromite.lib import commandline
from chromite.lib import cros_logging as logging
from chromite.signing.image_signing import imagefile


class Error(Exception):
  """Base exception for all exceptions in this module."""


# For a given image_type argument, what are the arguments that we
# need to pass to imagefile.SignImage()?  Several of the image types
# take very similar parameters.  See the dictionary immediately following.
SignArgs = collections.namedtuple(
    'SignArgs', ('image_type', 'kernel_id', 'keyA_prefix'))

# Dictionary to convert the user's given image_type to the correct arguments for
# image_type, kernel_id, and keyA_prefix to imagefile.Signimage.
SignImageArgs = {
    'ssd': SignArgs('SSD', 2, ''),
    'base': SignArgs('SSD', 2, ''),
    'usb': SignArgs('USB', 2, ''),
    'recovery': SignArgs('recovery', 4, 'recovery_'),
    'factory': SignArgs('factory_install', 2, 'installer_'),
    'install': SignArgs('factory_install', 2, 'installer_'),
}

# All of the valid |image_type| values from the user.  Many of these are not
# implemented yet.  Since they will be, they get their own exit code.
ValidImageTypes = (
    'ssd', 'base', 'usb', 'recovery', 'factory', 'install', 'firmware',
    'nv_lp0_firmware', 'kernel', 'recovery_kernel', 'update_payload',
    'accessory_usbpd', 'accessory_rwsig',
)


def main(argv):
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('-t', '--type', required=True, dest='image_type',
                      choices=list(ValidImageTypes), help='Type of image')
  parser.add_argument('-i', '--input', required=True, dest='input_image',
                      type='path', help='Path to input image file')
  parser.add_argument('-k', '--keyset-dir', required=True, dest='key_dir',
                      help='Path to keyset directory')
  parser.add_argument('-o', '--output', required=True, dest='output_image',
                      help='Path to output image file')

  options = parser.parse_args(argv)
  options.Freeze()

  # See what kind of image this is.
  call_args = SignImageArgs.get(options.image_type, None)
  if call_args:
    imagefile.SignImage(
        call_args.image_type, options.input_image, options.output_image,
        call_args.kernel_id, options.key_dir, call_args.keyA_prefix)
    return 0

  # TODO(lamontjones): implement signing for the other supported image types:
  # firmware, nv_lp0_firmware, kernel, recovery_kernel, update_payload,
  # accessory_usbpd, accessory_rwsig.
  logging.error('Unimplemented --type %s', options.image_type)
  return 2
