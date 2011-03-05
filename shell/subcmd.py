# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""ChromiteCmd abstract class and related functions."""

# Python imports
import os
import sys

# Local imports
import chromite.lib.cros_build_lib as cros_lib
from chromite.shell import utils


class ChromiteCmd(object):
  """The abstract base class of commands listed at the top level of chromite."""

  def __init__(self):
    """ChromiteCmd constructor.

    Args:
      cr: ChromiteEnv object to use for this command
    """
    super(ChromiteCmd, self).__init__()
    self.cros_env = None

  def SetChromiteEnv(self, cros_env):
    """Sets the Chromite environment for this command

    This is split out from __init() since subclasses of ChromiteCmd do not
    have similar constructors and there are a lot of them, and the number is
    likely to grow.

    Please call this method after the constructor.
    """
    self.cros_env = cros_env
    self._oper = cros_env.GetOperation()

  def Run(self, raw_argv, chroot_config=None):
    """Run the command.

    All subclasses must implement this.

    Args:
      raw_argv: Command line arguments, including this command's name, but not
          the chromite command name or chromite options.
      chroot_config: A SafeConfigParser for the chroot config; or None chromite
          was called from within the chroot.
    """
    # Must be implemented by subclass...
    raise NotImplementedError()


class WrappedChrootCmd(ChromiteCmd):
  """Superclass for any command that is simply wrapped by chromite.

  These are commands where:
  - We parse the command line only enough to figure out what board they
    want.  All othe command line parsing is handled by the wrapped command.
    Because of this, the board name _needs_ to be specified first.
  - Everything else (arg parsing, help, etc) is handled by the wrapped command.
    The usage string will be a little messed up, but hopefully that's OK.
  """

  def __init__(self, name, target_cmd, host_cmd, need_args=False,
      env_whitelist=None):
    """WrappedChrootCmd constructor.

    Args:
      name: This is a name for the command, displayed to the user.
      target_cmd: We'll put this at the start of argv when calling a target
          command.  We'll substiture %s with the target.
          Like - ['my_command-%s'] or ['my_command', '--board=%s']
      host_cmd: We'll put this at the start of argv when calling a host command.
          Like - ['my_command'] or ['sudo', 'my_command']
      need_args: If True, we'll prompt for arguments if they weren't specified.
          This makes the most sense when someone runs chromite with no arguments
          and then walks through the menus.  It's not ideal, but less sucky than
          just quitting.
      env_whitelist: This is a whitelist of environment variables that will be
          read from the current environment and passed through to the command.
          Note that this doesn't matter much when we start inside the chroot,
          but matters a lot when we transition into the chroot (since the
          environment is reset when that happens).
          Useful for portage commands.  Like: ['USE', 'FEATURES']
    """
    # Call superclass constructor.
    super(WrappedChrootCmd, self).__init__()

    # Save away params for use later in Run().
    self._name = name
    self._target_cmd = target_cmd
    self._host_cmd = host_cmd
    self._need_args = need_args

    # Handle the env_whitelist.  We need to do this in __init__ rather than in
    # Run(), since we want the environment vars from outside the chroot.
    if env_whitelist is None:
      self._env_to_add = {}
    else:
      self._env_to_add = dict((key, os.environ[key]) for key in env_whitelist
                              if key in os.environ)

  def Run(self, raw_argv, chroot_config=None, argv=None, build_config=None):
    """Run the command.

    Args:
      raw_argv: Command line arguments, including this command's name, but not
          the chromite command name or chromite options.
      chroot_config: A SafeConfigParser for the chroot config; or None chromite
          was called from within the chroot.
      argv: None when called normally, but contains argv with board stripped off
          if we call ourselves with utils.EnterChroot().
      build_config: None when called normally, but contains the SafeConfigParser
          for the build config if we call ourselves with utils.EnterChroot().
          Note that even when called through utils.EnterChroot(), could still
          be None if user chose 'HOST' as the target.
    """
    # If we didn't get called through EnterChroot, we need to read the build
    # config.
    if argv is None:
      # We look for the build config without calling OptionParser.  This means
      # that the board _needs_ to be first (if it's specified) and all options
      # will be passed straight to our subcommand.
      argv, build_config = utils.GetBuildConfigFromArgs(raw_argv[1:])

    # Enter the chroot if needed...
    if not cros_lib.IsInsideChroot():
      self._oper.Info('ENTERING THE CHROOT')
      utils.EnterChroot(chroot_config, (self, 'Run'), raw_argv, argv=argv,
                        build_config=build_config)
    else:
      # We'll put CWD as src/scripts when running the command.  Since everyone
      # running by hand has their cwd there, it is probably the safest.
      cwd = os.path.join(utils.SRCROOT_PATH, 'src', 'scripts')

      # Get command to call.  If build_config is None, it means host.
      if build_config is None:
        argv_prefix = self._host_cmd
      else:
        # Make argv_prefix w/ target.
        target_name = build_config.get('DEFAULT', 'target')
        argv_prefix = [arg % target_name for arg in self._target_cmd]

      # Not a great way to to specify arguments, but works for now...  Wrapped
      # commands are not wonderful interfaces anyway...
      if self._need_args and not argv:
        while True:
          sys.stderr.write('arg %d (blank to exit): ' % (len(argv)+1))
          arg = raw_input()
          if not arg:
            break
          argv.append(arg)

      # Add the prefix...
      argv = argv_prefix + argv

      # Update the environment with anything from the whitelist.
      env = os.environ.copy()
      env.update(self._env_to_add)

      self.cros_env.RunScript(self._name, '', argv, env=env)
