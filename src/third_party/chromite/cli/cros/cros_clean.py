# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO: Support cleaning /build/*/tmp.
# TODO: Support running `eclean -q packages` on / and the sysroots.
# TODO: Support cleaning sysroots as a destructive option.

"""Clean up working files in a Chromium OS checkout.

If unsure, just use the --safe flag to clean out various objects.
"""

from __future__ import print_function

import errno
import glob
import os
import sys

from chromite.lib import constants
from chromite.cli import command
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import dev_server_wrapper
from chromite.lib import osutils


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


@command.CommandDecorator('clean')
class CleanCommand(command.CliCommand):
  """Clean up working files from the build."""

  @classmethod
  def AddParser(cls, parser):
    """Add parser arguments."""
    super(CleanCommand, cls).AddParser(parser)

    parser.add_argument(
        '--safe',
        default=False,
        action='store_true',
        help='Clean up files that are automatically created.')
    parser.add_argument(
        '-n',
        '--dry-run',
        default=False,
        action='store_true',
        help='Show which paths would be cleaned up.')

    group = parser.add_argument_group(
        'Cache Selection (Advanced)',
        description='Clean out specific caches (--safe does all of these).')
    group.add_argument(
        '--cache',
        default=False,
        action='store_true',
        help='Clean up our shared cache dir.')
    group.add_argument(
        '--chromite',
        default=False,
        action='store_true',
        help='Clean up chromite working directories.')
    group.add_argument(
        '--deploy',
        default=False,
        action='store_true',
        help='Clean files cached by cros deploy.')
    group.add_argument(
        '--flash',
        default=False,
        action='store_true',
        help='Clean files cached by cros flash.')
    group.add_argument(
        '--images',
        default=False,
        action='store_true',
        help='Clean up locally generated images.')
    group.add_argument(
        '--incrementals',
        default=False,
        action='store_true',
        help='Clean up incremental package objects.')
    group.add_argument(
        '--logs',
        default=False,
        action='store_true',
        help='Clean up various build log files.')
    group.add_argument(
        '--workdirs',
        default=False,
        action='store_true',
        help='Clean up build various package build directories.')
    group.add_argument(
        '--chroot-tmp',
        default=False,
        action='store_true',
        help="Empty the chroot's /tmp directory.")

    group = parser.add_argument_group(
        'Unrecoverable Options (Dangerous)',
        description='Clean out objects that cannot be recovered easily.')
    group.add_argument(
        '--clobber',
        default=False,
        action='store_true',
        help='Delete all non-source objects.')
    group.add_argument(
        '--chroot',
        default=False,
        action='store_true',
        help='Delete build chroot (affects all boards).')
    group.add_argument(
        '--board', action='append', help='Delete board(s) sysroot(s).')
    group.add_argument(
        '--sysroots',
        default=False,
        action='store_true',
        help='Delete ALL of the sysroots. This is the same as calling with '
             '--board with every board that has been built.')
    group.add_argument(
        '--autotest',
        default=False,
        action='store_true',
        help='Delete build_externals packages.')

    group = parser.add_argument_group(
        'Advanced Customization',
        description='Advanced options that are rarely be needed.')
    group.add_argument(
        '--sdk-path',
        type='path',
        default=os.path.join(constants.SOURCE_ROOT,
                             constants.DEFAULT_CHROOT_DIR),
        help='The sdk (chroot) path. This only needs to be provided if your '
             'chroot is not in the default location.')

  def __init__(self, options):
    """Initializes cros clean."""
    command.CliCommand.__init__(self, options)

  def Run(self):
    """Perform the cros clean command."""

    # If no option is set, default to "--safe"
    if not (self.options.safe or self.options.clobber or self.options.board or
            self.options.chroot or self.options.cache or self.options.deploy or
            self.options.flash or self.options.images or
            self.options.autotest or self.options.incrementals or
            self.options.chroot_tmp or self.options.sysroots):
      self.options.safe = True

    if self.options.clobber:
      self.options.chroot = True
      self.options.autotest = True
      self.options.safe = True

    if self.options.safe:
      self.options.cache = True
      self.options.chromite = True
      self.options.chroot_tmp = True
      self.options.deploy = True
      self.options.flash = True
      self.options.images = True
      self.options.incrementals = True
      self.options.logs = True
      self.options.workdirs = True

    self.options.Freeze()

    chroot_dir = self.options.sdk_path

    cros_build_lib.AssertOutsideChroot()

    def _LogClean(path):
      logging.notice('would have cleaned: %s', path)

    def Clean(path):
      """Helper wrapper for the dry-run checks"""
      if self.options.dry_run:
        _LogClean(path)
      else:
        osutils.RmDir(path, ignore_missing=True, sudo=True)

    def Empty(path):
      """Helper wrapper for the dry-run checks"""
      if self.options.dry_run:
        logging.notice('would have emptied: %s', path)
      else:
        osutils.EmptyDir(path, ignore_missing=True, sudo=True)

    def CleanNoBindMount(path):
      # This test is a convenience for developers that bind mount these dirs.
      if not os.path.ismount(path):
        Clean(path)
      else:
        logging.debug('Ignoring bind mounted dir: %s', path)

    # Delete this first since many of the caches below live in the chroot.
    if self.options.chroot:
      logging.debug('Remove the chroot.')
      if self.options.dry_run:
        logging.notice('would have cleaned: %s', chroot_dir)
      else:
        cros_build_lib.run(['cros_sdk', '--delete'])

    boards = self.options.board or []
    if self.options.sysroots:
      try:
        boards = os.listdir(os.path.join(chroot_dir, 'build'))
      except OSError as e:
        if e.errno != errno.ENOENT:
          raise
    for b in boards:
      logging.debug('Clean up the %s sysroot.', b)
      Clean(os.path.join(chroot_dir, 'build', b))

    if self.options.chroot_tmp:
      logging.debug('Empty chroot tmp directory.')
      Empty(os.path.join(chroot_dir, 'tmp'))

    if self.options.cache:
      logging.debug('Clean the common cache.')
      CleanNoBindMount(self.options.cache_dir)

      # Recreate dirs that cros_sdk does when entering.
      # TODO: When sdk_lib/enter_chroot.sh is moved to chromite, we should unify
      # with those code paths.
      if not self.options.dry_run:
        for subdir in ('ccache', 'host', 'target'):
          osutils.SafeMakedirs(
              os.path.join(self.options.cache_dir, 'distfiles', subdir))
        os.chmod(
            os.path.join(self.options.cache_dir, 'distfiles', 'ccache'), 0o2775)

    if self.options.chromite:
      logging.debug('Clean chromite workdirs.')
      Clean(os.path.join(constants.CHROMITE_DIR, 'venv', 'venv'))
      Clean(os.path.join(constants.CHROMITE_DIR, 'venv', '.venv_lock'))

    if self.options.deploy:
      logging.debug('Clean up the cros deploy cache.')
      for subdir in ('custom-packages', 'gmerge-packages'):
        for d in glob.glob(os.path.join(chroot_dir, 'build', '*', subdir)):
          Clean(d)

    if self.options.flash:
      if self.options.dry_run:
        _LogClean(dev_server_wrapper.DEFAULT_STATIC_DIR)
      else:
        dev_server_wrapper.DevServerWrapper.WipeStaticDirectory()

    if self.options.images:
      logging.debug('Clean the images cache.')
      cache_dir = os.path.join(constants.SOURCE_ROOT, 'src', 'build')
      CleanNoBindMount(cache_dir)

    if self.options.incrementals:
      logging.debug('Clean package incremental objects.')
      Clean(os.path.join(chroot_dir, 'var', 'cache', 'portage'))
      for d in glob.glob(
          os.path.join(chroot_dir, 'build', '*', 'var', 'cache', 'portage')):
        Clean(d)

    if self.options.logs:
      logging.debug('Clean log files.')
      Clean(os.path.join(chroot_dir, 'var', 'log'))
      for d in glob.glob(
          os.path.join(chroot_dir, 'build', '*', 'tmp', 'portage', 'logs')):
        Clean(d)

    if self.options.workdirs:
      logging.debug('Clean package workdirs.')
      Clean(os.path.join(chroot_dir, 'var', 'tmp', 'portage'))
      Clean(os.path.join(constants.CHROMITE_DIR, 'venv', 'venv'))
      for d in glob.glob(
          os.path.join(chroot_dir, 'build', '*', 'tmp', 'portage')):
        Clean(d)

    if self.options.autotest:
      logging.debug('Clean build_externals.')
      packages_dir = os.path.join(constants.SOURCE_ROOT, 'src', 'third_party',
                                  'autotest', 'files', 'site-packages')
      Clean(packages_dir)
