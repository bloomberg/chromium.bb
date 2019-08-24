# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Choose the profile for a board that has been or is being setup."""

from __future__ import print_function

import functools
import os

import six

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import sysroot_lib


# Default value constants.
_DEFAULT_PROFILE = 'base'


def PathPrefixDecorator(f):
  """Add a prefix to the path or paths returned by the decorated function.

  Will not prepend the prefix if the path already starts with the prefix, so the
  decorator may be applied to functions that have mixed sources that may
  or may not already have applied them. This is especially useful for allowing
  tests and CLI args a little more leniency in how paths are provided.
  """
  @functools.wraps(f)
  def wrapper(*args, **kwargs):
    result = f(*args, **kwargs)
    prefix = PathPrefixDecorator.prefix

    if not prefix or not result:
      # Nothing to do.
      return result
    elif not isinstance(result, six.string_types):
      # Transform each path in the collection.
      new_result = []
      for path in result:
        prefixed_path = os.path.join(prefix, path.lstrip(os.sep))
        new_result.append(path if path.startswith(prefix) else prefixed_path)

      return new_result
    elif not result.startswith(prefix):
      # Add the prefix.
      return os.path.join(prefix, result.lstrip(os.sep))

    # An already prefixed path.
    return result

  return wrapper

PathPrefixDecorator.prefix = None


class Error(Exception):
  """Base error for custom exceptions in this script."""


class InvalidArgumentsError(Error):
  """Invalid arguments."""


class MakeProfileIsNotLinkError(Error):
  """The make profile exists but is not a link."""


class ProfileDirectoryNotFoundError(Error):
  """Unable to find the profile directory."""


def ChooseProfile(board, profile):
  """Make the link to choose the profile, print relevant warnings.

  Args:
    board: Board - the board being used.
    profile: Profile - the profile being used.

  Raises:
    OSError when the board's make_profile path exists and is not a link.
  """
  if not os.path.isfile(os.path.join(profile.directory, 'parent')):
    logging.warning("Portage profile directory %s has no 'parent' file. "
                    'This likely means your profile directory is invalid and '
                    'build_packages will fail.', profile.directory)

  current_profile = None
  if os.path.exists(board.make_profile):
    # Only try to read if it exists; we only want it to raise an error when the
    # path exists and is not a link.
    try:
      current_profile = os.readlink(board.make_profile)
    except OSError:
      raise MakeProfileIsNotLinkError('%s is not a link.' % board.make_profile)

  if current_profile == profile.directory:
    # The existing link is what we were going to make, so nothing to do.
    return
  elif current_profile is not None:
    # It exists and is changing, emit warning.
    fmt = {'board': board.board_variant, 'profile': profile.name}
    msg = ('You are switching profiles for a board that is already setup. This '
           'can cause trouble for Portage. If you experience problems with '
           'build_packages you may need to run:\n'
           "\t'setup_board --board %(board)s --force --profile %(profile)s'\n"
           '\nAlternatively, you can correct the dependency graph by using '
           "'emerge-%(board)s -c' or 'emerge-%(board)s -C <ebuild>'.")
    logging.warning(msg, fmt)

  # Make the symlink, overwrites existing link if one already exists.
  osutils.SafeSymlink(profile.directory, board.make_profile, sudo=True)

  # Update the profile override value.
  if profile.override:
    board.profile_override = profile.override


class Profile(object):
  """Simple data container class for the profile data."""
  def __init__(self, name, directory, override):
    self.name = name
    self._directory = directory
    self.override = override

  @property
  @PathPrefixDecorator
  def directory(self):
    return self._directory


def _GetProfile(opts, board):
  """Get the profile list."""
  # Determine the override value - which profile is being selected.
  override = opts.profile if opts.profile else board.profile_override

  profile = _DEFAULT_PROFILE
  profile_directory = None

  if override and os.path.exists(override):
    profile_directory = os.path.abspath(override)
    profile = os.path.basename(profile_directory)
  elif override:
    profile = override

  if profile_directory is None:
    # Build profile directories in reverse order so we can search from most to
    # least specific.
    profile_dirs = ['%s/profiles/%s' % (overlay, profile) for overlay in
                    reversed(board.overlays)]

    for profile_dir in profile_dirs:
      if os.path.isdir(profile_dir):
        profile_directory = profile_dir
        break
    else:
      searched = ', '.join(profile_dirs)
      raise ProfileDirectoryNotFoundError(
          'Profile directory not found, searched in (%s).' % searched)

  return Profile(profile, profile_directory, override)


class Board(object):
  """Manage the board arguments and configs."""

  # Files located on the board.
  MAKE_PROFILE = '%(board_root)s/etc/portage/make.profile'

  def __init__(self, board=None, variant=None, board_root=None):
    """Board constructor.

    board [+ variant] is given preference when both board and board_root are
    provided.

    Preconditions:
      Either board and build_root are not None, or board_root is not None.
        With board + build_root [+ variant] we can construct the board root.
        With the board root we can have the board[_variant] directory.

    Args:
      board: str|None - The board name.
      variant: str|None - The variant name.
      board_root: str|None - The boards fully qualified build directory path.
    """
    if not board and not board_root:
      # Enforce preconditions.
      raise InvalidArgumentsError('Either board or board_root must be '
                                  'provided.')
    elif board:
      # The board and variant can be specified separately, or can both be
      # contained in the board name, separated by an underscore.
      board_split = board.split('_')
      variant_default = variant

      self._board_root = None
    else:
      self._board_root = os.path.normpath(board_root)

      board_split = os.path.basename(self._board_root).split('_')
      variant_default = None

    self.board = board_split.pop(0)
    self.variant = board_split.pop(0) if board_split else variant_default

    if self.variant:
      self.board_variant = '%s_%s' % (self.board, self.variant)
    else:
      self.board_variant = self.board

    self.make_profile = self.MAKE_PROFILE % {'board_root': self.root}
    # This must come after the arguments required to build each variant of the
    # build root have been processed.
    self._sysroot_config = sysroot_lib.Sysroot(self.root)

  @property
  @PathPrefixDecorator
  def root(self):
    if self._board_root:
      return self._board_root

    return os.path.join(cros_build_lib.GetSysroot(self.board_variant))

  @property
  @PathPrefixDecorator
  def overlays(self):
    return self._sysroot_config.GetStandardField(
        sysroot_lib.STANDARD_FIELD_BOARD_OVERLAY).split()

  @property
  def profile_override(self):
    return self._sysroot_config.GetCachedField('PROFILE_OVERRIDE')

  @profile_override.setter
  def profile_override(self, value):
    self._sysroot_config.SetCachedField('PROFILE_OVERRIDE', value)


def _GetBoard(opts):
  """Factory method to build a Board from the parsed CLI arguments."""
  return Board(board=opts.board, variant=opts.variant,
               board_root=opts.board_root)


def GetParser():
  """ArgumentParser builder and argument definitions."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('-b', '--board',
                      default=os.environ.get('DEFAULT_BOARD'),
                      help='The name of the board to set up.')
  parser.add_argument('-r', '--board-root',
                      type='path',
                      help='Board root where the profile should be created.')
  parser.add_argument('-p', '--profile',
                      help='The portage configuration profile to use.')
  parser.add_argument('-v', '--variant', help='Board variant.')

  group = parser.add_argument_group('Advanced options')
  group.add_argument('--filesystem-prefix',
                     type='path',
                     help='Force filesystem accesses to be prefixed by the '
                          'given path.')
  return parser


def ParseArgs(argv):
  """Parse and validate the arguments."""
  parser = GetParser()
  opts = parser.parse_args(argv)

  # See Board.__init__ Preconditions.
  board_valid = opts.board is not None
  board_root_valid = opts.board_root and os.path.exists(opts.board_root)

  if not board_valid and not board_root_valid:
    parser.error('Either board or board_root must be provided.')

  PathPrefixDecorator.prefix = opts.filesystem_prefix
  del opts.filesystem_prefix

  opts.Freeze()
  return opts


def main(argv):
  # Parse arguments.
  opts = ParseArgs(argv)

  # Build and validate the board and profile.
  board = _GetBoard(opts)

  if not os.path.exists(board.root):
    cros_build_lib.Die('The board has not been setup, please run setup_board '
                       'first.')

  try:
    profile = _GetProfile(opts, board)
  except ProfileDirectoryNotFoundError as e:
    cros_build_lib.Die(e.message)

  # Change the profile to the selected.
  logging.info('Selecting profile: %s for %s', profile.directory, board.root)

  try:
    ChooseProfile(board, profile)
  except MakeProfileIsNotLinkError as e:
    cros_build_lib.Die(e.message)
