# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This implements the entry point for CLI toolsets `cros` and `brillo`.

This script is invoked by chromite/bin/{cros,brillo}, which sets up the
proper execution environment and calls this module's main() function.

In turn, this script looks for a subcommand based on how it was invoked. For
example, `cros lint` will use the cli/cros/cros_lint.py subcommand.

See cli/ for actual command implementations.
"""

from __future__ import print_function

import sys

from chromite.cli import command
from chromite.lib import commandline
from chromite.lib import cros_logging as logging
from chromite.lib import stats


def GetOptions(my_commands):
  """Returns the argparse to use for commandline parsing."""
  parser = commandline.ArgumentParser(caching=True, default_log_level='notice')
  if command.GetToolset() == 'brillo':
    # For brillo commands, we want to store the logs to a file for error
    # handling at logging level notice.
    command.SetupFileLogger()

  if not command:
    return parser

  subparsers = parser.add_subparsers(title='Subcommands')
  for cmd_name, class_def in sorted(my_commands.iteritems(),
                                    key=lambda x: x[0]):
    epilog = getattr(class_def, 'EPILOG', None)
    sub_parser = subparsers.add_parser(
        cmd_name, description=class_def.__doc__, epilog=epilog,
        caching=class_def.use_caching_options,
        formatter_class=commandline.argparse.RawDescriptionHelpFormatter)
    class_def.AddParser(sub_parser)

  return parser


def _RunSubCommand(subcommand):
  """Helper function for testing purposes."""
  return subcommand.Run()


def main(argv):
  try:
    parser = GetOptions(command.ListCommands())
    # Cros currently does nothing without a subcmd. Print help if no args are
    # specified.
    if not argv:
      parser.print_help()
      return 1

    namespace = parser.parse_args(argv)
    subcommand = namespace.command_class(namespace)
    with stats.UploadContext() as queue:
      if subcommand.upload_stats:
        cmd_base = subcommand.options.command_class.command_name
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
  except KeyboardInterrupt:
    logging.debug('Aborted due to keyboard interrupt.')
    return 1
