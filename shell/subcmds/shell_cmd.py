# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of the 'shell' chromite command."""

# Python imports
import optparse
import os


# Local imports
import chromite.lib.cros_build_lib as cros_lib
from chromite.shell import utils
from chromite.shell import subcmd


def _SplitEnvFromArgs(argv):
  """Split environment settings from arguments.

  This function will just loop over the arguments, looking for ones with an '='
  character in them.  As long as it finds them, it takes them out of the arg
  list adds them to a dictionary.  As soon as it finds the first argument
  without an '=', it stops looping.

  NOTE: Some doctests below test for equality with ==, since dicts with more
        than one item may be arbitrarily ordered.

  >>> result = _SplitEnvFromArgs(['abc=1', 'def=two', 'three'])
  >>> result == ({'abc': '1', 'def': 'two'}, ['three'])
  True

  >>> _SplitEnvFromArgs(['one', 'two', 'three'])
  ({}, ['one', 'two', 'three'])

  >>> _SplitEnvFromArgs(['abc=1', 'three', 'def=two'])
  ({'abc': '1'}, ['three', 'def=two'])

  >>> result = _SplitEnvFromArgs(['abc=1', 'ghi=4 4', 'def=two'])
  >>> result == ({'abc': '1', 'ghi': '4 4', 'def': 'two'}, [])
  True

  >>> _SplitEnvFromArgs(['abc=1', 'abc=2', 'three'])
  ({'abc': '2'}, ['three'])

  >>> _SplitEnvFromArgs([])
  ({}, [])

  Args:
    argv: The arguments to parse; this list is not modified.  Should
        not include "argv[0]"
  Returns:
    env: A dict containing key=value paris.
    argv: A new list containing anything left after.
  """
  # Copy the list so we don't screw with caller...
  argv = list(argv)

  env = {}
  while argv:
    if '=' in argv[0]:
      key, val = argv.pop(0).split('=', 2)
      env[key] = val
    else:
      break

  return env, argv


class ShellCmd(subcmd.ChromiteCmd):
  """Start a shell in the chroot.

  This can either just start a simple interactive shell, it can be used to
  run an arbirtary command inside the chroot and then exit.
  """

  def Run(self, raw_argv, chroot_config=None):
    """Run the command.

    Args:
      raw_argv: Command line arguments, including this command's name, but not
          the chromite command name or chromite options.
      chroot_config: A SafeConfigParser for the chroot config; or None chromite
          was called from within the chroot.
    """
    # Parse options for command...
    # ...note that OptionParser will eat the '--' if it's there, which is what
    # we want..
    usage_str = ('usage: %%prog [chromite_options] %s [options] [VAR=value] '
                 '[-- command [arg1] [arg2] ...]') % raw_argv[0]
    parser = optparse.OptionParser(usage=usage_str)
    (_, argv) = parser.parse_args(raw_argv[1:])

    # Enter the chroot if needed...
    if not cros_lib.IsInsideChroot():
      utils.EnterChroot(chroot_config, (self, 'Run'), raw_argv)
    else:
      # We'll put CWD as src/scripts when running the command.  Since everyone
      # running by hand has their cwd there, it is probably the safest.
      cwd = os.path.join(utils.SRCROOT_PATH, 'src', 'scripts')

      # By default, no special environment...
      env = None

      if not argv:
        # If no arguments, we'll just start bash.
        argv = ['bash']
      else:
        # Parse the command line, looking at the beginning for VAR=value type
        # statements.  I couldn't figure out a way to get bash to do this for
        # me.
        user_env, argv = _SplitEnvFromArgs(argv)
        if not argv:
          cros_lib.Die('No command specified')

        # If there was some environment, use it to override the standard
        # environment.
        if user_env:
          env = dict(os.environ)
          env.update(user_env)

      # Don't show anything special for errors; we'll let the shell report them.
      cros_lib.RunCommand(argv, cwd=cwd, env=env, error_ok=True,
                          ignore_sigint=True)
