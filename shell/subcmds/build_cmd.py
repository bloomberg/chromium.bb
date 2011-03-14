# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of the 'build' chromite command."""

# Python imports
import optparse
import os


# Local imports
import chromite.lib.cros_build_lib as cros_lib
from chromite.shell import utils
from chromite.shell import subcmd

# TODO(sjg): I would prefer that these methods be inside the BuildCmd() class.


def _DoMakeChroot(cros_env, chroot_config, clean_first):
  """Build the chroot, if needed.

  Args:
    cros_env: Chromite environment to use for this command.
    chroot_config: A SafeConfigParser representing the config for the chroot.
    clean_first: Delete any old chroot first.
  """
  # Skip this whole command if things already exist.
  # TODO(dianders): Theoretically, calling make_chroot a second time is OK
  # and "fixes up" the chroot.  ...but build_packages will do the fixups
  # anyway (I think), so this isn't that important.
  chroot_dir = utils.GetChrootAbsDir(chroot_config)
  if (not clean_first) and utils.DoesChrootExist(chroot_config):
    cros_env.GetOperation().Info('%s already exists, skipping make_chroot.' %
        chroot_dir)
    return

  # Put together command.
  arg_list = [
      '--chroot="%s"' % chroot_dir,
      chroot_config.get('CHROOT', 'make_chroot_flags'),
  ]
  if clean_first:
    arg_list.insert(0, '--replace')

  cros_env.RunScript('MAKING THE CHROOT', './make_chroot', arg_list)


def _DoSetupBoard(cros_env, build_config, clean_first):
  """Setup the board, if needed.

  This just runs the setup_board command with the proper args, if needed.

  Args:
    cros_env: Chromite environment to use for this command.
    build_config: A SafeConfigParser representing the build config.
    clean_first: Delete any old board config first.
  """
  # Skip this whole command if things already exist.
  board_dir = utils.GetBoardDir(build_config)
  if (not clean_first) and os.path.isdir(board_dir):
    cros_env.GetOperation().Info('%s already exists, skipping setup_board.' %
        board_dir)
    return

  # Put together command.
  cmd_list = [
      '--board="%s"' % build_config.get('DEFAULT', 'target'),
      build_config.get('BUILD', 'setup_board_flags'),
  ]
  if clean_first:
    arg_list.insert(0, '--force')

  cros_env.RunScript('SETTING UP THE BOARD', './setup_board', arg_list)


def _DoBuildPackages(cros_env, build_config):
  """Build packages.

  This just runs the build_packages command with the proper args.

  Args:
    cros_env: Chromite environment to use for this command.
    build_config: A SafeConfigParser representing the build config.
  """

  # Put together command.  We're going to force the shell to do all of the
  # splitting of arguments, since we're throwing all of the flags from the
  # config file in there.
  arg_list = ['--board="%s"' % build_config.get('DEFAULT', 'target'),
      build_config.get('BUILD', 'build_packages_flags')
  ]

  cros_env.RunScript('BUILDING PACKAGES', './build_packages', arg_list,
      shell_vars=build_config.get('BUILD', 'build_packages_environ'))


def _DoBuildImage(cros_env, build_config):
  """Build an image.

  This just runs the build_image command with the proper args.

  Args:
    build_config: A SafeConfigParser representing the build config.
  """

  # Put together command.  We're going to force the shell to do all of the
  # splitting of arguments, since we're throwing all of the flags from the
  # config file in there.
  arg_list = ['--board="%s"' % build_config.get('DEFAULT', 'target'),
      build_config.get('IMAGE', 'build_image_flags')
  ]

  cros_env.RunScript('BUILDING THE IMAGE', './build_image', arg_list,
      shell_vars=build_config.get('IMAGE', 'build_image_environ'))


def _DoImagePostProcessing(cros_env, build_config):
  """Do post processing steps after the build image runs.

  Args:
    build_config: A SafeConfigParser representing the build config.
  """
  # The user specifies a list of "steps" in this space-separated variable.
  # We'll use each step name to construct other variable names to look for
  # the actual commands.
  steps = build_config.get('IMAGE', 'post_process_steps')
  for step_name in steps.split():
    # Get the name of the variable that the user stored the cmd in.
    cmd_var_name = 'post_process_%s_cmd' % step_name

    # Run the command.  Exceptions will cause the program to exit.
    cmd = build_config.get('IMAGE', cmd_var_name)
    cros_env.RunScript('IMAGING POST-PROCESS: %s' % step_name, '', [cmd])


class BuildCmd(subcmd.ChromiteCmd):
  """Build the chroot (if needed), the packages for a target, and the image."""

  def Run(self, raw_argv, chroot_config=None,
          loaded_config=False, build_config=None):
    """Run the command.

    Args:
      raw_argv: Command line arguments, including this command's name, but not
          the chromite command name or chromite options.
      chroot_config: A SafeConfigParser for the chroot config; or None chromite
          was called from within the chroot.
      loaded_config: If True, we've already loaded the config.
      build_config: None when called normally, but contains the SafeConfigParser
          for the build config if we call ourselves with _DoEnterChroot().  Note
          that even when called through _DoEnterChroot(), could still be None
          if user chose 'HOST' as the target.
    """
    # Parse options for command...
    usage_str = ('usage: %%prog [chromite_options] %s [options] [target]' %
                 raw_argv[0])
    parser = optparse.OptionParser(usage=usage_str)
    parser.add_option('--clean', default=False, action='store_true',
                      help='Clean before building.')
    (options, argv) = parser.parse_args(raw_argv[1:])

    # Load the build config if needed...
    if not loaded_config:
      argv, build_config = utils.GetBuildConfigFromArgs(argv)
      if argv:
        cros_lib.Die('Unknown arguments: %s' % ' '.join(argv))

    if not cros_lib.IsInsideChroot():
      # Note: we only want to clean the chroot if they do --clean and have the
      # host target.  If they do --clean and have a board target, it means
      # that they just want to clean the board...
      want_clean_chroot = options.clean and build_config is None

      _DoMakeChroot(self.cros_env, chroot_config, want_clean_chroot)

      if build_config is not None:
        self._oper.Info('ENTERING THE CHROOT')
        utils.EnterChroot(chroot_config, (self, 'Run'), raw_argv,
                          build_config=build_config, loaded_config=True)

      self._oper.Info('Done building.')
    else:
      if build_config is None:
        cros_lib.Die("You can't build the host chroot from within the chroot.")

      _DoSetupBoard(self.cros_env, build_config, options.clean)
      _DoBuildPackages(self.cros_env, build_config)
      _DoBuildImage(self.cros_env, build_config)
      _DoImagePostProcessing(self.cros_env, build_config)
