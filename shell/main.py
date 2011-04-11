#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main file for the chromite shell."""

# Python imports
import optparse
import os
import sys


# Local imports
from chromite.lib import text_menu
import chromite.lib.cros_build_lib as cros_lib
from chromite.shell import utils
from chromite.shell import chromite_env
from chromite.lib import operation

# Set up the cros system.
_cros_env = chromite_env.ChromiteEnv()


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
  if cmd_name in _cros_env.subcmds:
    return cmd_name

  # Find ones that match and put them in a menu...
  possible_cmds = []
  possible_choices = []
  for this_cmd in sorted(_cros_env.subcmds):
    if this_cmd.startswith(cmd_name):
      handler = _cros_env.subcmds[this_cmd]
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
  else:
    choice = text_menu.TextMenu(possible_choices, 'Which chromite command',
                                menu_width=0)

  if choice is None:
    cros_lib.Die('OK, cancelling...')
  else:
    return possible_cmds[choice]


def _ParseArguments(parser, argv):
  '''Helper function to separate arguments for a main program and sub-command.

  We split arguments into ones we understand, and ones to pass on to
  sub-commands. For the former, we put them through the given optparse and
  return the result options and sub-command name. For the latter, we just
  return a list of options and arguments not intended for us.

  We want to parse only the options that we understand at the top level of
  Chromite. Our heuristic here is that anything after the first positional
  parameter (which we assume is the command) is ignored at this level, and
  is passed down to the command level to handle.

  TODO(sjg): Revisit this to include tolerant option parser instead
  http://codereview.chromium.org/6469035/

  Args:
    parser: Option parser.
    argv:   List of program arguments

  Returns:
    options:  Top level options (returned from optparse).
    cmd_str:  Subcommand to run
    cmd_args: Arguments intended for subcommands.
  '''
  our_args = []
  cmd_args = list(argv)
  cmd_str = ''
  args = []   # Nothing until we call optparse
  while not cmd_str:
    if our_args:
      (options, args) = parser.parse_args(our_args)
    if len(args) > 1:
      cmd_str = args[1].lower()
    elif cmd_args:
      # We don't have a command yet. Transfer a positional arg from from
      # cmd_args to our_args to see if that does the trick. We move over any
      # options we find also.
      while cmd_args:
        arg = cmd_args.pop(0)
        our_args.append(arg)
        if not arg.startswith( '-'):
          break
    else:
      # No more cmd_args to consume.
      break

  # We must run the parser, even if it dies due to lack of arguments
  if not args:
    (options, args) = parser.parse_args(our_args)
  return options, cmd_str, cmd_args

def main():
  """Main function for the chromite shell."""

  # Hack it so that argv[0] is 'chromite' so that it doesn't tell user to run
  # 'main.py' in help commands...
  # TODO(dianders): Remove this hack, since it is ugly.  Shouldn't be needed
  # once we change the way that the chromite wrapper works.
  sys.argv[0] = 'chromite'

  # Support EnterChroot().
  # This may raise a ChromiteError if the child dies, so we must handle this.
  try:
    did_resume = utils.ResumeEnterChrootIfNeeded(sys.argv)
    if did_resume:
      return
  except chromite_env.ChromiteError:
    # The error has been reported, but we must exit indicating failure
    sys.exit(1)

  # TODO(dianders): Make help a little better.  Specifically:
  # 1. Add a command called 'help'
  # 2. Make the help string below include command list and descriptions (like
  #    the menu, but without being interactive).
  # 3. Make "help command" and "--help command" equivalent to "command --help".
  help_str = (
      """%(prog)s [chromite_options] [cmd [args]]\n"""
      """\n"""
      """The chromite script is a wrapper to make it easy to do various\n"""
      """build tasks.  For a list of commands, run without any arguments."""
  ) % {'prog': os.path.basename(sys.argv[0])}

  parser = optparse.OptionParser()

  # Verbose defaults to full for now, just to keep people acclimatized to
  # vast amounts of comforting output.
  parser.add_option('-v', dest='verbose', default=3, type='int',
      help='Control verbosity: 0=silent, 1=progress, 3=full')
  parser.add_option('-q', action='store_const', dest='verbose', const=0,
      help='Be quieter (sets verbosity to 1)')
  if not cros_lib.IsInsideChroot():
    parser.add_option('--chroot', action='store', type='string',
        dest='chroot_name', default='chroot',
        help="Chroot spec to use. Can be an absolute path to a spec file "
            "or a substring of a chroot spec name (without .spec suffix)")
  parser.usage = help_str

  # Parse the arguments and separate them into top-level and subcmd arguments.
  options, cmd_str, cmd_args = _ParseArguments(parser, sys.argv)

  # Configure the operation setup.
  oper = _cros_env.GetOperation()
  oper.verbose = options.verbose >= 3
  oper.progress = options.verbose >= 1

  # Look for special "--chroot" argument to allow for alternate chroots
  if not cros_lib.IsInsideChroot():
    chroot_spec_path = utils.FindSpec(options.chroot_name,
                                      spec_type=utils.CHROOT_SPEC_TYPE)

    oper.Info('Using chroot "%s"' % os.path.relpath(chroot_spec_path))

    chroot_config = utils.ReadConfig(chroot_spec_path)
  else:
    # Already in the chroot; no need to get config...
    chroot_config = None

  # Validate the subcmd, popping a menu if needed.
  cmd_str = _FindCommand(cmd_str)
  oper.Info("Running command '%s'." % cmd_str)

  # Finally, call the function w/ standard argv.
  cmd_cls = _cros_env.subcmds[cmd_str]
  cmd_obj = cmd_cls()
  cmd_obj.SetChromiteEnv(_cros_env)
  try:
    cmd_obj.Run([cmd_str] + cmd_args, chroot_config=chroot_config)

  # Handle an error in one of the scripts: print a message and exit.
  except chromite_env.ChromiteError, msg:
    sys.exit(1)

if __name__ == '__main__':
  main()
