#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to upload host prebuilts as well as board BINHOSTS.

Prebuilts are uploaded using gsutil to Google Storage. After these prebuilts
are successfully uploaded, a file is updated with the proper BINHOST version.

To read more about prebuilts/binhost binary packages please refer to:
http://goto/chromeos-prebuilts

Example of uploading prebuilt amd64 host files to Google Storage:
upload_prebuilts -p /b/cbuild/build -s -u gs://chromeos-prebuilt

Example of uploading x86-dogfood binhosts to Google Storage:
upload_prebuilts -b x86-dogfood -p /b/cbuild/build/ -u gs://chromeos-prebuilt -g

"""

import datetime
import multiprocessing
import optparse
import os
import sys

from chromite.buildbot import constants
from chromite.buildbot import portage_utilities
from chromite.lib import binpkg
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import toolchain

# How many times to retry uploads.
_RETRIES = 10

# Multiplier for how long to sleep (in seconds) between retries; will delay
# (1*sleep) the first time, then (2*sleep), continuing via attempt * sleep.
_SLEEP_TIME = 60

_HOST_PACKAGES_PATH = 'chroot/var/lib/portage/pkgs'
_CATEGORIES_PATH = 'chroot/etc/portage/categories'
_PYM_PATH = 'chroot/usr/lib/portage/pym'
_HOST_ARCH = 'amd64'
_BOARD_PATH = 'chroot/build/%(board)s'
_REL_BOARD_PATH = 'board/%(target)s/%(version)s'
_REL_HOST_PATH = 'host/%(host_arch)s/%(target)s/%(version)s'
# Private overlays to look at for builds to filter
# relative to build path
_PRIVATE_OVERLAY_DIR = 'src/private-overlays'
_GOOGLESTORAGE_ACL_FILE = 'googlestorage_acl.xml'
_BINHOST_BASE_URL = gs.PUBLIC_BASE_HTTPS_URL + 'chromeos-prebuilt'
_PREBUILT_BASE_DIR = 'src/third_party/chromiumos-overlay/chromeos/config/'
# Created in the event of new host targets becoming available
_PREBUILT_MAKE_CONF = {'amd64': os.path.join(_PREBUILT_BASE_DIR,
                                             'make.conf.amd64-host')}
_BINHOST_CONF_DIR = 'src/third_party/chromiumos-overlay/chromeos/binhost'


class BuildTarget(object):
  """A board/variant/profile tuple."""

  def __init__(self, board_variant, profile=None):
    self.board_variant = board_variant
    self.board, _, self.variant = board_variant.partition('_')
    self.profile = profile

  def __str__(self):
    if self.profile:
      return '%s_%s' % (self.board_variant, self.profile)
    else:
      return self.board_variant

  def __eq__(self, other):
    return str(other) == str(self)

  def __hash__(self):
    return hash(str(self))


def UpdateLocalFile(filename, value, key='PORTAGE_BINHOST'):
  """Update the key in file with the value passed.
  File format:
    key="value"
  Note quotes are added automatically

  Args:
    filename: Name of file to modify.
    value: Value to write with the key.
    key: The variable key to update. (Default: PORTAGE_BINHOST)
  """
  if os.path.exists(filename):
    file_fh = open(filename)
  else:
    file_fh = open(filename, 'w+')
  file_lines = []
  found = False
  keyval_str = '%(key)s=%(value)s'
  for line in file_fh:
    # Strip newlines from end of line. We already add newlines below.
    line = line.rstrip("\n")

    if len(line.split('=')) != 2:
      # Skip any line that doesn't fit key=val.
      file_lines.append(line)
      continue

    file_var, file_val = line.split('=')
    if file_var == key:
      found = True
      print 'Updating %s=%s to %s="%s"' % (file_var, file_val, key, value)
      value = '"%s"' % value
      file_lines.append(keyval_str % {'key': key, 'value': value})
    else:
      file_lines.append(keyval_str % {'key': file_var, 'value': file_val})

  if not found:
    value = '"%s"' % value
    file_lines.append(keyval_str % {'key': key, 'value': value})

  file_fh.close()
  # write out new file
  osutils.WriteFile(filename, '\n'.join(file_lines) + '\n')


def RevGitFile(filename, data, retries=5, dryrun=False):
  """Update and push the git file.

  Args:
    filename: file to modify that is in a git repo already
    data: A dict of key/values to update in |filename|
    retries: The number of times to retry before giving up, default: 5
  """
  prebuilt_branch = 'prebuilt_branch'
  cwd = os.path.abspath(os.path.dirname(filename))
  commit = git.RunGit(cwd, ['rev-parse', 'HEAD']).output.rstrip()
  description = '%s: updating %s' % (os.path.basename(filename),
                                     ', '.join(data.keys()))
  # UpdateLocalFile will print out the keys/values for us.
  print 'Revving git file %s' % filename

  try:
    git.CreatePushBranch(prebuilt_branch, cwd)
    for key, value in data.iteritems():
      UpdateLocalFile(filename, value, key)
    git.RunGit(cwd, ['add', filename])
    git.RunGit(cwd, ['commit', '-m', description])
    git.PushWithRetry(prebuilt_branch, cwd, dryrun=dryrun, retries=retries)
  finally:
    cros_build_lib.RunCommand(['git', 'checkout', commit], cwd=cwd)


def GetVersion():
  """Get the version to put in LATEST and update the git version with."""
  return datetime.datetime.now().strftime('%Y.%m.%d.%H%M%S')


def _GsUpload(local_file, remote_file, acl):
  """Upload to GS bucket.

  Args:
    args: a tuple of three arguments that contains local_file, remote_file, and
          the acl used for uploading the file.

  Returns:
    Return the arg tuple of two if the upload failed
  """
  CANNED_ACLS = ['public-read', 'private', 'bucket-owner-read',
                 'authenticated-read', 'bucket-owner-full-control',
                 'public-read-write']
  if acl in CANNED_ACLS:
    cmd = [gs.GSUTIL_BIN, 'cp', '-a', acl, local_file, remote_file]
    acl_cmd = None
  else:
    # For private uploads we assume that the overlay board is set up properly
    # and a googlestore_acl.xml is present. Otherwise, this script errors.
    cmd = [gs.GSUTIL_BIN, 'cp', '-a', 'private', local_file, remote_file]
    acl_cmd = [gs.GSUTIL_BIN, 'setacl', acl, remote_file]

  cros_build_lib.RunCommandWithRetries(_RETRIES, cmd, print_cmd=True,
                                       sleep=_SLEEP_TIME,
                                       redirect_stdout=True,
                                       redirect_stderr=True)
  if acl_cmd:
    # Apply the passed in ACL xml file to the uploaded object.
    cros_build_lib.RunCommandWithRetries(_RETRIES, acl_cmd, print_cmd=False,
                                         sleep=_SLEEP_TIME)


def RemoteUpload(acl, files, pool=10):
  """Upload to google storage.

  Create a pool of process and call _GsUpload with the proper arguments.

  Args:
    acl: The canned acl used for uploading. acl can be one of: "public-read",
         "public-read-write", "authenticated-read", "bucket-owner-read",
         "bucket-owner-full-control", or "private".
    files: dictionary with keys to local files and values to remote path.
    pool: integer of maximum proesses to have at the same time.

  Returns:
    Return a set of tuple arguments of the failed uploads
  """
  tasks = [[key, value, acl] for key, value in files.iteritems()]
  parallel.RunTasksInProcessPool(_GsUpload, tasks, pool)


def GenerateUploadDict(base_local_path, base_remote_path, pkgs):
  """Build a dictionary of local remote file key pairs to upload.

  Args:
    base_local_path: The base path to the files on the local hard drive.
    remote_path: The base path to the remote paths.
    pkgs: The packages to upload.

  Returns:
    Returns a dictionary of local_path/remote_path pairs
  """
  upload_files = {}
  for pkg in pkgs:
    suffix = pkg['CPV'] + '.tbz2'
    local_path = os.path.join(base_local_path, suffix)
    assert os.path.exists(local_path)
    remote_path = '%s/%s' % (base_remote_path.rstrip('/'), suffix)
    upload_files[local_path] = remote_path

  return upload_files

def GetBoardOverlay(build_path, target):
  """Get the path to the board variant.
   Args:
     build_path: The path to the root of the build directory
     target: The target board as a BuildTarget object.
   Returns:
     The last overlay configured for the given board as a string.
  """
  board = target.board_variant
  overlays = portage_utilities.FindOverlays(constants.BOTH_OVERLAYS, board,
                                            buildroot=build_path)
  # We only care about the last entry.
  return overlays[-1]


def DeterminePrebuiltConfFile(build_path, target):
  """Determine the prebuilt.conf file that needs to be updated for prebuilts.

    Args:
      build_path: The path to the root of the build directory
      target: String representation of the board. This includes host and board
        targets

    Returns
      A string path to a prebuilt.conf file to be updated.
  """
  if _HOST_ARCH == target:
    # We are host.
    # Without more examples of hosts this is a kludge for now.
    # TODO(Scottz): as new host targets come online expand this to
    # work more like boards.
    make_path = _PREBUILT_MAKE_CONF[target]
  else:
    # We are a board
    board = GetBoardOverlay(build_path, target)
    make_path = os.path.join(board, 'prebuilt.conf')

  return make_path


def UpdateBinhostConfFile(path, key, value):
  """Update binhost config file file with key=value.

  Args:
    path: Filename to update.
    key: Key to update.
    value: New value for key.
  """
  cwd = os.path.dirname(os.path.abspath(path))
  filename = os.path.basename(path)
  osutils.SafeMakedirs(cwd)
  osutils.WriteFile(path, '', mode='a')
  UpdateLocalFile(path, value, key)
  cros_build_lib.RunCommand(['git', 'add', filename], cwd=cwd)
  description = '%s: updating %s' % (os.path.basename(filename), key)
  cros_build_lib.RunCommand(['git', 'commit', '-m', description], cwd=cwd)


def _GrabAllRemotePackageIndexes(binhost_urls):
  """Grab all of the packages files associated with a list of binhost_urls.

  Args:
    binhost_urls: The URLs for the directories containing the Packages files we
                  want to grab.

  Returns:
    A list of PackageIndex objects.
  """
  pkg_indexes = []
  for url in binhost_urls:
    pkg_index = binpkg.GrabRemotePackageIndex(url)
    if pkg_index:
      pkg_indexes.append(pkg_index)
  return pkg_indexes



class PrebuiltUploader(object):
  """Synchronize host and board prebuilts."""

  def __init__(self, upload_location, acl, binhost_base_url,
               pkg_indexes, build_path, packages, skip_upload,
               binhost_conf_dir, debug, target, slave_targets):
    """Constructor for prebuilt uploader object.

    This object can upload host or prebuilt files to Google Storage.

    Args:
      upload_location: The upload location.
      acl: The canned acl used for uploading to Google Storage. acl can be one
           of: "public-read", "public-read-write", "authenticated-read",
           "bucket-owner-read", "bucket-owner-full-control", or "private". If
           we are not uploading to Google Storage, this parameter is unused.
      binhost_base_url: The URL used for downloading the prebuilts.
      pkg_indexes: Old uploaded prebuilts to compare against. Instead of
          uploading duplicate files, we just link to the old files.
      build_path: The path to the directory containing the chroot.
      packages: Packages to upload.
      skip_upload: Don't actually upload the tarballs.
      binhost_conf_dir: Directory where to store binhost.conf files.
      debug: Don't push or upload prebuilts.
      target: BuildTarget managed by this builder.
      slave_targets: List of BuildTargets managed by slave builders.
    """
    self._upload_location = upload_location
    self._acl = acl
    self._binhost_base_url = binhost_base_url
    self._pkg_indexes = pkg_indexes
    self._build_path = build_path
    self._packages = set(packages)
    self._found_packages = set()
    self._skip_upload = skip_upload
    self._binhost_conf_dir = binhost_conf_dir
    self._debug = debug
    self._target = target
    self._slave_targets = slave_targets

  def _ShouldFilterPackage(self, pkg):
    if not self._packages:
      return False
    pym_path = os.path.abspath(os.path.join(self._build_path, _PYM_PATH))
    sys.path.insert(0, pym_path)
    import portage.versions
    cat, pkgname = portage.versions.catpkgsplit(pkg['CPV'])[0:2]
    cp = '%s/%s' % (cat, pkgname)
    self._found_packages.add(cp)
    return pkgname not in self._packages and cp not in self._packages

  def _UploadPrebuilt(self, package_path, url_suffix):
    """Upload host or board prebuilt files to Google Storage space.

    Args:
      package_path: The path to the packages dir.
      url_suffix: The remote subdirectory where we should upload the packages.

    """

    # Process Packages file, removing duplicates and filtered packages.
    pkg_index = binpkg.GrabLocalPackageIndex(package_path)
    pkg_index.SetUploadLocation(self._binhost_base_url, url_suffix)
    pkg_index.RemoveFilteredPackages(self._ShouldFilterPackage)
    uploads = pkg_index.ResolveDuplicateUploads(self._pkg_indexes)
    unmatched_pkgs = self._packages - self._found_packages
    if unmatched_pkgs:
      cros_build_lib.Warning('unable to match packages: %r' % unmatched_pkgs)

    # Write Packages file.
    tmp_packages_file = pkg_index.WriteToNamedTemporaryFile()

    remote_location = '%s/%s' % (self._upload_location.rstrip('/'), url_suffix)
    assert remote_location.startswith('gs://')

    # Build list of files to upload.
    upload_files = GenerateUploadDict(package_path, remote_location, uploads)
    remote_file = '%s/Packages' % remote_location.rstrip('/')
    upload_files[tmp_packages_file.name] = remote_file

    RemoteUpload(self._acl, upload_files)

  def _UploadSdkTarball(self, board_path, url_suffix, version, prepackaged,
                        toolchain_tarballs, toolchain_upload_path):
    """Upload a tarball of the sdk at the specified path to Google Storage.

    Args:
      board_path: The path to the board dir.
      url_suffix: The remote subdirectory where we should upload the packages.
      version: The version of the board.
      prepackaged: If given, a tarball that has been packaged outside of this
                   script and should be used.
      toolchain_tarballs: List of toolchain tarballs to upload.
      toolchain_upload_path: Path under the bucket to place toolchain tarballs.
    """
    remote_location = '%s/%s' % (self._upload_location.rstrip('/'), url_suffix)
    assert remote_location.startswith('gs://')
    boardname = os.path.basename(board_path.rstrip('/'))
    # We do not upload non SDK board tarballs,
    assert boardname == constants.CHROOT_BUILDER_BOARD
    assert prepackaged is not None

    version_str = version[len('chroot-'):]
    remote_tarfile = toolchain.GetSdkURL(
        for_gsutil=True, suburl='cros-sdk-%s.tar.xz' % (version_str,))
    # For SDK, also upload the manifest which is guaranteed to exist
    # by the builderstage.
    _GsUpload(prepackaged + '.Manifest', remote_tarfile + '.Manifest',
              self._acl)
    _GsUpload(prepackaged, remote_tarfile, self._acl)

    # Post the toolchain tarballs too.
    for tarball in toolchain_tarballs:
      target, local_path = tarball.split(':')
      suburl = toolchain_upload_path % {'target': target}
      remote_path = toolchain.GetSdkURL(for_gsutil=True, suburl=suburl)
      _GsUpload(local_path, remote_path, self._acl)

    # Finally, also update the pointer to the latest SDK on which polling
    # scripts rely.
    with osutils.TempDir() as tmpdir:
      pointerfile = os.path.join(tmpdir, 'cros-sdk-latest.conf')
      remote_pointerfile = toolchain.GetSdkURL(for_gsutil=True,
                                               suburl='cros-sdk-latest.conf')
      osutils.WriteFile(pointerfile, 'LATEST_SDK="%s"' % version_str)
      _GsUpload(pointerfile, remote_pointerfile, self._acl)

  def _GetTargets(self):
    """Retuns the list of targets to use."""
    targets = self._slave_targets[:]
    if self._target:
      targets.append(self._target)

    return targets

  def SyncHostPrebuilts(self, version, key, git_sync, sync_binhost_conf):
    """Synchronize host prebuilt files.

    This function will sync both the standard host packages, plus the host
    packages associated with all targets that have been "setup" with the
    current host's chroot. For instance, if this host has been used to build
    x86-generic, it will sync the host packages associated with
    'i686-pc-linux-gnu'. If this host has also been used to build arm-generic,
    it will also sync the host packages associated with
    'armv7a-cros-linux-gnueabi'.

    Args:
      version: A unique string, intended to be included in the upload path,
          which identifies the version number of the uploaded prebuilts.
      key: The variable key to update in the git file.
      git_sync: If set, update make.conf of target to reference the latest
          prebuilt packages generated here.
      sync_binhost_conf: If set, update binhost config file in
          chromiumos-overlay for the host.
    """
    # Slave boards are listed before the master board so that the master board
    # takes priority (i.e. x86-generic preflight host prebuilts takes priority
    # over preflight host prebuilts from other builders.)
    binhost_urls = []
    for target in self._GetTargets():
      url_suffix = _REL_HOST_PATH % {'version': version,
                                     'host_arch': _HOST_ARCH,
                                     'target': target}
      packages_url_suffix = '%s/packages' % url_suffix.rstrip('/')

      if self._target == target and not self._skip_upload and not self._debug:
        # Upload prebuilts.
        package_path = os.path.join(self._build_path, _HOST_PACKAGES_PATH)
        self._UploadPrebuilt(package_path, packages_url_suffix)

      # Record URL where prebuilts were uploaded.
      binhost_urls.append('%s/%s/' % (self._binhost_base_url.rstrip('/'),
                                      packages_url_suffix.rstrip('/')))

    binhost = ' '.join(binhost_urls)
    if git_sync:
      git_file = os.path.join(self._build_path,
          _PREBUILT_MAKE_CONF[_HOST_ARCH])
      RevGitFile(git_file, {key: binhost}, dryrun=self._debug)
    if sync_binhost_conf:
      binhost_conf = os.path.join(self._build_path, self._binhost_conf_dir,
          'host', '%s-%s.conf' % (_HOST_ARCH, key))
      UpdateBinhostConfFile(binhost_conf, key, binhost)

  def SyncBoardPrebuilts(self, version, key, git_sync, sync_binhost_conf,
                         upload_board_tarball, prepackaged_board,
                         toolchain_tarballs, toolchain_upload_path):
    """Synchronize board prebuilt files.

    Args:
      version: A unique string, intended to be included in the upload path,
          which identifies the version number of the uploaded prebuilts.
      key: The variable key to update in the git file.
      git_sync: If set, update make.conf of target to reference the latest
          prebuilt packages generated here.
      sync_binhost_conf: If set, update binhost config file in
          chromiumos-overlay for the current board.
      upload_board_tarball: Include a tarball of the board in our upload.
      prepackaged_board: A tarball of the board built outside of this script.
      toolchain_tarballs: A list of toolchain tarballs to upload.
      toolchain_upload_path: Path under the bucket to place toolchain tarballs.
    """
    for target in self._GetTargets():
      board_path = os.path.join(self._build_path,
                                _BOARD_PATH % {'board': target.board_variant})
      package_path = os.path.join(board_path, 'packages')
      url_suffix = _REL_BOARD_PATH % {'target': target, 'version': version}
      packages_url_suffix = '%s/packages' % url_suffix.rstrip('/')

      if self._target == target and not self._skip_upload and not self._debug:
        version_str = version[len('chroot-'):]

        # Upload board tarballs in the background.
        if upload_board_tarball:
          if toolchain_upload_path:
            toolchain_upload_path %= {'version': version_str}
          tar_process = multiprocessing.Process(
              target=self._UploadSdkTarball,
              args=(board_path, url_suffix, version, prepackaged_board,
                    toolchain_tarballs, toolchain_upload_path))
          tar_process.start()

        # Upload prebuilts.
        self._UploadPrebuilt(package_path, packages_url_suffix)

        # Make sure we finished uploading the board tarballs.
        if upload_board_tarball:
          tar_process.join()
          assert tar_process.exitcode == 0
          # TODO(zbehan): This should be done cleaner.
          if target.board == constants.CHROOT_BUILDER_BOARD:
            sdk_conf = os.path.join(self._build_path, self._binhost_conf_dir,
                                    'host/sdk_version.conf')
            sdk_settings = {
                'SDK_LATEST_VERSION': version_str,
                'TC_PATH': toolchain_upload_path,
            }
            RevGitFile(sdk_conf, sdk_settings, dryrun=self._debug)

      # Record URL where prebuilts were uploaded.
      url_value = '%s/%s/' % (self._binhost_base_url.rstrip('/'),
                              packages_url_suffix.rstrip('/'))

      if git_sync:
        git_file = DeterminePrebuiltConfFile(self._build_path, target)
        RevGitFile(git_file, {key: url_value}, dryrun=self._debug)
      if sync_binhost_conf:
        binhost_conf = os.path.join(self._build_path, self._binhost_conf_dir,
            'target', '%s-%s.conf' % (target, key))
        UpdateBinhostConfFile(binhost_conf, key, url_value)


def Usage(parser, msg):
  """Display usage message and parser help then exit with 1."""
  print >> sys.stderr, msg
  parser.print_help()
  sys.exit(1)


def _AddSlaveBoard(_option, _opt_str, value, parser):
  """Callback that adds a slave board to the list of slave targets."""
  parser.values.slave_targets.append(BuildTarget(value))


def _AddSlaveProfile(_option, _opt_str, value, parser):
  """Callback that adds a slave profile to the list of slave targets."""
  if not parser.values.slave_targets:
    Usage(parser, 'Must specify --slave-board before --slave-profile')
  if parser.values.slave_targets[-1].profile is not None:
    Usage(parser, 'Cannot specify --slave-profile twice for same board')
  parser.values.slave_targets[-1].profile = value


def ParseOptions():
  """Returns options given by the user and the target specified.

  Returns a tuple containing a parsed options object and BuildTarget.
  target instance is None if no board is specified.
  """
  parser = optparse.OptionParser()
  parser.add_option('-H', '--binhost-base-url', dest='binhost_base_url',
                    default=_BINHOST_BASE_URL,
                    help='Base URL to use for binhost in make.conf updates')
  parser.add_option('', '--previous-binhost-url', action='append',
                    default=[], dest='previous_binhost_url',
                    help='Previous binhost URL')
  parser.add_option('-b', '--board', dest='board', default=None,
                    help='Board type that was built on this machine')
  parser.add_option('-B', '--prepackaged-tarball', dest='prepackaged_tarball',
                    default=None,
                    help='Board tarball prebuilt outside of this script.')
  parser.add_option('--toolchain-tarball', dest='toolchain_tarballs',
                    action='append', default=[],
                    help='Redistributable toolchain tarball.')
  parser.add_option('--toolchain-upload-path', default='',
                    help='Path to place toolchain tarballs in the sdk tree.')
  parser.add_option('', '--profile', dest='profile', default=None,
                    help='Profile that was built on this machine')
  parser.add_option('', '--slave-board', default=[], action='callback',
                    dest='slave_targets', type='string',
                    callback=_AddSlaveBoard,
                    help='Board type that was built on a slave machine. To '
                         'add a profile to this board, use --slave-profile.')
  parser.add_option('', '--slave-profile', action='callback', type='string',
                    callback=_AddSlaveProfile,
                    help='Board profile that was built on a slave machine. '
                         'Applies to previous slave board.')
  parser.add_option('-p', '--build-path', dest='build_path',
                    help='Path to the directory containing the chroot')
  parser.add_option('', '--packages', action='append',
                    default=[], dest='packages',
                    help='Only include the specified packages. '
                         '(Default is to include all packages.)')
  parser.add_option('-s', '--sync-host', dest='sync_host',
                    default=False, action='store_true',
                    help='Sync host prebuilts')
  parser.add_option('-g', '--git-sync', dest='git_sync',
                    default=False, action='store_true',
                    help='Enable git version sync (This commits to a repo.) '
                         'This is used by full builders to commit directly '
                         'to board overlays.')
  parser.add_option('-u', '--upload', dest='upload',
                    default=None,
                    help='Upload location')
  parser.add_option('-V', '--prepend-version', dest='prepend_version',
                    default=None,
                    help='Add an identifier to the front of the version')
  parser.add_option('-f', '--filters', dest='filters', action='store_true',
                    default=False,
                    help='Turn on filtering of private ebuild packages')
  parser.add_option('-k', '--key', dest='key',
                    default='PORTAGE_BINHOST',
                    help='Key to update in make.conf / binhost.conf')
  parser.add_option('', '--set-version', dest='set_version',
                    default=None,
                    help='Specify the version string')
  parser.add_option('', '--sync-binhost-conf', dest='sync_binhost_conf',
                    default=False, action='store_true',
                    help='Update binhost.conf in chromiumos-overlay or '
                         'chromeos-overlay. Commit the changes, but don\'t '
                         'push them. This is used for preflight binhosts.')
  parser.add_option('', '--binhost-conf-dir', dest='binhost_conf_dir',
                    default=_BINHOST_CONF_DIR,
                    help='Directory to commit binhost config with '
                         '--sync-binhost-conf.')
  parser.add_option('-P', '--private', dest='private', action='store_true',
                    default=False, help='Mark gs:// uploads as private.')
  parser.add_option('', '--skip-upload', dest='skip_upload',
                    action='store_true', default=False,
                    help='Skip upload step.')
  parser.add_option('', '--upload-board-tarball', dest='upload_board_tarball',
                    action='store_true', default=False,
                    help='Upload board tarball to Google Storage.')
  parser.add_option('', '--debug', dest='debug',
                    action='store_true', default=False,
                    help='Don\'t push or upload prebuilts.')

  options, args = parser.parse_args()
  if not options.build_path:
    Usage(parser, 'Error: you need provide a chroot path')
  if not options.upload and not options.skip_upload:
    Usage(parser, 'Error: you need to provide an upload location using -u')
  if not options.set_version and options.skip_upload:
    Usage(parser, 'Error: If you are using --skip-upload, you must specify a '
                  'version number using --set-version.')
  if args:
    Usage(parser, 'Error: invalid arguments passed to upload_prebuilts: '
                  '%r' % args)

  target = None
  if options.board:
    target = BuildTarget(options.board, options.profile)

  if target in options.slave_targets:
    Usage(parser, 'Error: --board/--profile must not also be a slave target.')

  if len(set(options.slave_targets)) != len(options.slave_targets):
    Usage(parser, 'Error: --slave-boards must not have duplicates.')

  if options.slave_targets and options.git_sync:
    Usage(parser, 'Error: --slave-boards is not compatible with --git-sync')

  if (options.upload_board_tarball and options.skip_upload and
      options.board == 'amd64-host'):
    Usage(parser, 'Error: --skip-upload is not compatible with '
                  '--upload-board-tarball and --board=amd64-host')

  if (options.upload_board_tarball and not options.skip_upload and
      not options.upload.startswith('gs://')):
    Usage(parser, 'Error: --upload-board-tarball only works with gs:// URLs.\n'
                  '--upload must be a gs:// URL.')

  if options.upload_board_tarball and options.prepackaged_tarball is None:
    Usage(parser, 'Error: --upload-board-tarball requires '
                  '--prepackaged-tarball.')

  if options.private:
    if options.sync_host:
      Usage(parser, 'Error: --private and --sync-host/-s cannot be specified '
                    'together, we do not support private host prebuilts')

    if not options.upload or not options.upload.startswith('gs://'):
      Usage(parser, 'Error: --private is only valid for gs:// URLs.\n'
                    '--upload must be a gs:// URL.')

    if options.binhost_base_url != _BINHOST_BASE_URL:
      Usage(parser, 'Error: when using --private the --binhost-base-url '
                    'is automatically derived.')

  return options, target

def main(_argv):
  # Set umask to a sane value so that files created as root are readable.
  os.umask(022)

  options, target = ParseOptions()

  # Calculate a list of Packages index files to compare against. Whenever we
  # upload a package, we check to make sure it's not already stored in one of
  # the packages files we uploaded. This list of packages files might contain
  # both board and host packages.
  pkg_indexes = _GrabAllRemotePackageIndexes(options.previous_binhost_url)

  if options.set_version:
    version = options.set_version
  else:
    version = GetVersion()
  if options.prepend_version:
    version = '%s-%s' % (options.prepend_version, version)

  acl = 'public-read'
  binhost_base_url = options.binhost_base_url

  if target and options.private:
    binhost_base_url = options.upload
    board_path = GetBoardOverlay(options.build_path, target)
    acl = os.path.join(board_path, _GOOGLESTORAGE_ACL_FILE)

  uploader = PrebuiltUploader(options.upload, acl, binhost_base_url,
                              pkg_indexes, options.build_path,
                              options.packages, options.skip_upload,
                              options.binhost_conf_dir, options.debug,
                              target, options.slave_targets)

  if options.sync_host:
    uploader.SyncHostPrebuilts(version, options.key, options.git_sync,
                               options.sync_binhost_conf)

  if options.board or options.slave_targets:
    uploader.SyncBoardPrebuilts(version, options.key, options.git_sync,
                                options.sync_binhost_conf,
                                options.upload_board_tarball,
                                options.prepackaged_tarball,
                                options.toolchain_tarballs,
                                options.toolchain_upload_path)
