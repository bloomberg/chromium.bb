# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module is responsible for generating a stateful update payload."""

from __future__ import print_function

from chromite.lib import commandline

from chromite.lib.paygen import paygen_stateful_payload_lib


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

  paygen_stateful_payload_lib.GenerateStatefulPayload(opts.image_path,
                                                      opts.output_dir)
