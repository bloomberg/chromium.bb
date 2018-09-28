# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate DLC metadata and move artifacts to output directory"""

from __future__ import print_function

import json
import os

from chromite.lib import commandline
from chromite.lib import osutils


def ReduceDlcMetadata(meta_dir):
  """Combines metadata from DLC classes into a single manifest file.

  When DLCs are built we generate a JSON file used by imageloader to enable
  verification and loading of DLC image files. This function pulls information
  out of these files in order to create a manifest file describing all of the
  DLC available for this build.

  Args:
    meta_dir: A path to the DLC build artifact directory.

  Returns:
    metadata: A dictionary that can be used to generate a serialized manifest.
  """
  # TODO(chowes@google.com): Metadata format is still TBD, just a placeholder.
  metadata = {}
  for f in osutils.DirectoryIterator(meta_dir):
    if os.path.basename(f) == 'imageloader.json':
      with open(f, 'r') as metafile:
        dlc_info = json.load(metafile)
        metadata[dlc_info['id']] = dlc_info['version']
  return metadata


def WriteMetadata(metadata, output_dir):
  """Writes serialized metadata into the DLC output directory.

  Given a dictionary containing DLC metadata, this function writes this
  information to the DLC output directory.

  Args:
    metadata: A dictionary containing DLC metadata.
    output_dir: path to the DLC output directory.
  """
  # TODO(chowes@google.com): Metadata format is still TBD, just a placeholder.
  with open(os.path.join(output_dir, 'manifest.json'), 'w') as manifest:
    json.dump(metadata, manifest)


def CopyDlc(build_dir, output_dir):
  """Copies DLC image files into the DLC output directory.

  Copies the DLC image files in the given build directory into the given DLC
  output directory. If the DLC build directory does not exist, this function
  does nothing.

  Args:
    build_dir: Path to directory containing DLC images.
    output_dir: Path to DLC output directory.
  """
  if os.path.exists(build_dir):
    osutils.SafeMakedirs(output_dir)
    osutils.CopyDirContents(build_dir, output_dir)


def GetParser():
  """Generate an ArgumentParser instance.

  Returns:
    ArgumentParser instance.
  """
  parser = commandline.ArgumentParser(description=__doc__)

  required = parser.add_argument_group('Required Arguments')
  required.add_argument('--build-dir', type='path', metavar='DIR',
                        required=True,
                        help='Directory containing DLC image files.')
  required.add_argument('--output-dir', type='path', metavar='DIR',
                        required=True,
                        help='Path to DLC output directory.')

  return parser


def main(argv):
  _DLC_BUILD_SUBDIR = 'build/rootfs/dlc'
  _DLC_META_SUBDIR = 'opt/google/dlc/'
  _DLC_OUTPUT_SUBDIR = 'dlc'

  opts = GetParser().parse_args(argv)
  opts.Freeze()

  CopyDlc(os.path.join(opts.build_dir, _DLC_BUILD_SUBDIR),
          os.path.join(opts.output_dir, _DLC_OUTPUT_SUBDIR))

  metadata = ReduceDlcMetadata(os.path.join(opts.build_dir, _DLC_META_SUBDIR))
  WriteMetadata(metadata, os.path.join(opts.output_dir, _DLC_OUTPUT_SUBDIR))
