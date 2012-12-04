# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Calculate what workon packages have changed since the last build.

A workon package is treated as changed if any of the below are true:
  1) The package is not installed.
  2) A file exists in the associated repository which has a newer modification
     time than the installed package.
  3) The source ebuild has a newer modification time than the installed package.

Some caveats:
  - We do not look at eclasses. This replicates the existing behavior of the
    commit queue, which also does not look at eclass changes.
  - We do not try to fallback to the non-workon package if the local tree is
    unmodified. This is probably a good thing, since developers who are
    "working on" a package want to compile it locally.
  - Portage only stores the time that a package finished building, so we
    aren't able to detect when users modify source code during builds.
"""

import errno
import logging
import multiprocessing
import optparse
import os
import Queue

from chromite.buildbot import constants
from chromite.buildbot import portage_utilities
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import parallel


class WorkonProjectsMonitor(object):
  """Class for monitoring the last modification time of workon projects.

  Members:
    _tasks: A list of the (project, path) pairs to check.
    _result_queue: A queue. When GetProjectModificationTimes is called,
      (project, mtime) tuples are pushed onto the end of this queue.
  """

  def __init__(self, projects):
    """Create a new object for checking what projects were modified and when.

    Args:
      projects: A list of the project names we are interested in monitoring.
    """
    manifest = git.ManifestCheckout.Cached(constants.SOURCE_ROOT)
    self._tasks = [(name, manifest.GetProjectPath(name, True))
                   for name in set(projects).intersection(manifest.projects)]
    self._result_queue = multiprocessing.Queue(len(self._tasks))

  def _EnqueueProjectModificationTime(self, project, path):
    """Calculate the last time that this project was modified, and enqueue it.

    Args:
      project: The project to look at.
      path: The path associated with the specified project.
    """
    if os.path.isdir(path):
      self._result_queue.put((project, self._LastModificationTime(path)))

  def _LastModificationTime(self, path):
    """Calculate the last time a directory subtree was modified.

    Args:
      path: Directory to look at.
    """
    cmd = 'find . -name .git -prune -o -printf "%T@\n" | sort -nr | head -n1'
    ret = cros_build_lib.RunCommandCaptureOutput(cmd, cwd=path, shell=True,
                                                 print_cmd=False)
    return float(ret.output) if ret.output else 0

  def GetProjectModificationTimes(self):
    """Get the last modification time of each specified project.

    Returns:
      A dictionary mapping project names to last modification times.
    """
    task = self._EnqueueProjectModificationTime
    parallel.RunTasksInProcessPool(task, self._tasks)

    # Create a dictionary mapping project names to last modification times.
    # All of the workon projects are already stored in the queue, so we can
    # retrieve them all without waiting any longer.
    mtimes = {}
    while True:
      try:
        project, mtime = self._result_queue.get_nowait()
      except Queue.Empty:
        break
      mtimes[project] = mtime
    return mtimes


class WorkonPackageInfo(object):
  """Class for getting information about workon packages.

  Members:
    cp: The package name (e.g. chromeos-base/power_manager).
    mtime: The modification time of the installed package.
    project: The project associated with the installed package.
    src_ebuild_mtime: The modification time of the source ebuild.
  """

  def __init__(self, cp, mtime, projects, src_ebuild_mtime):
    self.cp = cp
    self.pkg_mtime = int(mtime)
    self.projects = projects
    self.src_ebuild_mtime = src_ebuild_mtime


def ListWorkonPackages(board, host):
  """List the packages that are currently being worked on.

  Args:
    board: The board to look at. If host is True, this should be set to None.
    host: Whether to look at workon packages for the host.
  """
  cmd = [os.path.join(constants.CROSUTILS_DIR, 'cros_workon'), 'list']
  cmd.extend(['--host'] if host else ['--board', board])
  result = cros_build_lib.RunCommandCaptureOutput(cmd, print_cmd=False)
  return result.output.split()


def ListWorkonPackagesInfo(board, host):
  """Find the specified workon packages for the specified board.

  Args:
    board: The board to look at. If host is True, this should be set to None.
    host: Whether to look at workon packages for the host.

  Returns a list of unique packages being worked on.
  """
  # Import portage late so that this script can be imported outside the chroot.
  # pylint: disable=W0404
  import portage.const
  packages = ListWorkonPackages(board, host)
  if not packages:
    return []
  results = {}
  install_root = '/' if host else '/build/%s' % board
  vdb_path = os.path.join(install_root, portage.const.VDB_PATH)
  buildroot, both = constants.SOURCE_ROOT, constants.BOTH_OVERLAYS
  for overlay in portage_utilities.FindOverlays(both, board, buildroot):
    for filename, projects in portage_utilities.GetWorkonProjectMap(overlay,
                                                                    packages):
      # chromeos-base/power_manager/power_manager-9999
      # cp = chromeos-base/power_manager
      # cpv = chromeos-base/power_manager-9999
      category, pn, p = portage_utilities.SplitEbuildPath(filename)
      cp = '%s/%s' % (category, pn)
      cpv = '%s/%s' % (category, p)

      # Get the time the package finished building. TODO(build): Teach Portage
      # to store the time the package started building and use that here.
      pkg_mtime_file = os.path.join(vdb_path, cpv, 'BUILD_TIME')
      try:
        pkg_mtime = int(osutils.ReadFile(pkg_mtime_file))
      except EnvironmentError as ex:
        if ex.errno != errno.ENOENT:
          raise
        pkg_mtime = 0

      # Get the modificaton time of the ebuild in the overlay.
      src_ebuild_mtime = os.lstat(os.path.join(overlay, filename)).st_mtime

      # Write info into the results dictionary, overwriting any previous
      # values. This ensures that overlays override appropriately.
      results[cp] = WorkonPackageInfo(cp, pkg_mtime, projects, src_ebuild_mtime)

  return results.values()


def ListModifiedWorkonPackages(board, host):
  """List the workon packages that need to be rebuilt.

  Args:
    board: The board to look at. If host is True, this should be set to None.
    host: Whether to look at workon packages for the host.
  """
  packages = ListWorkonPackagesInfo(board, host)
  if packages:
    projects = []
    for info in packages:
      projects.extend(info.projects)
    mtimes = WorkonProjectsMonitor(projects).GetProjectModificationTimes()
    for info in packages:
      mtime = int(max([mtimes.get(p, 0) for p in info.projects] +
                      [info.src_ebuild_mtime]))
      if mtime >= info.pkg_mtime:
        yield info.cp


def _ParseArguments(argv):
  parser = optparse.OptionParser(usage='USAGE: %prog [options]')

  parser.add_option('--board', default=None,
                    dest='board',
                    help='Board name')
  parser.add_option('--host', default=False,
                    dest='host', action='store_true',
                    help='Look at host packages instead of board packages')

  flags, remaining_arguments = parser.parse_args(argv)
  if not flags.board and not flags.host:
    parser.print_help()
    cros_build_lib.Die('--board or --host is required')
  if flags.board is not None and flags.host:
    parser.print_help()
    cros_build_lib.Die('--board and --host are mutually exclusive')
  if remaining_arguments:
    parser.print_help()
    cros_build_lib.Die('Invalid arguments')
  return flags


def main(argv):
  logging.getLogger().setLevel(logging.INFO)
  flags = _ParseArguments(argv)
  modified = ListModifiedWorkonPackages(flags.board, flags.host)
  print ' '.join(sorted(modified))
