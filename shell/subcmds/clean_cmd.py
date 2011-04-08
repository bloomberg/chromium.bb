# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of the 'clean' chromite command."""

# Python imports
import optparse
import os
import sys


# Local imports
import chromite.lib.cros_build_lib as cros_lib
from chromite.shell import utils
from chromite.shell import subcmd


def _DoClean(cros_env, chroot_config, build_config, want_force_yes):
  """Clean a target.

  Args:
    cros_env: Chromite environment to use for this command.
    chroot_config: A SafeConfigParser representing the config for the chroot.
    build_config: A SafeConfigParser representing the build config.
    want_force_yes: If True, we won't ask any questions--we'll just assume
        that the user really wants to kill the directory.  If False, we'll
        show UI asking the user to confirm.
  """
  # We'll need the directory so we can delete stuff; this is a chroot path.
  board_dir = utils.GetBoardDir(build_config)

  # If not in the chroot, convert board_dir into a non-chroot path...
  if not cros_lib.IsInsideChroot():
    chroot_dir = utils.GetChrootAbsDir(chroot_config)

    # We'll need to make the board directory relative to the chroot.
    assert board_dir.startswith('/'), 'Expected unix-style, absolute path.'
    board_dir = board_dir.lstrip('/')
    board_dir = os.path.join(chroot_dir, board_dir)

  if not os.path.isdir(board_dir):
    cros_lib.Die("Nothing to clean: the board directory doesn't exist.\n  %s" %
        board_dir)

  if not want_force_yes:
    sys.stderr.write('\n'
                     'Board dir is at: %s\n'
                     'Are you sure you want to delete it (YES/NO)? ' %
                     board_dir)
    answer = raw_input()
    if answer.lower() not in ('y', 'ye', 'yes'):
      cros_lib.Die("You must answer 'yes' if you want to proceed.")

  # Since we're about to do a sudo rm -rf, these are just extra precautions.
  # This shouldn't be the only place testing these (assert fails are ugly and
  # can be turned off), but better safe than sorry.
  # Note that the restriction on '*' is a bit unnecessary, since no shell
  # expansion should happen.  ...but again, I'd rather be safe.
  assert os.path.isabs(board_dir), 'Board dir better be absolute'
  assert board_dir != '/', 'Board dir better not be /'
  assert '*' not in board_dir, 'Board dir better not have any *s'
  assert build_config.get('DEFAULT', 'target'), 'Target better not be blank'
  assert build_config.get('DEFAULT', 'target') in board_dir, \
         'Target name better be in board dir'

  arg_list = ['--', 'rm', '-rf', board_dir]
  cros_env.RunScript('DELETING: %s' % board_dir, 'sudo', arg_list)


def _DoDistClean(cros_env, chroot_config, want_force_yes):
  """Remove the whole chroot.

  Args:
    cros_env: Chromite environment to use for this command.
    chroot_config: A SafeConfigParser representing the config for the chroot.
    want_force_yes: If True, we won't ask any questions--we'll just assume
        that the user really wants to kill the directory.  If False, we'll
        show UI asking the user to confirm.
  """
  if cros_lib.IsInsideChroot():
    cros_lib.Die('Please exit the chroot before trying to delete it.')

  chroot_dir = utils.GetChrootAbsDir(chroot_config)
  if not want_force_yes:
    sys.stderr.write('\n'
                     'Chroot is at: %s\n'
                     'Are you sure you want to delete it (YES/NO)? ' %
                     chroot_dir)
    answer = raw_input()
    if answer.lower() not in ('y', 'ye', 'yes'):
      cros_lib.Die("You must answer 'yes' if you want to proceed.")

  # Can pass argv and not shell=True, since no user flags.  :)
  arg_list = ['--chroot=%s' % chroot_dir, '--delete']

  # Run it.  Pass any failures upward.
  cros_env.RunScript('REMOVING CHROOT', './make_chroot', arg_list, cwd=cwd)


class CleanCmd(subcmd.ChromiteCmd):
  """Clean out built packages for a target; if target=host, deletes chroot."""

  def Run(self, raw_argv, chroot_config=None):
    """Run the command.

    Args:
      raw_argv: Command line arguments, including this command's name, but not
          the chromite command name or chromite options.
      chroot_config: A SafeConfigParser for the chroot config; or None chromite
          was called from within the chroot.
    """
    # Parse options for command...
    usage_str = ('usage: %%prog [chromite_options] %s [options] [target]' %
                 raw_argv[0])
    parser = optparse.OptionParser(usage=usage_str)
    parser.add_option('-y', '--yes', default=False, action='store_true',
                      help='Answer "YES" to "are you sure?" questions.')
    (options, argv) = parser.parse_args(raw_argv[1:])

    # Make sure the chroot exists first, before possibly prompting for board...
    # ...not really required, but nice for the user...
    if not cros_lib.IsInsideChroot():
      if not utils.DoesChrootExist(chroot_config):
        cros_lib.Die("Nothing to clean: the chroot doesn't exist.\n  %s" %
            utils.GetChrootAbsDir(chroot_config))

    # Load the build config...
    argv, build_config = utils.GetBuildConfigFromArgs(argv)
    if argv:
      cros_lib.Die('Unknown arguments: %s' % ' '.join(argv))

    # If they do clean host, we'll delete the whole chroot
    if build_config is None:
      _DoDistClean(self.cros_env, chroot_config, options.yes)
    else:
      _DoClean(self.cros_env, chroot_config, build_config, options.yes)
