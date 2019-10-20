# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Trigger signing for cr50 images.

Causes signing to occur for a given artifact.
"""

from __future__ import print_function

import argparse
import json
import re

from chromite.api.gen.chromiumos import common_pb2
from chromite.api.gen.chromiumos import sign_image_pb2
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging

CR50_PRODUCTION_JOB = 'chromeos/packaging/sign-image'
CR50_STAGING_JOB = 'chromeos/staging/staging-sign-image'

# See ../infra/proto/src/chromiumos/common.proto.
_channels = {k.lower().replace('channel_', ''): v
             for k, v in common_pb2.Channel.items()}

# See ../infra/proto/src/chromiumos/common.proto.
_image_types = {k.lower(): v for k, v in common_pb2.ImageType.items() if v}

# See ../infra/proto/src/chromiumos/sign_image.proto.
_signer_types = {k.lower().replace('signer_', ''): v
                 for k, v in sign_image_pb2.SignerType.items() if v}

# See ../infra/proto/src/chromiumos/sign_image.proto.
_target_types = {k.lower() if v else 'unchanged': v
                 for k, v in sign_image_pb2.Cr50Instructions.Target.items()}


def GetParser():
  """Creates the argparse parser."""
  class uint32(int):
    """Unsigned 32-bit int."""
    def __new__(cls, val):
      """Return a positive 32-bit value."""
      return int(val, 0) & 0xffffffff

  class Dev01Action(argparse.Action):
    """Convert --dev01 MMM NNN into an %08x-%08x device_id."""
    def __call__(self, parser, namespace, values, option_string=None):
      if not namespace.dev_ids:
        namespace.dev_ids = []
      namespace.dev_ids.append('%08x-%08x' % (values[0], values[1]))

  parser = commandline.ArgumentParser(
      description=__doc__,
      default_log_level='debug')

  parser.add_argument(
      '--staging', action='store_true', dest='staging',
      help='Use the staging instance to create the request.')

  parser.add_argument(
      '--build-target', default='unknown', help='The build target.')

  parser.add_argument(
      '--channel', choices=_channels, default='unspecified',
      help='The (optional) channel for the artifact.')

  parser.add_argument(
      '--archive', required=True,
      help='The gs://path of the archive to sign.')

  parser.add_argument(
      '--dry-run', action='store_true', default=False,
      help='This is a dryrun, nothing will be triggered.')

  parser.add_argument(
      '--keyset', required=True, help='The keyset to use.')

  parser.add_argument(
      '--image-type', choices=_image_types, default='cr50_firmware',
      help='The image type.')

  parser.add_argument(
      '--signer-type', choices=_signer_types,
      default='dev', help='The image type.')

  parser.add_argument(
      '--target', choices=_target_types,
      default='unchanged', help='The image type.')

  node_locked = parser.add_argument_group(
      'Node_locked',
      description='Additional arguments for --target=node_locked')

  node_locked.add_argument(
      '--device-id', action='append', dest='dev_ids',
      help='The device_id ("%%08x-%%08x" format).')

  node_locked.add_argument(
      '--dev01', nargs=2, type=uint32, action=Dev01Action, metavar='DEVx',
      dest='dev_ids', help='DEV0 and DEV1 for the device')

  return parser


def LaunchOne(dry_run, builder, properties):
  """Launch one build.

  Args:
    dry_run: If true, just echo what would be done.
    builder: builder to use.
    properties: json properties to use.
  """
  json_prop = json.dumps(properties)
  cmd = ['bb', 'add', '-p', '@/dev/stdin', builder]
  if dry_run:
    logging.info('Would run: %s with input: %s', ' '.join(cmd), json_prop)
  else:
    cros_build_lib.run(cmd, input=json_prop, log_output=True)


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)
  options.Freeze()

  passes = True
  if options.image_type != 'cr50_firmware':
    logging.error('Unsupported --image-type %s', options.image_type)
    passes = False

  if options.target == 'node_locked':
    if not options.dev_ids:
      logging.error('--target node_locked must specify device_id')
      passes = False
    else:
      for dev in options.dev_ids:
        if not re.match('[0-9a-fA-F]{8}-[0-9a-fA-F]{8}$', dev):
          logging.error('Illegal device_id %s', dev)
          passes = False
  elif options.dev_ids:
    logging.error('Device IDs are only valid with `--target node_locked`')
    passes = False

  if not passes:
    return 1

  builder = CR50_STAGING_JOB if options.staging else CR50_PRODUCTION_JOB

  properties = {
      'archive': options.archive,
      'build_target': {'name': options.build_target},
      'channel': _channels[options.channel],
      'cr50_instructions': {
          'target': _target_types[options.target],
      },
      'image_type': _image_types[options.image_type],
      'keyset': options.keyset,
      'signer_type': _signer_types[options.signer_type],
  }

  if options.target != 'node_locked':
    LaunchOne(options.dry_run, builder, properties)
  else:
    for dev in options.dev_ids:
      properties['cr50_instructions']['device_id'] = dev
      LaunchOne(options.dry_run, builder, properties)
  return 0
