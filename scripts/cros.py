# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

# Older version of Python might be missing argparse.
try:
  import argparse
except ImportError:
  print >> sys.stderr, "argparse missing. sudo apt-get install python-argparse"
  sys.exit(1)

from chromite.cros import commands


def GetOptions(my_commands):
  """Returns the argparse to use for Cros."""
  parser = argparse.ArgumentParser()
  if not commands:
    return parser

  subparsers = parser.add_subparsers(title='cros commands')
  for cmd_name, class_def in my_commands.iteritems():
    sub_parser = subparsers.add_parser(cmd_name, help=class_def.__doc__)
    class_def.AddParser(sub_parser)

  return parser


def main(args):
  parser = GetOptions(commands.ListCommands())
  # Cros currently does nothing without a subcmd. Print help if no args are
  # specified.
  if not args:
    parser.print_help()
    return 1

  namespace = parser.parse_args(args)
  subcommand = namespace.cros_class(namespace)
  subcommand.Run()
  return 0
