#!/usr/bin/python
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import multiprocessing
import tempfile
import sys

from chromite.lib import osutils

def main(argv):
  """Build packages according to options specified on command-line."""

  if os.getuid() != 0:
    cros_build_lib.Die("superuser access required")

  scripts_dir = os.path.abspath(__file__ + "/../../..")
  builder = PackageBuilder(scripts_dir)
  options, _ = builder.ParseArgs(argv)

  # Calculate packages to install.
  # TODO(davidjames): Grab these from a spec file.
  packages = ["chromeos-base/chromeos"]
  if options.withdev:
    packages.append("chromeos-base/chromeos-dev")
  if options.withfactory:
    packages.append("chromeos-base/chromeos-factoryinstall")
  if options.withtest:
    packages.append("chromeos-base/chromeos-test")

  if options.usetarball:
    builder.ExtractTarball(options, packages)
  else:
    builder.BuildTarball(options, packages)


def _Apply(args):
  """Call the function specified in args[0], with arguments in args[1:]."""
  return apply(args[0], args[1:])


def _GetLatestPrebuiltPrefix(board):
  """Get the latest prebuilt prefix for the specified board.

  Args:
    board: The board you want prebuilts for.
  Returns:
    Latest prebuilt prefix.
  """
  # TODO(davidjames): Also append profile names here.
  prefix = "http://commondatastorage.googleapis.com/chromeos-prebuilt/board"
  with tempfile.NamedTemporaryFile() as tmpfile:
    _Run("curl '%s/%s-latest' -o %s" % (prefix, board, tmpfile.name), retries=3)
    tmpfile.seek(0)
    latest = tmpfile.read().strip()
  return "%s/%s" % (prefix, latest)


def _GetPrebuiltDownloadCommands(prefix):
  """Return a list of commands for grabbing packages.

  There must be a file called "packages/Packages" that contains the list of
  packages. The specified list of commands will fill the packages directory
  with the bzipped packages from the specified prefix.

  Args:
    prefix: Url prefix to download packages from.
  Returns:
    List of commands for grabbing packages.
  """

  cmds = []
  for line in file("packages/Packages"):
    if line.startswith("CPV: "):
      pkgpath, pkgname = line.replace("CPV: ", "").strip().split("/")
      path = "%s/%s.tbz2" % (pkgpath, pkgname)
      url = "%s/%s" % (prefix, path)
      dirname = "packages/%s" % pkgpath
      fullpath = "packages/%s" % path
      osutils.SafeMakedirs(dirname)
      if not os.path.exists(fullpath):
        cmds.append("curl -s %s -o %s" % (url, fullpath))
  return cmds


def _Run(cmd, retries=0):
  """Run the specified command.

  If the command fails, and the retries have been exhausted, the program exits
  with an appropriate error message.

  Args:
    cmd: The command to run.
    retries: If exit code is non-zero, retry this many times.
  """
  try:
    result = cros_build_lib.RunCommandWithRetries(retries, cmd, shell=True)
  except cros_build_lib.RunCommandException:
    cros_build_lib.Die("Command failed, exiting: %s" % cmd)
  cros_build_lib.Info("Command succeeded: %s" % cmd)


def _RunManyParallel(cmds, retries=0):
  """Run list of provided commands in parallel.

  To work around a bug in the multiprocessing module, we use map_async instead
  of the usual map function. See http://bugs.python.org/issue9205

  Args:
    cmds: List of commands to run.
    retries: Number of retries per command.
  """
  # TODO(davidjames): Move this to common library.
  pool = multiprocessing.Pool()
  args = []
  for cmd in cmds:
    args.append((_Run, cmd, retries))
  result = pool.map_async(_Apply, args, chunksize=1)
  while True:
    try:
      result.get(60*60)
      break
    except multiprocessing.TimeoutError:
      pass


class PackageBuilder(object):
  """A class for building and extracting tarballs of Chromium OS packages."""

  def __init__(self, scripts_dir):
    self.scripts_dir = scripts_dir

  def BuildTarball(self, options, packages):
    """Build a tarball with the specified packages.

    Args:
      options: Options object, as output by ParseArgs.
      packages: List of packages to build.
    """

    board = options.board

    # Run setup_board. TODO(davidjames): Integrate the logic used in
    # setup_board into chromite.
    _Run("%s/setup_board --force --board=%s" % (self.scripts_dir, board))

    # Create complete build directory
    _Run(self._EmergeBoardCmd(options, packages))

    # Archive build directory as tarballs
    os.chdir("/build/%s" % board)
    cmds = [
        "tar -c --wildcards --exclude='usr/lib/debug/*' "
        "--exclude='packages/*' * | pigz -c > packages/%s-build.tgz" % board,
        "tar -c usr/lib/debug/* | pigz -c > packages/%s-debug.tgz" % board
    ]

    # Run list of commands.
    _RunManyParallel(cmds)

  def ExtractTarball(self, options, packages):
    """Extract the latest build tarball, then update the specified packages.

    Args:
      options: Options object, as output by ParseArgs.
      packages: List of packages to update.
    """

    board = options.board
    prefix = _GetLatestPrebuiltPrefix(board)

    # If the user doesn't have emerge-${BOARD} setup yet, we need to run
    # setup_board. TODO(davidjames): Integrate the logic used in setup_board
    # into chromite.
    if not os.path.exists("/usr/local/bin/emerge-%s" % board):
      _Run("%s/setup_board --force --board=%s" % (self.scripts_dir, board))

    # Delete old build directory. This process might take a while, so do it in
    # the background.
    cmds = []
    if os.path.exists("/build/%s" % board):
      tempdir = tempfile.mkdtemp()
      _Run("mv /build/%s %s" % (board, tempdir))
      cmds.append("rm -rf %s" % tempdir)

    # Create empty build directory, and chdir into it.
    os.makedirs("/build/%s/packages" % board)
    os.chdir("/build/%s" % board)

    # Download and expand build tarball.
    build_url = "%s/%s-build.tgz" % (prefix, board)
    cmds.append("curl -s %s | tar -xz" % build_url)

    # Download and expand debug tarball (if requested).
    if options.debug:
      debug_url = "%s/%s-debug.tgz" % (prefix, board)
      cmds.append("curl -s %s | tar -xz" % debug_url)

    # Download prebuilt packages.
    _Run("curl '%s/Packages' -o packages/Packages" % prefix, retries=3)
    cmds.extend(_GetPrebuiltDownloadCommands(prefix))

    # Run list of commands, with three retries per command, in case the network
    # is flaky.
    _RunManyParallel(cmds, retries=3)

    # Emerge remaining packages.
    _Run(self._EmergeBoardCmd(options, packages))

  def ParseArgs(self, argv):
    """Parse arguments from the command line using optparse."""

    # TODO(davidjames): We should use spec files for this.
    default_board = self._GetDefaultBoard()
    parser = optparse.OptionParser()
    parser.add_option("--board", dest="board", default=default_board,
                      help="The board to build packages for.")
    parser.add_option("--debug", action="store_true", dest="debug",
                      default=False, help="Include debug symbols.")
    parser.add_option("--nowithdev", action="store_false", dest="withdev",
                      default=True,
                      help="Don't build useful developer friendly utilities.")
    parser.add_option("--nowithtest", action="store_false", dest="withtest",
                      default=True, help="Build packages required for testing.")
    parser.add_option("--nowithfactory", action="store_false",
                      dest="withfactory", default=True,
                      help="Build factory installer")
    parser.add_option("--nousepkg", action="store_false",
                      dest="usepkg", default=True,
                      help="Don't use binary packages.")
    parser.add_option("--nousetarball", action="store_false",
                      dest="usetarball", default=True,
                      help="Don't use tarball.")
    parser.add_option("--nofast", action="store_false", dest="fast",
                      default=True,
                      help="Don't merge packages in parallel.")

    return parser.parse_args(argv)

  def _EmergeBoardCmd(self, options, packages):
    """Calculate board emerge command."""
    board = options.board
    scripts_dir = self.scripts_dir
    emerge_board = "emerge-%s" % board
    if options.fast:
      emerge_board = "%s/parallel_emerge --board=%s" % (scripts_dir, board)
    usepkg = ""
    if options.usepkg:
      usepkg = "g"
    return "%s -uDNv%s %s" % (emerge_board, usepkg, " ".join(packages))

  def _GetDefaultBoard(self):
    """Get the default board configured by the user."""

    default_board_file = "%s/.default_board" % self.scripts_dir
    default_board = None
    if os.path.exists(default_board_file):
      default_board = osutils.ReadFile(default_board_file).strip()
    return default_board
