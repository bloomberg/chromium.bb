# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from chromite.cros import commands
from chromite.lib import commandline
from chromite.lib import stats


def GetOptions(my_commands):
  """Returns the argparse to use for Cros."""
  parser = commandline.ArgumentParser(caching=True)
  if not commands:
    return parser

  subparsers = parser.add_subparsers(title='cros commands')
  for cmd_name, class_def in sorted(my_commands.iteritems(), key=lambda x:x[0]):
    epilog = getattr(class_def, 'EPILOG', None)
    sub_parser = subparsers.add_parser(
        cmd_name, description=class_def.__doc__, epilog=epilog,
        formatter_class=commandline.argparse.RawDescriptionHelpFormatter)
    class_def.AddParser(sub_parser)

  return parser


def _RunSubCommand(subcommand):
  """Helper function for testing purposes."""
  return subcommand.Run()


def main(argv):
  parser = GetOptions(commands.ListCommands())
  # Cros currently does nothing without a subcmd. Print help if no args are
  # specified.
  if not argv:
    parser.print_help()
    return 1

  namespace = parser.parse_args(argv)
  subcommand = namespace.cros_class(namespace)
  with stats.UploadContext() as queue:
    if subcommand.upload_stats:
      cmd_base = subcommand.options.cros_class.command_name
      cmd_stats = stats.Stats.SafeInit(cmd_line=sys.argv, cmd_base=cmd_base)
      if cmd_stats:
        queue.put([cmd_stats, stats.StatsUploader.URL,
                   subcommand.upload_stats_timeout])
    # TODO: to make command completion faster, send an interrupt signal to the
    # stats uploader task after the subcommand completes.
    code = _RunSubCommand(subcommand)
    if code is not None:
      return code

  return 0
