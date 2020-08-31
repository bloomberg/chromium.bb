# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main module for parsing and interpreting XBuddy paths for the devserver."""

from __future__ import print_function

import datetime
import distutils.version  # pylint: disable=import-error,no-name-in-module
import operator
import os
import re
import shutil
import sys
import time
import threading

from six.moves import configparser

from chromite.lib import constants
from chromite.lib import image_lib
from chromite.lib import gs
from chromite.lib import path_util
from chromite.lib.xbuddy import artifact_info
from chromite.lib.xbuddy import build_artifact
from chromite.lib.xbuddy import common_util
from chromite.lib.xbuddy import devserver_constants
from chromite.lib.xbuddy import downloader
from chromite.lib.xbuddy import cherrypy_log_util


# Module-local log function.
def _Log(message, *args):
  return cherrypy_log_util.LogWithTag('XBUDDY', message, *args)

# xBuddy config constants
CONFIG_FILE = 'xbuddy_config.ini'
SHADOW_CONFIG_FILE = 'shadow_xbuddy_config.ini'
PATH_REWRITES = 'PATH_REWRITES'
GENERAL = 'GENERAL'
LOCATION_SUFFIXES = 'LOCATION_SUFFIXES'

# Path for shadow config in chroot.
CHROOT_SHADOW_DIR = '/mnt/host/source/src/platform/dev'

# XBuddy aliases
TEST = 'test'
BASE = 'base'
DEV = 'dev'
FULL = 'full_payload'
DELTA = 'delta_payload'
SIGNED = 'signed'
RECOVERY = 'recovery'
STATEFUL = 'stateful'
AUTOTEST = 'autotest'
FACTORY_SHIM = 'factory_shim'

# Local build constants
ANY = 'ANY'
LATEST = 'latest'
LOCAL = 'local'
REMOTE = 'remote'

# TODO(sosa): Fix a lot of assumptions about these aliases. There is too much
# implicit logic here that's unnecessary. What should be done:
# 1) Collapse Alias logic to one set of aliases for xbuddy (not local/remote).
# 2) Do not use zip when creating these dicts. Better to not rely on ordering.
# 3) Move alias/artifact mapping to a central module rather than having it here.
# 4) Be explicit when things are missing i.e. no dev images in image.zip.

LOCAL_ALIASES = [
    TEST,
    DEV,
    BASE,
    RECOVERY,
    FACTORY_SHIM,
    FULL,
    DELTA,
    STATEFUL,
    ANY,
]

LOCAL_FILE_NAMES = [
    devserver_constants.TEST_IMAGE_FILE,
    devserver_constants.IMAGE_FILE,
    devserver_constants.BASE_IMAGE_FILE,
    devserver_constants.RECOVERY_IMAGE_FILE,
    devserver_constants.FACTORY_SHIM_IMAGE_FILE,
    devserver_constants.UPDATE_FILE,
    devserver_constants.STATEFUL_FILE,
    None, # For ANY.
]

LOCAL_ALIAS_TO_FILENAME = dict(zip(LOCAL_ALIASES, LOCAL_FILE_NAMES))

# Google Storage constants
GS_ALIASES = [
    TEST,
    BASE,
    RECOVERY,
    SIGNED,
    FACTORY_SHIM,
    FULL,
    DELTA,
    STATEFUL,
    AUTOTEST,
]

GS_FILE_NAMES = [
    devserver_constants.TEST_IMAGE_FILE,
    devserver_constants.BASE_IMAGE_FILE,
    devserver_constants.RECOVERY_IMAGE_FILE,
    devserver_constants.SIGNED_IMAGE_FILE,
    devserver_constants.FACTORY_SHIM_IMAGE_FILE,
    devserver_constants.UPDATE_FILE,
    devserver_constants.STATEFUL_FILE,
    devserver_constants.AUTOTEST_DIR,
]

ARTIFACTS = [
    artifact_info.TEST_IMAGE,
    artifact_info.BASE_IMAGE,
    artifact_info.RECOVERY_IMAGE,
    artifact_info.SIGNED_IMAGE,
    artifact_info.FACTORY_SHIM_IMAGE,
    artifact_info.FULL_PAYLOAD,
    artifact_info.DELTA_PAYLOAD,
    artifact_info.STATEFUL_PAYLOAD,
    artifact_info.AUTOTEST,
]

GS_ALIAS_TO_FILENAME = dict(zip(GS_ALIASES, GS_FILE_NAMES))
GS_ALIAS_TO_ARTIFACT = dict(zip(GS_ALIASES, ARTIFACTS))

LATEST_OFFICIAL = 'latest-official'

RELEASE = '-release'


class XBuddyException(Exception):
  """Exception classes used by this module."""


class Timestamp(object):
  """Class to translate build path strings and timestamp filenames."""

  _TIMESTAMP_DELIMITER = 'SLASH'
  XBUDDY_TIMESTAMP_DIR = 'xbuddy_UpdateTimestamps'

  @staticmethod
  def TimestampToBuild(timestamp_filename):
    return timestamp_filename.replace(Timestamp._TIMESTAMP_DELIMITER, '/')

  @staticmethod
  def BuildToTimestamp(build_path):
    return build_path.replace('/', Timestamp._TIMESTAMP_DELIMITER)

  @staticmethod
  def UpdateTimestamp(timestamp_dir, build_id):
    """Update timestamp file of build with build_id."""
    common_util.MkDirP(timestamp_dir)
    _Log('Updating timestamp for %s', build_id)
    time_file = os.path.join(timestamp_dir,
                             Timestamp.BuildToTimestamp(build_id))
    with open(time_file, 'a'):
      os.utime(time_file, None)


class XBuddy(object):
  """Class that manages image retrieval and caching by the devserver.

  Image retrieval by xBuddy path:
    XBuddy accesses images and artifacts that it stores using an xBuddy
    path of the form: board/version/alias
    The primary xbuddy.Get call retrieves the correct artifact or url to where
    the artifacts can be found.

  Image caching:
    Images and other artifacts are stored identically to how they would have
    been if devserver's stage rpc was called and the xBuddy cache replaces
    build versions on a LRU basis. Timestamps are maintained by last accessed
    times of representative files in the a directory in the static serve
    directory (XBUDDY_TIMESTAMP_DIR).

  Private class members:
    _true_values: used for interpreting boolean values
    _staging_thread_count: track download requests
    _timestamp_folder: directory with empty files standing in as timestamps
                        for each image currently cached by xBuddy
  """
  _true_values = ['true', 't', 'yes', 'y']

  # Number of threads that are staging images.
  _staging_thread_count = 0
  # Lock used to lock increasing/decreasing count.
  _staging_thread_count_lock = threading.Lock()

  def __init__(self, manage_builds=False, board=None, version=None,
               images_dir=None, log_screen=True, static_dir=None):
    if not log_screen:
      cherrypy_log_util.UpdateConfig({'log.screen': False})

    self.config = self._ReadConfig()
    self._manage_builds = manage_builds or self._ManageBuilds()
    self._board = board
    self._version = version
    self.static_dir = static_dir
    self._timestamp_folder = os.path.join(self.static_dir,
                                          Timestamp.XBUDDY_TIMESTAMP_DIR)
    self.images_dir = images_dir or os.path.join(constants.SOURCE_ROOT,
                                                 'src/build/images')

    cache_user = 'chronos' if common_util.IsRunningOnMoblab() else None
    self._ctx = gs.GSContext(cache_user=cache_user)

    common_util.MkDirP(self._timestamp_folder)

  @classmethod
  def ParseBoolean(cls, boolean_string):
    """Evaluate a string to a boolean value"""
    if boolean_string:
      return boolean_string.lower() in cls._true_values
    else:
      return False

  def _ReadConfig(self):
    """Read xbuddy config from ini files.

    Reads the base config from xbuddy_config.ini, and then merges in the
    shadow config from shadow_xbuddy_config.ini

    Returns:
      The merged configuration.

    Raises:
      XBuddyException if the config file is missing.
    """
    devserver_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
    config_file = os.path.join(devserver_dir, CONFIG_FILE)
    xbuddy_config = configparser.ConfigParser()
    if os.path.exists(config_file):
      xbuddy_config.read(config_file)
    else:
      # Get the directory of xbuddy.py file.
      file_dir = os.path.dirname(os.path.realpath(__file__))
      # Read the default xbuddy_config.ini from the directory.
      xbuddy_config.read(os.path.join(file_dir, CONFIG_FILE))

    # Read the shadow file if there is one.
    if os.path.isdir(CHROOT_SHADOW_DIR):
      shadow_config_file = os.path.join(CHROOT_SHADOW_DIR, SHADOW_CONFIG_FILE)
    else:
      shadow_config_file = os.path.join(devserver_dir, SHADOW_CONFIG_FILE)

    _Log('Using shadow config file stored at %s', shadow_config_file)
    if os.path.exists(shadow_config_file):
      shadow_xbuddy_config = configparser.ConfigParser()
      shadow_xbuddy_config.read(shadow_config_file)

      # Merge shadow config in.
      sections = shadow_xbuddy_config.sections()
      for s in sections:
        if not xbuddy_config.has_section(s):
          xbuddy_config.add_section(s)
        options = shadow_xbuddy_config.options(s)
        for o in options:
          val = shadow_xbuddy_config.get(s, o)
          xbuddy_config.set(s, o, val)

    return xbuddy_config

  def _ManageBuilds(self):
    """Checks if xBuddy is managing local builds using the current config."""
    try:
      return self.ParseBoolean(self.config.get(GENERAL, 'manage_builds'))
    except configparser.Error:
      return False

  def _Capacity(self):
    """Gets the xbuddy capacity from the current config."""
    try:
      return int(self.config.get(GENERAL, 'capacity'))
    except configparser.Error:
      return 5

  def LookupAlias(self, alias, board=None, version=None):
    """Given the full xbuddy config, look up an alias for path rewrite.

    Args:
      alias: The xbuddy path that could be one of the aliases in the
        rewrite table.
      board: The board to fill in with when paths are rewritten. Can be from
        the update request xml or the default board from devserver. If None,
        defers to the value given during XBuddy initialization.
      version: The version to fill in when rewriting paths. Could be a specific
        version number or a version alias like LATEST. If None, defers to the
        value given during XBuddy initialization, or LATEST.

    Returns:
      A pair (val, suffix) where val is the rewritten path, or the original
      string if no rewrite was found; and suffix is the assigned location
      suffix, or the default suffix if none was found.
    """
    try:
      suffix = self.config.get(LOCATION_SUFFIXES, alias)
    except configparser.Error:
      suffix = RELEASE

    try:
      val = self.config.get(PATH_REWRITES, alias)
    except configparser.Error:
      # No alias lookup found. Return original path.
      val = None

    if not (val and val.strip()):
      val = alias
    else:
      # The found value is not an empty string.
      # Fill in the board and version.
      val = val.replace('BOARD', '%(board)s')
      val = val.replace('VERSION', '%(version)s')
      val = val % {'board': board or self._board,
                   'version': version or self._version or LATEST}

    _Log('Path is %s, location suffix is %s', val, suffix)
    return val, suffix

  @staticmethod
  def _ResolveImageDir(image_dir):
    """Clean up and return the image dir to use.

    Args:
      image_dir: directory in Google Storage to use.

    Returns:
      |image_dir| if |image_dir| is not None. Otherwise, returns
        devserver_constants.GS_IMAGE_DIR
    """
    image_dir = image_dir or devserver_constants.GS_IMAGE_DIR
    # Remove trailing slashes.
    return image_dir.rstrip('/')

  def _LookupOfficial(self, board, suffix, image_dir=None):
    """Check LATEST-master for the version number of interest."""
    _Log('Checking gs for latest %s-%s image', board, suffix)
    image_dir = XBuddy._ResolveImageDir(image_dir)
    latest_addr = (devserver_constants.GS_LATEST_MASTER %
                   {'image_dir': image_dir,
                    'board': board,
                    'suffix': suffix})
    # Full release + version is in the LATEST file.
    version = self._ctx.Cat(latest_addr, encoding='utf-8')

    return devserver_constants.IMAGE_DIR % {'board':board,
                                            'suffix':suffix,
                                            'version':version}

  def _LS(self, path, list_subdirectory=False):
    """Does a directory listing of the given gs path.

    Args:
      path: directory location on google storage to check.
      list_subdirectory: whether to only list subdirectory for |path|.

    Returns:
      A list of paths that matched |path|.
    """
    if list_subdirectory:
      return self._ctx.DoCommand(
          ['ls', '-d', '--', path], stdout=True).output.splitlines()
    else:
      return self._ctx.LS(path)

  def _GetLatestVersionFromGsDir(self, path, list_subdirectory=False,
                                 with_release=True):
    """Returns most recent version number found in a google storage directory.

    This lists out the contents of the given GS bucket or regex to GS buckets,
    and tries to grab the newest version found in the directory names.

    Args:
      path: directory location on google storage to check.
      list_subdirectory: whether to only list subdirectory for |path|.
      with_release: whether versions include a release milestone (e.g. R12).

    Returns:
      The most recent version number found.
    """
    list_result = self._LS(path, list_subdirectory=list_subdirectory)
    dir_names = [os.path.basename(p.rstrip('/')) for p in list_result]
    try:
      versions_re = re.compile(devserver_constants.VERSION_RE if with_release
                               else devserver_constants.VERSION)
      versions = [d for d in dir_names if versions_re.match(d)]
      latest_version = max(versions, key=distutils.version.LooseVersion)
    except ValueError:
      raise gs.GSContextException(
          'Failed to find most recent builds at %s' % path)

    return latest_version

  def _LookupChannel(self, board, suffix, channel='stable',
                     image_dir=None):
    """Check the channel folder for the version number of interest."""
    # Get all names in channel dir. Get 10 highest directories by version.
    _Log("Checking channel '%s' for latest '%s' image", channel, board)
    # Due to historical reasons, gs://chromeos-releases uses
    # daisy-spring as opposed to the board name daisy_spring. Convert
    # he board name for the lookup.
    channel_dir = devserver_constants.GS_CHANNEL_DIR % {
        'channel':channel,
        'board':re.sub('_', '-', board)}
    latest_version = self._GetLatestVersionFromGsDir(channel_dir,
                                                     with_release=False)

    # Figure out release number from the version number.
    image_url = devserver_constants.IMAGE_DIR % {
        'board': board,
        'suffix': suffix,
        'version': 'R*' + latest_version}
    image_dir = XBuddy._ResolveImageDir(image_dir)
    gs_url = os.path.join(image_dir, image_url)

    # There should only be one match on cros-image-archive.
    full_version = self._GetLatestVersionFromGsDir(gs_url,
                                                   list_subdirectory=True)

    return devserver_constants.IMAGE_DIR % {'board': board,
                                            'suffix': suffix,
                                            'version': full_version}

  def _LookupVersion(self, board, suffix, version):
    """Search GS image releases for the highest match to a version prefix."""
    # Build the pattern for GS to match.
    _Log("Checking gs for latest '%s' image with prefix '%s'", board, version)
    image_url = devserver_constants.IMAGE_DIR % {'board': board,
                                                 'suffix': suffix,
                                                 'version': version + '*'}
    image_dir = os.path.join(devserver_constants.GS_IMAGE_DIR, image_url)

    # Grab the newest version of the ones matched.
    full_version = self._GetLatestVersionFromGsDir(image_dir,
                                                   list_subdirectory=True)
    return devserver_constants.IMAGE_DIR % {'board': board,
                                            'suffix': suffix,
                                            'version': full_version}

  def _RemoteBuildId(self, board, suffix, version):
    """Returns the remote build_id for the given board and version.

    Raises:
      XBuddyException: If we failed to resolve the version to a valid build_id.
    """
    build_id_as_is = devserver_constants.IMAGE_DIR % {'board': board,
                                                      'suffix': '',
                                                      'version': version}
    build_id_suffix = devserver_constants.IMAGE_DIR % {'board': board,
                                                       'suffix': suffix,
                                                       'version': version}
    # Return the first path that exists. We assume that what the user typed
    # is better than with a default suffix added i.e. x86-generic/blah is
    # more valuable than x86-generic-release/blah.
    for build_id in build_id_as_is, build_id_suffix:
      try:
        version = self._ctx.LS(
            '%s/%s' % (devserver_constants.GS_IMAGE_DIR, build_id))
        return build_id
      except gs.GSContextException as e:
        if common_util.IsAnonymousCaller(e):
          _Log('Anonymous caller cannot list chromeos image archive')
          return build_id_as_is
        continue

    raise XBuddyException('Could not find remote build_id for %s %s' % (
        board, version))

  def _ResolveBuildVersion(self, board, suffix, base_version):
    """Check LATEST-<base_version> and returns a full build version."""
    _Log('Checking gs for full version for %s of %s', base_version, board)
    # TODO(garnold) We might want to accommodate version prefixes and pick the
    # most recent found, as done in _LookupVersion().
    latest_addr = (devserver_constants.GS_LATEST_BASE_VERSION %
                   {'image_dir': devserver_constants.GS_IMAGE_DIR,
                    'board': board,
                    'suffix': suffix,
                    'base_version': base_version})
    # Full release + version is in the LATEST file.
    return self._ctx.Cat(latest_addr)

  def _ResolveVersionToBuildIdAndChannel(self, board, suffix, version,
                                         image_dir=None):
    """Handle version aliases for remote payloads in GS.

    Args:
      board: as specified in the original call. (i.e. x86-generic, parrot)
      suffix: The location suffix, to be added to board name.
      version: as entered in the original call. can be
        {TBD, 0. some custom alias as defined in a config file}
        1. fully qualified build version.
        2. latest
        3. latest-{channel}
        4. latest-official-{board suffix}
        5. version prefix (i.e. RX-Y.X, RX-Y, RX)
      image_dir: image directory to check in Google Storage. If none,
        the default bucket is used.

    Returns:
      Tuple of (Location where the image dir is actually found on GS (build_id),
      best guess for the channel).

    Raises:
      XBuddyException: If we failed to resolve the version to a valid url.
    """
    # Only the last segment of the alias is variable relative to the rest.
    version_tuple = version.rsplit('-', 1)

    if re.match(devserver_constants.VERSION_RE, version):
      return self._RemoteBuildId(board, suffix, version), None
    elif re.match(devserver_constants.VERSION, version):
      raise XBuddyException('"%s" is not valid. Should provide the fully '
                            'qualified version with a version prefix "RX-" '
                            'due to crbug.com/585914' % version)
    elif version == LATEST_OFFICIAL:
      # latest-official --> LATEST build in board-release
      return self._LookupOfficial(board, suffix, image_dir=image_dir), None
    elif version_tuple[0] == LATEST_OFFICIAL:
      # latest-official-{suffix} --> LATEST build in board-{suffix}
      return self._LookupOfficial(board, version_tuple[1],
                                  image_dir=image_dir), None
    elif version == LATEST:
      # latest --> latest build on stable channel
      return self._LookupChannel(board, suffix, image_dir=image_dir), 'stable'
    elif version_tuple[0] == LATEST:
      if re.match(devserver_constants.VERSION_RE, version_tuple[1]):
        # latest-R* --> most recent qualifying build
        return self._LookupVersion(board, suffix, version_tuple[1]), None
      else:
        # latest-{channel} --> latest build within that channel
        return self._LookupChannel(board, suffix, channel=version_tuple[1],
                                   image_dir=image_dir), version_tuple[1]
    else:
      # The given version doesn't match any known patterns.
      raise XBuddyException("Version %s unknown. Can't find on GS." % version)

  @staticmethod
  def _Symlink(link, target):
    """Symlinks link to target, and removes whatever link was there before."""
    _Log('Linking to %s from %s', link, target)
    if os.path.lexists(link):
      os.unlink(link)
    os.symlink(target, link)

  def _GetLatestLocalVersion(self, board):
    """Get the version of the latest image built for board by build_image

    Updates the symlink reference within the xBuddy static dir to point to
    the real image dir in the local /build/images directory.

    Args:
      board: board that image was built for.

    Returns:
      The discovered version of the image.

    Raises:
      XBuddyException if neither test nor dev image was found in latest built
      directory.
    """
    latest_local_dir = image_lib.GetLatestImageLink(board)
    if not latest_local_dir or not os.path.exists(latest_local_dir):
      raise XBuddyException('No builds found for %s. Did you run build_image?' %
                            board)

    # Assume that the version number is the name of the directory.
    return os.path.basename(os.path.realpath(latest_local_dir))

  @staticmethod
  def _FindAny(local_dir):
    """Returns the image_type for ANY given the local_dir."""
    test_image = os.path.join(local_dir, devserver_constants.TEST_IMAGE_FILE)
    dev_image = os.path.join(local_dir, devserver_constants.IMAGE_FILE)
    # Prioritize test images over dev images.
    if os.path.exists(test_image):
      return 'test'

    if os.path.exists(dev_image):
      return 'dev'

    raise XBuddyException('No images found in %s' % local_dir)

  @staticmethod
  def _InterpretPath(path, default_board=None, default_version=None):
    """Split and return the pieces of an xBuddy path name

    Args:
      path: the path xBuddy Get was called with.
      default_board: board to use in case board isn't in path.
      default_version: Version to use in case version isn't in path.

    Returns:
      tuple of (image_type, board, version, whether the path is local)

    Raises:
      XBuddyException: if the path can't be resolved into valid components
    """
    path_list = [p for p in path.split('/') if p]

    # Do the stuff that is well known first. We know that if paths have a
    # image_type, it must be one of the GS/LOCAL aliases and it must be at the
    # end. Similarly, local/remote are well-known and must start the path list.
    # Default to remote for chrome checkout.
    is_local = (path_util.DetermineCheckout().type !=
                path_util.CHECKOUT_TYPE_GCLIENT)
    if path_list and path_list[0] in (REMOTE, LOCAL):
      is_local = (path_list.pop(0) == LOCAL)

    # Default image type is determined by remote vs. local.
    image_type = ANY if is_local else TEST

    if path_list and path_list[-1] in GS_ALIASES + LOCAL_ALIASES:
      image_type = path_list.pop(-1)

    # Now for the tricky part. We don't actually know at this point if the rest
    # of the path is just a board | version (like R33-2341.0.0) or just a board
    # or just a version. So we do our best to do the right thing.
    board = default_board
    version = default_version or LATEST
    if len(path_list) == 1:
      path = path_list.pop(0)
      if board == path or version == path:
        pass
      # Treat this as a version if it's one we know (contains default or
      # latest), or we were given an actual default board.
      elif default_version in path or LATEST in path or board is not None:
        version = path
      else:
        board = path

    elif len(path_list) == 2:
      # Assumes board/version.
      board = path_list.pop(0)
      version = path_list.pop(0)

    if path_list:
      raise XBuddyException('Path is not valid. Could not figure out how to '
                            'parse remaining components: %s.' % path_list)

    _Log("Get artifact '%s' with board %s and version %s'. Locally? %s",
         image_type, board, version, is_local)

    return image_type, board, version, is_local

  def _SyncRegistryWithBuildImages(self):
    """Crawl images_dir for build_ids of images generated from build_image.

    This will find images and symlink them in xBuddy's static dir so that
    xBuddy's cache can serve them.
    If xBuddy's _manage_builds option is on, then a timestamp will also be
    generated, and xBuddy will clear them from the directory they are in, as
    necessary.
    """
    if not os.path.isdir(self.images_dir):
      # Skip syncing if images_dir does not exist.
      _Log('Cannot find %s; skip syncing image registry.', self.images_dir)
      return

    build_ids = []
    for b in os.listdir(self.images_dir):
      # Ignore random files in the build dir.
      board_dir = os.path.join(self.images_dir, b)
      if not os.path.isdir(board_dir):
        continue

      # Ensure we have directories to track all boards in build/images
      common_util.MkDirP(os.path.join(self.static_dir, b))
      build_ids.extend(['/'.join([b, v]) for v
                        in os.listdir(board_dir) if not v == LATEST])

    # Symlink undiscovered images, and update timestamps if manage_builds is on.
    for build_id in build_ids:
      link = os.path.join(self.static_dir, build_id)
      target = os.path.join(self.images_dir, build_id)
      XBuddy._Symlink(link, target)
      if self._manage_builds:
        Timestamp.UpdateTimestamp(self._timestamp_folder, build_id)

  def _ListBuildTimes(self):
    """Returns the currently cached builds and their last access timestamp.

    Returns:
      list of tuples that matches xBuddy build/version to timestamps in long
    """
    # Update currently cached builds.
    build_dict = {}

    for f in os.listdir(self._timestamp_folder):
      last_accessed = os.path.getmtime(os.path.join(self._timestamp_folder, f))
      build_id = Timestamp.TimestampToBuild(f)
      stale_time = datetime.timedelta(seconds=(time.time() - last_accessed))
      build_dict[build_id] = stale_time
    return_tup = sorted(build_dict.items(), key=operator.itemgetter(1))
    return return_tup

  def _Download(self, gs_url, artifacts, build_id):
    """Download the artifacts from the given gs_url.

    Raises:
      build_artifact.ArtifactDownloadError: If we failed to download the
                                            artifact.
    """
    with XBuddy._staging_thread_count_lock:
      XBuddy._staging_thread_count += 1
    try:
      _Log('Downloading %s from %s', artifacts, gs_url)
      dl = downloader.GoogleStorageDownloader(self.static_dir, gs_url, build_id)
      factory = build_artifact.ChromeOSArtifactFactory(
          dl.GetBuildDir(), artifacts, [], dl.GetBuild())
      dl.Download(factory)
    finally:
      with XBuddy._staging_thread_count_lock:
        XBuddy._staging_thread_count -= 1

  def CleanCache(self):
    """Delete all builds besides the newest N builds"""
    if not self._manage_builds:
      return
    cached_builds = [e[0] for e in self._ListBuildTimes()]
    _Log('In cache now: %s', cached_builds)

    for b in range(self._Capacity(), len(cached_builds)):
      b_path = cached_builds[b]
      _Log("Clearing '%s' from cache", b_path)

      time_file = os.path.join(self._timestamp_folder,
                               Timestamp.BuildToTimestamp(b_path))
      os.unlink(time_file)
      clear_dir = os.path.join(self.static_dir, b_path)
      try:
        # Handle symlinks, in the case of links to local builds if enabled.
        if os.path.islink(clear_dir):
          target = os.readlink(clear_dir)
          _Log('Deleting locally built image at %s', target)

          os.unlink(clear_dir)
          if os.path.exists(target):
            shutil.rmtree(target)
        elif os.path.exists(clear_dir):
          _Log('Deleting downloaded image at %s', clear_dir)
          shutil.rmtree(clear_dir)

      except Exception as err:
        raise XBuddyException('Failed to clear %s: %s' % (clear_dir, err))

  def _TranslateSignedGSUrl(self, build_id, channel=None):
    """Translate the GS URL to be able to find signed images.

    Args:
      build_id: Path to the image or update directory on the devserver or
        in Google Storage. e.g. 'x86-generic/R26-4000.0.0'
      channel: The channel for the image. If none, it tries to guess it in
        order of stability.

    Returns:
      The GS URL for the directory where the signed image can be found.

    Raises:
      build_artifact.ArtifactDownloadError: If we failed to download the
                                            artifact.
    """
    match = re.match(r'^([^/]+?)(?:-release)?/R\d+-(.*)$', build_id)

    channels = []
    if channel:
      channels.append(channel)
    else:
      # Attempt to enumerate all channels, in order of stability.
      channels.extend(devserver_constants.CHANNELS[::-1])

    for c in channels:
      image_dir = devserver_constants.GS_CHANNEL_DIR % {
          'channel': c,
          'board': match.group(1),
      }
      gs_url = os.path.join(image_dir, match.group(2))
      try:
        self._LS(gs_url)
        return gs_url
      except gs.GSNoSuchKey:
        continue
    raise build_artifact.ArtifactDownloadError(
        'Could not find signed image URL for %s in Google Storage' %
        build_id)

  def _GetFromGS(self, build_id, image_type, image_dir=None, channel=None):
    """Check if the artifact is available locally. Download from GS if not.

    Args:
      build_id: Path to the image or update directory on the devserver or
        in Google Storage. e.g. 'x86-generic/R26-4000.0.0'
      image_type: Image type to download. Look at aliases at top of file for
        options.
      image_dir: Google Storage image archive to search in if requesting a
        remote artifact. If none uses the default bucket.
      channel: The channel for the image. If none, it tries to guess it in
        order of stability.

    Raises:
      build_artifact.ArtifactDownloadError: If we failed to download the
        artifact.
    """
    # Stage image if not found in cache.
    file_name = GS_ALIAS_TO_FILENAME[image_type]
    file_loc = os.path.join(self.static_dir, build_id, file_name)
    cached = os.path.exists(file_loc)

    if not cached:
      artifact = GS_ALIAS_TO_ARTIFACT[image_type]
      if image_type == SIGNED:
        gs_url = self._TranslateSignedGSUrl(build_id, channel=channel)
      else:
        image_dir = XBuddy._ResolveImageDir(image_dir)
        gs_url = os.path.join(image_dir, build_id)
      self._Download(gs_url, [artifact], build_id)
    else:
      _Log('Image already cached.')

  def _GetArtifact(self, path_list, board=None, version=None,
                   lookup_only=False, image_dir=None):
    """Interpret an xBuddy path and return directory/file_name to resource.

    Note board can be passed that in but by default if self._board is set,
    that is used rather than board.

    Args:
      path_list: [board, version, alias] as split from the xbuddy call url.
      board: Board whos artifacts we are looking for. Only used if no board was
        given during XBuddy initialization.
      version: Version whose artifacts we are looking for. Used if no version
        was given during XBuddy initialization. If None, defers to LATEST.
      lookup_only: If true just look up the artifact, if False stage it on
        the devserver as well.
      image_dir: Google Storage image archive to search in if requesting a
        remote artifact. If none uses the default bucket.

    Returns:
      build_id: Path to the image or update directory on the devserver or
        in Google Storage. e.g. 'x86-generic/R26-4000.0.0'
      file_name: of the artifact in the build_id directory.

    Raises:
      XBuddyException: if the path could not be translated
      build_artifact.ArtifactDownloadError: if we failed to download the
                                            artifact.
    """
    path = '/'.join(path_list)
    default_board = self._board or board
    default_version = self._version or version or LATEST
    # Rewrite the path if there is an appropriate default.
    path, suffix = self.LookupAlias(path, board=default_board,
                                    version=default_version)
    # Parse the path.
    image_type, board, version, is_local = self._InterpretPath(
        path, default_board, default_version)
    if is_local:
      # Get a local image.
      if version == LATEST:
        # Get the latest local image for the given board.
        version = self._GetLatestLocalVersion(board)

      build_id = os.path.join(board, version)
      artifact_dir = os.path.join(self.static_dir, build_id)
      if image_type == ANY:
        image_type = self._FindAny(artifact_dir)

      file_name = LOCAL_ALIAS_TO_FILENAME[image_type]
      artifact_path = os.path.join(artifact_dir, file_name)
      if not os.path.exists(artifact_path):
        raise XBuddyException('Local %s artifact not in static_dir at %s' %
                              (image_type, artifact_path))

    else:
      # Get a remote image.
      if image_type not in GS_ALIASES:
        raise XBuddyException('Bad remote image type: %s. Use one of: %s' %
                              (image_type, GS_ALIASES))
      build_id, channel = self._ResolveVersionToBuildIdAndChannel(
          board, suffix, version, image_dir=image_dir)
      _Log('Resolved version %s to %s.', version, build_id)
      file_name = GS_ALIAS_TO_FILENAME[image_type]
      if not lookup_only:
        self._GetFromGS(build_id, image_type, image_dir=image_dir,
                        channel=channel)

    return build_id, file_name

  ############################ BEGIN PUBLIC METHODS

  def List(self):
    """Lists the currently available images & time since last access."""
    self._SyncRegistryWithBuildImages()
    builds = self._ListBuildTimes()
    return_string = ''
    for build, timestamp in builds:
      return_string += '<b>' + build + '</b>       '
      return_string += '(time since last access: ' + str(timestamp) + ')<br>'
    return return_string

  def Capacity(self):
    """Returns the number of images cached by xBuddy."""
    return str(self._Capacity())

  def Translate(self, path_list, board=None, version=None, image_dir=None):
    """Translates an xBuddy path to a real path to artifact if it exists.

    Equivalent to the Get call, minus downloading and updating timestamps,

    Args:
      path_list: [board, version, alias] as split from the xbuddy call url.
      board: Board whos artifacts we are looking for. If None, use the board
        XBuddy was initialized to use.
      version: Version whose artifacts we are looking for. If None, use the
        version XBuddy was initialized with, or LATEST.
      image_dir: image directory to check in Google Storage. If none,
        the default bucket is used.

    Returns:
      build_id: Path to the image or update directory on the devserver.
        e.g. 'x86-generic/R26-4000.0.0'
        The returned path is always the path to the directory within
        static_dir, so it is always the build_id of the image.
      file_name: The file name of the artifact. Can take any of the file
        values in devserver_constants.
        e.g. 'chromiumos_test_image.bin' or 'update.gz' if the path list
        specified 'test' or 'full_payload' artifacts, respectively.

    Raises:
      XBuddyException: if the path couldn't be translated
    """
    self._SyncRegistryWithBuildImages()
    build_id, file_name = self._GetArtifact(path_list, board=board,
                                            version=version,
                                            lookup_only=True,
                                            image_dir=image_dir)

    _Log('Returning path to payload: %s/%s', build_id, file_name)
    return build_id, file_name

  def StageTestArtifactsForUpdate(self, path_list):
    """Stages test artifacts for update and returns build_id.

    Raises:
      XBuddyException: if the path could not be translated
      build_artifact.ArtifactDownloadError: if we failed to download the test
                                            artifacts.
    """
    build_id, file_name = self.Translate(path_list)
    if file_name == devserver_constants.TEST_IMAGE_FILE:
      gs_url = os.path.join(devserver_constants.GS_IMAGE_DIR,
                            build_id)
      artifacts = [FULL, STATEFUL]
      self._Download(gs_url, artifacts, build_id)
      return build_id

  def Get(self, path_list, image_dir=None):
    """The full xBuddy call, returns resource specified by path_list.

    Please see devserver.py:xbuddy for full documentation.

    Args:
      path_list: [board, version, alias] as split from the xbuddy call url.
      image_dir: image directory to check in Google Storage. If none,
        the default bucket is used.

    Returns:
      build_id: Path to the image or update directory on the devserver.
        e.g. 'x86-generic/R26-4000.0.0'
        The returned path is always the path to the directory within
        static_dir, so it is always the build_id of the image.
      file_name: The file name of the artifact. Can take any of the file
        values in devserver_constants.
        e.g. 'chromiumos_test_image.bin' or 'update.gz' if the path list
        specified 'test' or 'full_payload' artifacts, respectively.

    Raises:
      XBuddyException: if the path could not be translated
      build_artifact.ArtifactDownloadError: if we failed to download the
                                            artifact.
    """
    self._SyncRegistryWithBuildImages()
    build_id, file_name = self._GetArtifact(path_list, image_dir=image_dir)
    Timestamp.UpdateTimestamp(self._timestamp_folder, build_id)
    # TODO(joyc): Run in separate thread.
    self.CleanCache()

    _Log('Returning path to payload: %s/%s', build_id, file_name)
    return build_id, file_name
