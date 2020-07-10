# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A CLI to do the replication described by a ReplicationConfig proto."""

from __future__ import print_function

from google.protobuf import json_format

from chromite.api.gen.config import replication_config_pb2
from chromite.lib import commandline
from chromite.lib import osutils
from chromite.lib import replication_lib


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)
  subparsers = parser.add_subparsers()

  run_subparser = subparsers.add_parser('run', help='Run a ReplicationConfig')
  run_subparser.add_argument(
      'replication_config', help='Path to the ReplicationConfig JSONPB')
  run_subparser.set_defaults(func=Run)

  return parser


def Run(options):
  """Runs the replication described by a PrelicationConfig proto."""
  replication_config = json_format.Parse(
      osutils.ReadFile(options.replication_config),
      replication_config_pb2.ReplicationConfig(),
  )

  replication_lib.Replicate(replication_config)


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)
  options.Freeze()

  options.func(options)
