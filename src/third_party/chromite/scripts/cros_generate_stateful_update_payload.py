# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module is responsible for generating a stateful update payload."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils


_STATEFUL_FILE = 'stateful.tgz'


# TODO(tbrindus): move to paygen.
def GenerateStatefulPayload(image_path, output_directory):
  """Generates a stateful update payload given a full path to an image.

  Args:
    image_path: Full path to the image.
    output_directory: Path to the directory to leave the resulting output.
    logging: logging instance.
  """
  logging.info('Generating stateful update file.')

  output_gz = os.path.join(output_directory, _STATEFUL_FILE)

  parts = cros_build_lib.GetImageDiskPartitionInfo(image_path)
  stateful_num = parts[constants.PART_STATE].number

  # Mount the image to pull out the important directories.
  with osutils.TempDir() as stateful_mnt, \
      osutils.MountImageContext(image_path, stateful_mnt, (stateful_num,)) as _:
    stateful_dir = os.path.join(stateful_mnt, 'dir-%s' % constants.PART_STATE)

    try:
      logging.info('Tarring up /usr/local and /var!')
      cros_build_lib.SudoRunCommand([
          'tar',
          '-czf',
          output_gz,
          '--directory=%s' % stateful_dir,
          '--hard-dereference',
          '--transform=s,^dev_image,dev_image_new,',
          '--transform=s,^var_overlay,var_new,',
          'dev_image',
          'var_overlay',
      ])
    except:
      logging.error('Failed to create stateful update file')
      raise

  logging.info('Successfully generated %s', output_gz)


def ParseArguments(argv):
  """Returns a namespace for the CLI arguments."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('-i', '--image_path', type='path', required=True,
                      help='The image to generate the stateful update for.')
  parser.add_argument('-o', '--output_dir', type='path', required=True,
                      help='The path to the directory to output the stateful'
                           'update file.')
  opts = parser.parse_args(argv)
  opts.Freeze()

  return opts


def main(argv):
  opts = ParseArguments(argv)

  GenerateStatefulPayload(opts.image_path, opts.output_dir)
