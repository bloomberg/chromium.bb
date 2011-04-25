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


def _CommandHelp():
  """Returns a help string listing all available sub-commands"""
  help_list = []
  for cmd_name in _cros_env.subcmds:
    doc = _cros_env.subcmds[cmd_name].__doc__.splitlines()[0]
    if doc.endswith('.'):
      doc = doc[:-1]
    help_list.append('   %-11s%s' % (cmd_name, doc))
  return '\n'.join(help_list)

def _ValidateCommand(cmd_str):
  """Ensures we have a valid command, guesses or reports an error if not.

  >>> _ValidateCommand('build')
  ('build', None)

  >>> _ValidateCommand('bui')
  ('build', None)

  >>> _ValidateCommand('uild')
  ('uild', "Unknown command 'uild'")

  >>> _ValidateCommand('uil')
  ('uil', "Unknown command 'uil'")

  >>> _cros_env.guess_commands = False
  >>> _ValidateCommand('bui')
  ('bui', "Unknown command 'bui'\\n(did you mean: 'chromite build'?)")

  >>> _cros_env.guess_commands = True
  >>> _ValidateCommand('yes_your_highness_your_highness')
  ('yes_your_highness_your_highness', \
"Unknown command 'yes_your_highness_your_highness'")

  >>> _ValidateCommand('')
  ('', "Unknown command ''")

  >>> _ValidateCommand(' ')
  (' ', "Unknown command ' '")

  >>> _ValidateCommand('BUILD')
  ('BUILD', "Unknown command 'BUILD'")

  Args:
    cmd_str: Command to validate.
  returns
    A tuple: cmd_str, error_msg:
      cmd_str: the command we decided to execute, or None.
      error_msg: None if all ok, otherwise a helpful message.
  """
  msg = "Unknown command '%s'" % cmd_str
  if cmd_str not in _cros_env.subcmds:
    possible_cmds = []
    for this_cmd in sorted(_cros_env.subcmds):
      if this_cmd.startswith(cmd_str):
        possible_cmds.append(this_cmd)
    if len(possible_cmds) == 1:
      msg += "\n(did you mean: 'chromite %s'?)" % possible_cmds[0]

      # Guess at the command if we are allowed to.
      if _cros_env.guess_commands:
        cmd_str = possible_cmds[0]
  if cmd_str in _cros_env.subcmds:
    return cmd_str, None

  # Abort with an error.
  return cmd_str, msg

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
      """%s [chromite_options] [cmd [args]]\n"""
      """\n"""
      """The chromite script is a wrapper to make it easy to do various\n"""
      """build tasks.  For a list of commands, run without any arguments.\n"""
      """\nAvailable commands:\n"""
  ) % os.path.basename(sys.argv[0])
  help_str += _CommandHelp()

  parser = optparse.OptionParser()

  # Verbose defaults to full for now, just to keep people acclimatized to
  # vast amounts of comforting output.
  parser.add_option('-v', dest='verbose', default=3, type='int',
      help='Control verbosity: 0=silent, 1=progress, 3=full')
  parser.add_option('-q', action='store_const', dest='verbose', const=1,
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

  # Validate the subcmd, providing help if needed. On failure, report error.
  cmd_str, msg = _ValidateCommand(cmd_str)
  if msg:
    parser.error(msg)

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

def _Test():
  """Run any built-in tests."""
  import doctest
  doctest.testmod()

if __name__ == '__main__':
  # If first argument is --test, run testing code.
  if sys.argv[1:2] == ["--test"]:
    _Test(*sys.argv[2:])
  else:
    main()
