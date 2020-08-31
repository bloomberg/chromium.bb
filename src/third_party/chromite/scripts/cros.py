# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This implements the entry point for the `cros` CLI toolset.

This script is invoked by chromite/bin/cros, which sets up the
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


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


def GetOptions(cmd_name=None):
  """Returns the parser to use for commandline parsing.

  Args:
    cmd_name: The subcommand to import & add.

  Returns:
    A commandline.ArgumentParser object.
  """
  parser = commandline.ArgumentParser(caching=True, default_log_level='notice')

  subparsers = parser.add_subparsers(title='Subcommands', dest='subcommand')
  subparsers.required = True

  # We add all the commands so `cros --help ...` looks reasonable.
  # We add them in order also so the --help output is stable for users.
  for subcommand in sorted(command.ListCommands()):
    if subcommand == cmd_name:
      class_def = command.ImportCommand(cmd_name)
      epilog = getattr(class_def, 'EPILOG', None)
      sub_parser = subparsers.add_parser(
          cmd_name, description=class_def.__doc__, epilog=epilog,
          caching=class_def.use_caching_options,
          formatter_class=commandline.argparse.RawDescriptionHelpFormatter)
      class_def.AddParser(sub_parser)
    else:
      subparsers.add_parser(subcommand, add_help=False)

  return parser


def _RunSubCommand(subcommand):
  """Helper function for testing purposes."""
  return subcommand.Run()


def main(argv):
  try:
    # The first time we parse the commandline is only to figure out what
    # subcommand the user wants to run.  This allows us to avoid importing
    # all subcommands which can be quite slow.  This works because there is
    # no way in Python to list all subcommands and their help output in a
    # single run.
    parser = GetOptions()
    if not argv:
      parser.print_help()
      return 1

    namespace, _ = parser.parse_known_args(argv)
    # The user has selected a subcommand now, so get the full parser after we
    # import the single subcommand.
    parser = GetOptions(namespace.subcommand)
    namespace = parser.parse_args(argv)
    subcommand = namespace.command_class(namespace)
    try:
      code = _RunSubCommand(subcommand)
    except (commandline.ChrootRequiredError, commandline.ExecRequiredError):
      # The higher levels want these passed back, so oblige.
      raise
    except Exception as e:
      code = 1
      logging.error('cros %s failed before completing.', namespace.subcommand)
      if namespace.debug:
        raise
      else:
        logging.error(e)
        logging.error('(Re-run with --debug for more details.)')

    if code is not None:
      return code

    return 0
  except KeyboardInterrupt:
    logging.debug('Aborted due to keyboard interrupt.')
    return 1
