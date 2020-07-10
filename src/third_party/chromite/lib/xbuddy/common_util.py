# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper class for interacting with the Dev Server."""

from __future__ import print_function

import ast
import base64
import distutils.version  # pylint: disable=no-name-in-module,import-error
import hashlib
import os
import re
import shutil
import tempfile
import threading

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib.xbuddy import cherrypy_log_util


# Module-local log function.
def _Log(message, *args):
  return cherrypy_log_util.LogWithTag('UTIL', message, *args)


_HASH_BLOCK_SIZE = 8192


class CommonUtilError(Exception):
  """Exception classes used by this module."""


def MkDirP(directory):
  """Thread-safely create a directory like mkdir -p.

  If the directory already exists, call chown on the directory and its subfiles
  recursively with current user and group to make sure current process has full
  access to the directory.
  """
  if os.path.isdir(directory):
    # Fix permissions and ownership of the directory and its subfiles by
    # calling chown recursively with current user and group.
    try:
      osutils.Chown(directory, user=os.getuid(), group=os.getgid(),
                    recursive=True)
    except cros_build_lib.RunCommandError as e:
      _Log('Could not chown: %s', e)
  else:
    osutils.SafeMakedirs(directory)


def GetLatestBuildVersion(static_dir, target, milestone=None):
  """Retrieves the latest build version for a given board.

  Searches the static_dir for builds for target, and returns the highest
  version number currently available locally.

  Args:
    static_dir: Directory where builds are served from.
    target: The build target, typically a combination of the board and the
        type of build e.g. x86-mario-release.
    milestone: For latest build set to None, for builds only in a specific
        milestone set to a str of format Rxx (e.g. R16). Default: None.

  Returns:
    If latest found, a full build string is returned e.g. R17-1234.0.0-a1-b983.
    If no latest is found for some reason or another a '' string is returned.

  Raises:
    CommonUtilError: If for some reason the latest build cannot be
        deteremined, this could be due to the dir not existing or no builds
        being present after filtering on milestone.
  """
  target_path = os.path.join(static_dir, target)
  if not os.path.isdir(target_path):
    raise CommonUtilError('Cannot find path %s' % target_path)

  # pylint: disable=no-member
  builds = [distutils.version.LooseVersion(build) for build in
            os.listdir(target_path) if not build.endswith('.exception')]

  if milestone and builds:
    # Check if milestone Rxx is in the string representation of the build.
    builds = [build for build in builds if milestone.upper() in str(build)]

  if not builds:
    raise CommonUtilError('Could not determine build for %s' % target)

  return str(max(builds))


def PathInDir(directory, path):
  """Returns True if the path is in directory.

  Args:
    directory: Directory where the path should be in.
    path: Path to check.

  Returns:
    True if path is in static_dir, False otherwise
  """
  directory = os.path.realpath(directory)
  path = os.path.realpath(path)
  return path.startswith(directory) and len(path) != len(directory)


def GetControlFile(static_dir, build, control_path):
  """Attempts to pull the requested control file from the Dev Server.

  Args:
    static_dir: Directory where builds are served from.
    build: Fully qualified build string; e.g. R17-1234.0.0-a1-b983.
    control_path: Path to control file on Dev Server relative to Autotest root.

  Returns:
    Content of the requested control file.

  Raises:
    CommonUtilError: If lock can't be acquired.
  """
  # Be forgiving if the user passes in the control_path with a leading /
  control_path = control_path.lstrip('/')
  control_path = os.path.join(static_dir, build, 'autotest',
                              control_path)
  if not PathInDir(static_dir, control_path):
    raise CommonUtilError('Invalid control file "%s".' % control_path)

  if not os.path.exists(control_path):
    # TODO(scottz): Come up with some sort of error mechanism.
    # crosbug.com/25040
    return 'Unknown control path %s' % control_path

  with open(control_path, 'r') as control_file:
    return control_file.read()


def GetControlFileListForSuite(static_dir, build, suite_name):
  """List all control files for a specified build, for the given suite.

  If the specified suite_name isn't found in the suite to control file
  map, this method will return all control files for the build by calling
  GetControlFileList.

  Args:
    static_dir: Directory where builds are served from.
    build: Fully qualified build string; e.g. R17-1234.0.0-a1-b983.
    suite_name: Name of the suite for which we require control files.

  Returns:
    String of each control file separated by a newline.

  Raises:
    CommonUtilError: If the suite_to_control_file_map isn't found in
        the specified build's staged directory.
  """
  suite_to_control_map = os.path.join(static_dir, build,
                                      'autotest', 'test_suites',
                                      'suite_to_control_file_map')

  if not PathInDir(static_dir, suite_to_control_map):
    raise CommonUtilError('suite_to_control_map not in "%s".' %
                          suite_to_control_map)

  if not os.path.exists(suite_to_control_map):
    raise CommonUtilError('Could not find this file. '
                          'Is it staged? %s' % suite_to_control_map)

  with open(suite_to_control_map, 'r') as fd:
    try:
      return '\n'.join(ast.literal_eval(fd.read())[suite_name])
    except KeyError:
      return GetControlFileList(static_dir, build)


def GetControlFileList(static_dir, build):
  """List all control|control. files in the specified board/build path.

  Args:
    static_dir: Directory where builds are served from.
    build: Fully qualified build string; e.g. R17-1234.0.0-a1-b983.

  Returns:
    String of each file separated by a newline.

  Raises:
    CommonUtilError: If path is outside of sandbox.
  """
  autotest_dir = os.path.join(static_dir, build, 'autotest/')
  if not PathInDir(static_dir, autotest_dir):
    raise CommonUtilError('Autotest dir not in sandbox "%s".' % autotest_dir)

  control_files = set()
  if not os.path.exists(autotest_dir):
    raise CommonUtilError('Could not find this directory.'
                          'Is it staged? %s' % autotest_dir)

  for entry in os.walk(autotest_dir):
    dir_path, _, files = entry
    for file_entry in files:
      if file_entry.startswith('control.') or file_entry == 'control':
        control_files.add(os.path.join(dir_path,
                                       file_entry).replace(autotest_dir, ''))

  return '\n'.join(control_files)


# Hashlib is strange and doesn't actually define these in a sane way that
# pylint can find them. Disable checks for them.
# pylint: disable=E1101,W0106
def GetFileHashes(file_path, do_sha256=False, do_md5=False):
  """Computes and returns a list of requested hashes.

  Args:
    file_path: path to file to be hashed
    do_sha256: whether or not to compute a SHA256 hash
    do_md5: whether or not to compute a MD5 hash

  Returns:
    A dictionary containing binary hash values, keyed by 'sha1', 'sha256' and
    'md5', respectively.
  """
  hashes = {}
  if any((do_sha256, do_md5)):
    # Initialize hashers.
    hasher_sha256 = hashlib.sha256() if do_sha256 else None
    hasher_md5 = hashlib.md5() if do_md5 else None

    # Read blocks from file, update hashes.
    with open(file_path, 'rb') as fd:
      while True:
        block = fd.read(_HASH_BLOCK_SIZE)
        if not block:
          break
        hasher_sha256 and hasher_sha256.update(block)
        hasher_md5 and hasher_md5.update(block)

    # Update return values.
    if hasher_sha256:
      hashes['sha256'] = hasher_sha256.digest()
    if hasher_md5:
      hashes['md5'] = hasher_md5.digest()

  return hashes


def GetFileSha256(file_path):
  """Returns the SHA256 checksum of the file given (base64 encoded)."""
  return base64.b64encode(GetFileHashes(file_path, do_sha256=True)['sha256'])


def CopyFile(source, dest):
  """Copies a file from |source| to |dest|."""
  _Log('Copy File %s -> %s' % (source, dest))
  shutil.copy(source, dest)


def SymlinkFile(target, link):
  """Atomically creates or replaces the symlink |link| pointing to |target|.

  If the specified |link| file already exists it is replaced with the new link
  atomically.
  """
  if not os.path.exists(target):
    _Log('Could not find target for symlink: %s', target)
    return

  _Log('Creating symlink: %s --> %s', link, target)

  # Use the created link_base file to prevent other calls to SymlinkFile() to
  # pick the same link_base temp file, thanks to mkstemp().
  with tempfile.NamedTemporaryFile(prefix=os.path.basename(link)) as link_fd:
    link_base = link_fd.name

    # Use the unique link_base filename to create a symlink, but on the same
    # directory as the required |link| to ensure the created symlink is in the
    # same file system as |link|.
    link_name = os.path.join(os.path.dirname(link),
                             os.path.basename(link_base) + '-link')

    # Create the symlink and then rename it to the final position. This ensures
    # the symlink creation is atomic.
    os.symlink(target, link_name)
    os.rename(link_name, link)


class LockDict(object):
  """A dictionary of locks.

  This class provides a thread-safe store of threading.Lock objects, which can
  be used to regulate access to any set of hashable resources.  Usage:

    foo_lock_dict = LockDict()
    ...
    with foo_lock_dict.lock('bar'):
      # Critical section for 'bar'
  """
  def __init__(self):
    self._lock = self._new_lock()
    self._dict = {}

  @staticmethod
  def _new_lock():
    return threading.Lock()

  def lock(self, key):
    with self._lock:
      lock = self._dict.get(key)
      if not lock:
        lock = self._new_lock()
        self._dict[key] = lock
      return lock


# TODO(ahassani, achuith, vapier): Move to cros_build_lib, which has a
# CreateTarball utility function.
def ExtractTarball(tarball_path, install_path, files_to_extract=None,
                   excluded_files=None, return_extracted_files=False):
  """Extracts a tarball using tar.

  Detects whether the tarball is compressed or not based on the file
  extension and extracts the tarball into the install_path.

  Args:
    tarball_path: Path to the tarball to extract.
    install_path: Path to extract the tarball to.
    files_to_extract: String of specific files in the tarball to extract.
    excluded_files: String of files to not extract.
    return_extracted_files: whether or not the caller expects the list of
      files extracted; if False, returns an empty list.

  Returns:
    List of absolute paths of the files extracted (possibly empty).
  """
  # Deal with exclusions.
  # Add 'm' for not extracting file's modified time. All extracted files are
  # marked with current system time.
  cmd = ['tar', 'xf', tarball_path, '--directory', install_path]

  # If caller requires the list of extracted files, get verbose.
  if return_extracted_files:
    cmd += ['--verbose']

  # Determine how to decompress.
  tarball = os.path.basename(tarball_path)
  if tarball.endswith('.tgz') or tarball.endswith('.tar.gz'):
    cmd.append('--gzip')

  if excluded_files:
    for exclude in excluded_files:
      cmd.extend(['--exclude', exclude])

  if files_to_extract:
    cmd.extend(files_to_extract)

  try:
    result = cros_build_lib.run(cmd, capture_output=True, encoding='utf-8')
    if result.stderr:
      _Log('Error happened while in extracting tarball: %s',
           result.stderr.rstrip())

    if return_extracted_files:
      return [os.path.join(install_path, filename)
              for filename in result.stdout.splitlines()
              if not filename.endswith('/')]
    return []
  except cros_build_lib.RunCommandError as e:
    raise CommonUtilError(
        'An error occurred when attempting to untar %s:\n%s' %
        (tarball_path, e))


def IsInsideChroot():
  """Returns True if we are inside chroot."""
  return os.path.exists('/etc/debian_chroot')


def IsRunningOnMoblab():
  """Returns True if this code is running on a chromiumOS DUT."""
  try:
    return bool(re.search(
        r'[_-]+moblab', osutils.ReadFile(constants.LSB_RELEASE_PATH)))
  except IOError:
    # File doesn't exist.
    return False
