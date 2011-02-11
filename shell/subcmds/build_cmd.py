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


def _DoMakeChroot(chroot_config, clean_first):
  """Build the chroot, if needed.

  Args:
    chroot_config: A SafeConfigParser representing the config for the chroot.
    clean_first: Delete any old chroot first.
  """
  # Skip this whole command if things already exist.
  # TODO(dianders): Theoretically, calling make_chroot a second time is OK
  # and "fixes up" the chroot.  ...but build_packages will do the fixups
  # anyway (I think), so this isn't that important.
  chroot_dir = utils.GetChrootAbsDir(chroot_config)
  if (not clean_first) and utils.DoesChrootExist(chroot_config):
    cros_lib.Info('%s already exists, skipping make_chroot.' % chroot_dir)
    return

  cros_lib.Info('MAKING THE CHROOT')

  # Put together command.
  cmd_list = [
      './make_chroot',
      '--chroot="%s"' % chroot_dir,
      chroot_config.get('CHROOT', 'make_chroot_flags'),
  ]
  if clean_first:
    cmd_list.insert(1, '--replace')

  # We're going convert to a string and force the shell to do all of the
  # splitting of arguments, since we're throwing all of the flags from the
  # config file in there.
  cmd = ' '.join(cmd_list)

  # We'll put CWD as src/scripts when running the command.  Since everyone
  # running by hand has their cwd there, it is probably the safest.
  cwd = os.path.join(utils.SRCROOT_PATH, 'src', 'scripts')

  # Run it.  Exceptions will cause the program to exit.
  cros_lib.RunCommand(cmd, shell=True, cwd=cwd, ignore_sigint=True)


def _DoSetupBoard(build_config, clean_first):
  """Setup the board, if needed.

  This just runs the setup_board command with the proper args, if needed.

  Args:
    build_config: A SafeConfigParser representing the build config.
    clean_first: Delete any old board config first.
  """
  # Skip this whole command if things already exist.
  board_dir = utils.GetBoardDir(build_config)
  if (not clean_first) and os.path.isdir(board_dir):
    cros_lib.Info('%s already exists, skipping setup_board.' % board_dir)
    return

  cros_lib.Info('SETTING UP THE BOARD')

  # Put together command.
  cmd_list = [
      './setup_board',
      '--board="%s"' % build_config.get('DEFAULT', 'target'),
      build_config.get('BUILD', 'setup_board_flags'),
  ]
  if clean_first:
    cmd_list.insert(1, '--force')

  # We're going convert to a string and force the shell to do all of the
  # splitting of arguments, since we're throwing all of the flags from the
  # config file in there.
  cmd = ' '.join(cmd_list)

  # We'll put CWD as src/scripts when running the command.  Since everyone
  # running by hand has their cwd there, it is probably the safest.
  cwd = os.path.join(utils.SRCROOT_PATH, 'src', 'scripts')

  # Run it.  Exceptions will cause the program to exit.
  cros_lib.RunCommand(cmd, shell=True, cwd=cwd, ignore_sigint=True)


def _DoBuildPackages(build_config):
  """Build packages.

  This just runs the build_packages command with the proper args.

  Args:
    build_config: A SafeConfigParser representing the build config.
  """
  cros_lib.Info('BUILDING PACKAGES')

  # Put together command.  We're going to force the shell to do all of the
  # splitting of arguments, since we're throwing all of the flags from the
  # config file in there.
  cmd = '%s ./build_packages --board="%s" %s' % (
      build_config.get('BUILD', 'build_packages_environ'),
      build_config.get('DEFAULT', 'target'),
      build_config.get('BUILD', 'build_packages_flags')
  )

  # We'll put CWD as src/scripts when running the command.  Since everyone
  # running by hand has their cwd there, it is probably the safest.
  cwd = os.path.join(utils.SRCROOT_PATH, 'src', 'scripts')

  # Run it.  Exceptions will cause the program to exit.
  cros_lib.RunCommand(cmd, shell=True, cwd=cwd, ignore_sigint=True)


def _DoBuildImage(build_config):
  """Build an image.

  This just runs the build_image command with the proper args.

  Args:
    build_config: A SafeConfigParser representing the build config.
  """
  cros_lib.Info('BUILDING THE IMAGE')

  # Put together command.  We're going to force the shell to do all of the
  # splitting of arguments, since we're throwing all of the flags from the
  # config file in there.
  cmd = '%s ./build_image --board="%s" %s' % (
      build_config.get('IMAGE', 'build_image_environ'),
      build_config.get('DEFAULT', 'target'),
      build_config.get('IMAGE', 'build_image_flags')
  )

  # We'll put CWD as src/scripts when running the command.  Since everyone
  # running by hand has their cwd there, it is probably the safest.
  cwd = os.path.join(utils.SRCROOT_PATH, 'src', 'scripts')

  # Run it.  Exceptions will cause the program to exit.
  cros_lib.RunCommand(cmd, shell=True, cwd=cwd, ignore_sigint=True)


def _DoImagePostProcessing(build_config):
  """Do post processing steps after the build image runs.

  Args:
    build_config: A SafeConfigParser representing the build config.
  """
  # We'll put CWD as src/scripts when running the commands, since many of these
  # legacy commands live in src/scripts.
  # TODO(dianders): Don't set CWD once crosutils are properly installed.
  cwd = os.path.join(utils.SRCROOT_PATH, 'src', 'scripts')

  # The user specifies a list of "steps" in this space-separated variable.
  # We'll use each step name to construct other variable names to look for
  # the actual commands.
  steps = build_config.get('IMAGE', 'post_process_steps')
  for step_name in steps.split():
    cros_lib.Info('IMAGING POST-PROCESS: %s' % step_name)

    # Get the name of the variable that the user stored the cmd in.
    cmd_var_name = 'post_process_%s_cmd' % step_name

    # Run the command.  Exceptions will cause the program to exit.
    cmd = build_config.get('IMAGE', cmd_var_name)
    cros_lib.RunCommand(cmd, shell=True, cwd=cwd, ignore_sigint=True)


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

      _DoMakeChroot(chroot_config, want_clean_chroot)

      if build_config is not None:
        utils.EnterChroot(chroot_config, (self, 'Run'), raw_argv,
                          build_config=build_config, loaded_config=True)

      cros_lib.Info('Done building.')
    else:
      if build_config is None:
        cros_lib.Die("You can't build the host chroot from within the chroot.")

      _DoSetupBoard(build_config, options.clean)
      _DoBuildPackages(build_config)
      _DoBuildImage(build_config)
      _DoImagePostProcessing(build_config)
