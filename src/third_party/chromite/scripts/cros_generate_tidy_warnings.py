# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a clang-tidy tarball for the clang-tidy builder."""

from __future__ import print_function

import os
import shutil
import sys

from chromite.lib import cros_build_lib
from chromite.lib import commandline
from chromite.lib import osutils
from chromite.lib import sudo


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


DEFAULT_NAME = 'clang_tidy_warnings.tar.xz'
TIDY_WARNINGS = 'clang_tidy_warnings'
PARSING_SCRIPT = ('/mnt/host/source/src/third_party/toolchain-utils/'
                  'clang_tidy/clang_tidy_parse_build_log.py')
WORKING_DIR = '/usr/bin'


def ParseCommandLine(argv):
  """Parse args, and run environment-independent checks."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--board', required=True,
                      help=('The board to generate the sysroot for.'))
  parser.add_argument('--logs-dir', required=True,
                      help=('The directory containg the logs files to '
                            'be parsed.'))
  parser.add_argument('--out-dir', type='path', required=True,
                      help='Directory to place the generated tarball.')
  parser.add_argument('--out-file', default=DEFAULT_NAME,
                      help='The name to give to the tarball. '
                           'Defaults to %(default)s.')
  options = parser.parse_args(argv)

  return options


class GenerateTidyWarnings(object):
  """Wrapper for generation functionality."""

  def __init__(self, warnings_dir, options):
    """Initialize

    Args:
      warnings_dir: Path to warnings directory.
      options: Parsed options.
    """
    self.warnings_dir = warnings_dir
    self.options = options

  def _FindLogFiles(self, logs_dir):
    files = []
    filelist = os.listdir(logs_dir)
    for f in filelist:
      logfile = os.path.join(logs_dir, f)
      files.append(logfile)
    return files

  def _ParseLogFiles(self):
    log_files = self._FindLogFiles(self.options.logs_dir)
    for f in log_files:
      # Copy log file to output directory because this is what we want to
      # upload to gs
      shutil.copy2(f, self.warnings_dir)

  def _CreateTarball(self):
    target = os.path.join(self.options.out_dir, self.options.out_file)
    cros_build_lib.CreateTarball(target, self.warnings_dir, sudo=True)

  def Perform(self):
    """Generate the warnings files."""
    self._ParseLogFiles()
    self._CreateTarball()


def FinishParsing(options):
  """Run environment dependent checks on parsed args."""
  target = os.path.join(options.out_dir, options.out_file)
  if os.path.exists(target):
    cros_build_lib.Die('Output file %r already exists.' % target)

  if not os.path.isdir(options.out_dir):
    cros_build_lib.Die(
        'Non-existent directory %r specified for --out-dir' % options.out_dir)


def main(argv):
  options = ParseCommandLine(argv)
  FinishParsing(options)

  cros_build_lib.AssertInsideChroot()

  with sudo.SudoKeepAlive(ttyless_sudo=False):
    with osutils.TempDir(set_global=True, sudo_rm=True) as tempdir:
      warnings_dir = os.path.join(tempdir, TIDY_WARNINGS)
      os.mkdir(warnings_dir)
      GenerateTidyWarnings(warnings_dir, options).Perform()
