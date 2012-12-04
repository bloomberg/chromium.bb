# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from chromite.cros import commands
from chromite.lib import commandline


def GetOptions(my_commands):
  """Returns the argparse to use for Cros."""
  parser = commandline.ArgumentParser()
  if not commands:
    return parser

  subparsers = parser.add_subparsers(title='cros commands')
  for cmd_name, class_def in sorted(my_commands.iteritems(), key=lambda x:x[0]):
    epilog = getattr(class_def, 'EPILOG', None)
    sub_parser = subparsers.add_parser(
        cmd_name, help=class_def.__doc__, epilog=epilog,
        formatter_class=commandline.argparse.RawDescriptionHelpFormatter)
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
