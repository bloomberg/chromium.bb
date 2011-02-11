#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main file for the chromite shell."""

# Python imports
import os
import sys


# Local imports
from chromite.lib import text_menu
import chromite.lib.cros_build_lib as cros_lib
from chromite.shell import utils
from chromite.shell.subcmds import build_cmd
from chromite.shell.subcmds import clean_cmd
from chromite.shell.subcmds import portage_cmds
from chromite.shell.subcmds import shell_cmd
from chromite.shell.subcmds import workon_cmd
from chromite.shell import chromite_env
from chromite.lib import operation


# Define command handlers and command strings.
#
# ORDER MATTERS here when we show the menu.
_COMMAND_HANDLERS = [
  build_cmd.BuildCmd,
  clean_cmd.CleanCmd,
  portage_cmds.EbuildCmd,
  portage_cmds.EmergeCmd,
  portage_cmds.EqueryCmd,
  portage_cmds.PortageqCmd,
  shell_cmd.ShellCmd,
  workon_cmd.WorkonCmd,
]
_COMMAND_STRS = [cls.__name__[:-len('Cmd')].lower()
                 for cls in _COMMAND_HANDLERS]


def _FindCommand(cmd_name):
  """Find the command that matches the given command name.

  This tries to be smart.  See the cmd_name parameter for details.

  Args:
    cmd_name: Can be any of the following:
        1. The full name of a command.  This is checked first so that if one
           command name is a substring of another, you can still specify
           the shorter spec name and know you won't get a menu (the exact
           match prevents the menu).
        2. A _prefix_ that will be used to pare-down a menu of commands
           Can be the empty string to show a menu of all commands

  Returns:
    The command name.
  """
  # Always make cmd_name lower.  Commands are case-insensitive.
  cmd_name = cmd_name.lower()

  # If we're an exact match, we're done!
  if cmd_name in _COMMAND_STRS:
    return cmd_name

  # Find ones that match and put them in a menu...
  possible_cmds = []
  possible_choices = []
  for cmd_num, this_cmd in enumerate(_COMMAND_STRS):
    if this_cmd.startswith(cmd_name):
      handler = _COMMAND_HANDLERS[cmd_num]
      assert hasattr(handler, '__doc__'), \
             ('All handlers must have docstrings: %s' % cmd_name)
      desc = handler.__doc__.splitlines()[0]

      possible_cmds.append(this_cmd)
      possible_choices.append('%s - %s' % (this_cmd, desc))

  if not possible_choices:
    cros_lib.Die('No commands matched: "%s".  '
        'Try running with no arguments for a menu.' %
        cmd_name)

  if cmd_name and len(possible_choices) == 1:
    # Avoid showing the user a menu if the user's search string matched exactly
    # one item.
    choice = 0
    cros_lib.Info("Running command '%s'." % possible_cmds[choice])
  else:
    choice = text_menu.TextMenu(possible_choices, 'Which chromite command',
                                menu_width=0)

  if choice is None:
    cros_lib.Die('OK, cancelling...')
  else:
    return possible_cmds[choice]


def main():
  """Main function for the chromite shell."""

  # Hack it so that argv[0] is 'chromite' so that it doesn't tell user to run
  # 'main.py' in help commands...
  # TODO(dianders): Remove this hack, since it is ugly.  Shouldn't be needed
  # once we change the way that the chromite wrapper works.
  sys.argv[0] = 'chromite'

  # Support EnterChroot().
  did_resume = utils.ResumeEnterChrootIfNeeded(sys.argv)
  if did_resume:
    return

  # TODO(dianders): Make help a little better.  Specifically:
  # 1. Add a command called 'help'
  # 2. Make the help string below include command list and descriptions (like
  #    the menu, but without being interactive).
  # 3. Make "help command" and "--help command" equivalent to "command --help".
  help_str = (
      """Usage: %(prog)s [chromite_options] [cmd [args]]\n"""
      """\n"""
      """The chromite script is a wrapper to make it easy to do various\n"""
      """build tasks.  For a list of commands, run without any arguments.\n"""
      """\n"""
      """Options:\n"""
      """  -h, --help            show this help message and exit\n"""
  ) % {'prog': os.path.basename(sys.argv[0])}
  if not cros_lib.IsInsideChroot():
    help_str += (
        """  --chroot=CHROOT_NAME  Chroot spec to use. Can be an absolute\n"""
        """                        path to a spec file or a substring of a\n"""
        """                        chroot spec name (without .spec suffix)\n"""
    )

  # We don't use OptionParser here, since options for different subcommands are
  # so different.  We just look for the chromite options here...
  if sys.argv[1:2] == ['--help']:
    print help_str
    sys.exit(0)
  else:
    # Start by skipping argv[0]
    argv = sys.argv[1:]

    # Look for special "--chroot" argument to allow for alternate chroots
    if not cros_lib.IsInsideChroot():
      # Default chroot name...
      chroot_name = 'chroot'

      # Get chroot spec name if specified; trim argv down if needed...
      if argv:
        if argv[0].startswith('--chroot='):
          _, chroot_name = argv[0].split('=', 2)
          argv = argv[1:]
        elif argv[0] == '--chroot':
          if len(argv) < 2:
            cros_lib.Die('Chroot not specified.')

          chroot_name = argv[1]
          argv = argv[2:]

      chroot_spec_path = utils.FindSpec(chroot_name,
                                        spec_type=utils.CHROOT_SPEC_TYPE)

      cros_lib.Info('Using chroot "%s"' % os.path.relpath(chroot_spec_path))

      chroot_config = utils.ReadConfig(chroot_spec_path)
    else:
      # Already in the chroot; no need to get config...
      chroot_config = None

    # Get command and arguments
    if argv:
      cmd_str = argv[0].lower()
      argv = argv[1:]
    else:
      cmd_str = ''

    # Validate the subcmd, popping a menu if needed.
    cmd_str = _FindCommand(cmd_str)

    # Set up the cros system.
    cros_env = chromite_env.ChromiteEnv()

    # Configure the operation setup.
    oper = cros_env.GetOperation()
    oper.SetVerbose(True)
    oper.SetProgress(True)

    # Finally, call the function w/ standard argv.
    cmd_cls = _COMMAND_HANDLERS[_COMMAND_STRS.index(cmd_str)]
    cmd_obj = cmd_cls()
    cmd_obj.SetChromiteEnv(cros_env)
    cmd_obj.Run([cmd_str] + argv, chroot_config=chroot_config)


if __name__ == '__main__':
  main()
