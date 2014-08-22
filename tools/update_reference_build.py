#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the Chrome reference builds.

Before running this script, you should first verify that you are authenticated
for SVN. You can do this by running:
  $ svn ls svn://svn.chromium.org/chrome/trunk/deps/reference_builds
You may need to get your SVN password from https://chromium-access.appspot.com/.

Usage:
  $ cd /tmp
  $ /path/to/update_reference_build.py VERSION  # e.g. 37.0.2062.94
  $ cd reference_builds/reference_builds
  $ gcl change
  $ gcl upload <change>
  $ gcl commit <change>
"""

import logging
import optparse
import os
import shutil
import subprocess
import sys
import time
import urllib
import urllib2
import zipfile


# Google storage location (no public web URL's), example:
# gs://chrome-unsigned/desktop-*/30.0.1595.0/precise32/chrome-precise32.zip
CHROME_GS_URL_FMT = ('gs://chrome-unsigned/desktop-*/%s/%s/%s')


class BuildUpdater(object):
  _CHROME_PLATFORM_FILES_MAP = {
      'Win': [
          'chrome-win.zip',
      ],
      'Mac': [
          'chrome-mac.zip',
      ],
      'Linux': [
          'chrome-precise32.zip',
      ],
      'Linux_x64': [
          'chrome-precise64.zip',
      ],
  }

  # Map of platform names to gs:// Chrome build names.
  _BUILD_PLATFORM_MAP = {
      'Linux': 'precise32',
      'Linux_x64': 'precise64',
      'Win': 'win',
      'Mac': 'mac',
  }

  _PLATFORM_DEST_MAP = {
      'Linux': 'chrome_linux',
      'Linux_x64': 'chrome_linux64',
      'Win': 'chrome_win',
      'Mac': 'chrome_mac',
  }

  def __init__(self, version, options):
    self._version = version
    self._platforms = options.platforms.split(',')

  @staticmethod
  def _GetCmdStatusAndOutput(args, cwd=None, shell=False):
    """Executes a subprocess and returns its exit code and output.

    Args:
      args: A string or a sequence of program arguments.
      cwd: If not None, the subprocess's current directory will be changed to
        |cwd| before it's executed.
      shell: Whether to execute args as a shell command.

    Returns:
      The tuple (exit code, output).
    """
    logging.info(str(args) + ' ' + (cwd or ''))
    p = subprocess.Popen(args=args, cwd=cwd, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, shell=shell)
    stdout, stderr = p.communicate()
    exit_code = p.returncode
    if stderr:
      logging.critical(stderr)
    logging.info(stdout)
    return (exit_code, stdout)

  def _GetBuildUrl(self, platform, version, filename):
    """Returns the URL for fetching one file.

    Args:
      platform: Platform name, must be a key in |self._BUILD_PLATFORM_MAP|.
      version: A Chrome version number, e.g. 30.0.1600.1.
      filename: Name of the file to fetch.

    Returns:
      The URL for fetching a file. This may be a GS or HTTP URL.
    """
    return CHROME_GS_URL_FMT % (
        version, self._BUILD_PLATFORM_MAP[platform], filename)

  def _FindBuildVersion(self, platform, version, filename):
    """Searches for a version where a filename can be found.

    Args:
      platform: Platform name.
      version: A Chrome version number, e.g. 30.0.1600.1.
      filename: Filename to look for.

    Returns:
      A version where the file could be found, or None.
    """
    # TODO(shadi): Iterate over official versions to find a valid one.
    return (version
            if self._DoesBuildExist(platform, version, filename) else None)

  def _DoesBuildExist(self, platform, version, filename):
    """Checks whether a file can be found for the given Chrome version.

    Args:
      platform: Platform name.
      version: Chrome version number, e.g. 30.0.1600.1.
      filename: Filename to look for.

    Returns:
      True if the file could be found, False otherwise.
    """
    url = self._GetBuildUrl(platform, version, filename)
    return self._DoesGSFileExist(url)

  def _DoesGSFileExist(self, gs_file_name):
    """Returns True if the GS file can be found, False otherwise."""
    exit_code = BuildUpdater._GetCmdStatusAndOutput(
        ['gsutil', 'ls', gs_file_name])[0]
    return not exit_code

  def _GetPlatformFiles(self, platform):
    """Returns a list of filenames to fetch for the given platform."""
    return BuildUpdater._CHROME_PLATFORM_FILES_MAP[platform]

  def _DownloadBuilds(self):
    for platform in self._platforms:
      for filename in self._GetPlatformFiles(platform):
        output = os.path.join('dl', platform,
                              '%s_%s_%s' % (platform,
                                            self._version,
                                            filename))
        if os.path.exists(output):
          logging.info('%s alread exists, skipping download', output)
          continue
        version = self._FindBuildVersion(platform, self._version, filename)
        if not version:
          logging.critical('Failed to find %s build for r%s\n', platform,
                           self._version)
          sys.exit(1)
        dirname = os.path.dirname(output)
        if dirname and not os.path.exists(dirname):
          os.makedirs(dirname)
        url = self._GetBuildUrl(platform, version, filename)
        self._DownloadFile(url, output)

  def _DownloadFile(self, url, output):
    logging.info('Downloading %s, saving to %s', url, output)
    BuildUpdater._GetCmdStatusAndOutput(['gsutil', 'cp', url, output])

  def _FetchSvnRepos(self):
    if not os.path.exists('reference_builds'):
      os.makedirs('reference_builds')
    BuildUpdater._GetCmdStatusAndOutput(
        ['gclient', 'config',
         'svn://svn.chromium.org/chrome/trunk/deps/reference_builds'],
        'reference_builds')
    BuildUpdater._GetCmdStatusAndOutput(
        ['gclient', 'sync'], 'reference_builds')

  def _UnzipFile(self, dl_file, dest_dir):
    """Unzips a file if it is a zip file.

    Args:
      dl_file: The downloaded file to unzip.
      dest_dir: The destination directory to unzip to.

    Returns:
      True if the file was unzipped. False if it wasn't a zip file.
    """
    if not zipfile.is_zipfile(dl_file):
      return False
    logging.info('Opening %s', dl_file)
    with zipfile.ZipFile(dl_file, 'r') as z:
      for content in z.namelist():
        dest = os.path.join(dest_dir, content[content.find('/')+1:])
        # Create dest parent dir if it does not exist.
        if not os.path.isdir(os.path.dirname(dest)):
          os.makedirs(os.path.dirname(dest))
        # If dest is just a dir listing, do nothing.
        if not os.path.basename(dest):
          continue
        if not os.path.isdir(os.path.dirname(dest)):
          os.makedirs(os.path.dirname(dest))
        with z.open(content) as unzipped_content:
          logging.info('Extracting %s to %s (%s)', content, dest, dl_file)
          with file(dest, 'wb') as dest_file:
            dest_file.write(unzipped_content.read())
          permissions = z.getinfo(content).external_attr >> 16
          if permissions:
            os.chmod(dest, permissions)
    return True

  def _ClearDir(self, dir):
    """Clears all files in |dir| except for hidden files and folders."""
    for root, dirs, files in os.walk(dir):
      # Skip hidden files and folders (like .svn and .git).
      files = [f for f in files if f[0] != '.']
      dirs[:] = [d for d in dirs if d[0] != '.']

      for f in files:
        os.remove(os.path.join(root, f))

  def _ExtractBuilds(self):
    for platform in self._platforms:
      if os.path.exists('tmp_unzip'):
        os.path.unlink('tmp_unzip')
      dest_dir = os.path.join('reference_builds', 'reference_builds',
                              BuildUpdater._PLATFORM_DEST_MAP[platform])
      self._ClearDir(dest_dir)
      for root, _, dl_files in os.walk(os.path.join('dl', platform)):
        for dl_file in dl_files:
          dl_file = os.path.join(root, dl_file)
          if not self._UnzipFile(dl_file, dest_dir):
            logging.info('Copying %s to %s', dl_file, dest_dir)
            shutil.copy(dl_file, dest_dir)

  def _SvnAddAndRemove(self):
    svn_dir = os.path.join('reference_builds', 'reference_builds')
    # List all changes without ignoring any files.
    stat = BuildUpdater._GetCmdStatusAndOutput(['svn', 'stat', '--no-ignore'],
                                               svn_dir)[1]
    for line in stat.splitlines():
      action, filename = line.split(None, 1)
      # Add new and ignored files.
      if action == '?' or action == 'I':
        BuildUpdater._GetCmdStatusAndOutput(
            ['svn', 'add', filename], svn_dir)
      elif action == '!':
        BuildUpdater._GetCmdStatusAndOutput(
            ['svn', 'delete', filename], svn_dir)
      filepath = os.path.join(svn_dir, filename)
      if not os.path.isdir(filepath) and os.access(filepath, os.X_OK):
        BuildUpdater._GetCmdStatusAndOutput(
            ['svn', 'propset', 'svn:executable', 'true', filename], svn_dir)

  def DownloadAndUpdateBuilds(self):
    self._DownloadBuilds()
    self._FetchSvnRepos()
    self._ExtractBuilds()
    self._SvnAddAndRemove()


def ParseOptions(argv):
  parser = optparse.OptionParser()
  parser.set_usage('Usage: %prog VERSION [-p PLATFORMS]')
  parser.add_option('-p', dest='platforms',
                    default='Win,Mac,Linux,Linux_x64',
                    help='Comma separated list of platforms to download '
                         '(as defined by the chromium builders).')

  options, args = parser.parse_args(argv)
  if len(args) != 2:
    parser.print_help()
    sys.exit(1)
  version = args[1]

  return version, options


def main(argv):
  logging.getLogger().setLevel(logging.DEBUG)
  version, options = ParseOptions(argv)
  b = BuildUpdater(version, options)
  b.DownloadAndUpdateBuilds()
  logging.info('Successfully updated reference builds. Move to '
               'reference_builds/reference_builds and make a change with gcl.')

if __name__ == '__main__':
  sys.exit(main(sys.argv))
