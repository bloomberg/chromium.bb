#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the Chrome reference builds.

Use -r option to update a Chromium reference build, or -b option for Chrome
official builds.

Usage:
  $ cd /tmp
  $ /path/to/update_reference_build.py -r <revision>
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

# Example chromium build location:
# gs://chromium-browser-snapshots/Linux/228977/chrome-linux.zip
CHROMIUM_URL_FMT = ('http://commondatastorage.googleapis.com/'
                    'chromium-browser-snapshots/%s/%s/%s')

# Chrome official build storage
# https://wiki.corp.google.com/twiki/bin/view/Main/ChromeOfficialBuilds

# Internal Google archive of official Chrome builds, example:
# https://goto.google.com/chrome_official_builds/
# 32.0.1677.0/precise32bit/chrome-precise32bit.zip
CHROME_INTERNAL_URL_FMT = ('http://master.chrome.corp.google.com/'
                           'official_builds/%s/%s/%s')

# Google storage location (no public web URL's), example:
# gs://chrome-archive/30/30.0.1595.0/precise32bit/chrome-precise32bit.zip
CHROME_GS_URL_FMT = ('gs://chrome-archive/%s/%s/%s/%s')


class BuildUpdater(object):
  _PLATFORM_FILES_MAP = {
      'Win': [
          'chrome-win32.zip',
          'chrome-win32-syms.zip',
      ],
      'Mac': [
          'chrome-mac.zip',
      ],
      'Linux': [
          'chrome-linux.zip',
      ],
      'Linux_x64': [
          'chrome-linux.zip',
      ],
  }

  _CHROME_PLATFORM_FILES_MAP = {
      'Win': [
          'chrome-win32.zip',
          'chrome-win32-syms.zip',
      ],
      'Mac': [
          'chrome-mac.zip',
      ],
      'Linux': [
          'chrome-precise32bit.zip',
      ],
      'Linux_x64': [
          'chrome-precise64bit.zip',
      ],
  }

  # Map of platform names to gs:// Chrome build names.
  _BUILD_PLATFORM_MAP = {
      'Linux': 'precise32bit',
      'Linux_x64': 'precise64bit',
      'Win': 'win',
      'Mac': 'mac',
  }

  _PLATFORM_DEST_MAP = {
      'Linux': 'chrome_linux',
      'Linux_x64': 'chrome_linux64',
      'Win': 'chrome_win',
      'Mac': 'chrome_mac',
  }

  def __init__(self, options):
    self._platforms = options.platforms.split(',')
    self._revision = options.build_number or int(options.revision)
    self._use_build_number = bool(options.build_number)
    self._use_gs = options.use_gs

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

  def _GetBuildUrl(self, platform, revision, filename):
    if self._use_build_number:
      # Chrome Google storage bucket.
      if self._use_gs:
        release = revision[:revision.find('.')]
        return (CHROME_GS_URL_FMT % (
            release,
            revision,
            self._BUILD_PLATFORM_MAP[platform],
            filename))
      # Chrome internal archive.
      return (CHROME_INTERNAL_URL_FMT % (
          revision,
          self._BUILD_PLATFORM_MAP[platform],
          filename))
    # Chromium archive.
    return CHROMIUM_URL_FMT % (urllib.quote_plus(platform), revision, filename)

  def _FindBuildRevision(self, platform, revision, filename):
    # TODO(shadi): Iterate over build numbers to find a valid one.
    if self._use_build_number:
      return (revision
              if self._DoesBuildExist(platform, revision, filename) else None)

    MAX_REVISIONS_PER_BUILD = 100
    for revision_guess in xrange(revision, revision + MAX_REVISIONS_PER_BUILD):
      if self._DoesBuildExist(platform, revision_guess, filename):
        return revision_guess
      else:
        time.sleep(.1)
    return None

  def _DoesBuildExist(self, platform, build_number, filename):
    url = self._GetBuildUrl(platform, build_number, filename)
    if self._use_gs:
      return self._DoesGSFileExist(url)

    r = urllib2.Request(url)
    r.get_method = lambda: 'HEAD'
    try:
      urllib2.urlopen(r)
      return True
    except urllib2.HTTPError, err:
      if err.code == 404:
        return False

  def _DoesGSFileExist(self, gs_file_name):
    exit_code = BuildUpdater._GetCmdStatusAndOutput(
        ['gsutil', 'ls', gs_file_name])[0]
    return not exit_code

  def _GetPlatformFiles(self, platform):
    if self._use_build_number:
      return BuildUpdater._CHROME_PLATFORM_FILES_MAP[platform]
    return BuildUpdater._PLATFORM_FILES_MAP[platform]

  def _DownloadBuilds(self):
    for platform in self._platforms:
      for f in self._GetPlatformFiles(platform):
        output = os.path.join('dl', platform,
                              '%s_%s_%s' % (platform, self._revision, f))
        if os.path.exists(output):
          logging.info('%s alread exists, skipping download', output)
          continue
        build_revision = self._FindBuildRevision(platform, self._revision, f)
        if not build_revision:
          logging.critical('Failed to find %s build for r%s\n', platform,
                           self._revision)
          sys.exit(1)
        dirname = os.path.dirname(output)
        if dirname and not os.path.exists(dirname):
          os.makedirs(dirname)
        url = self._GetBuildUrl(platform, build_revision, f)
        self._DownloadFile(url, output)

  def _DownloadFile(self, url, output):
    logging.info('Downloading %s, saving to %s', url, output)
    if self._use_build_number and self._use_gs:
      BuildUpdater._GetCmdStatusAndOutput(['gsutil', 'cp', url, output])
    else:
      r = urllib2.urlopen(url)
      with file(output, 'wb') as f:
        f.write(r.read())

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
  usage = 'usage: %prog <options>'
  parser.set_usage(usage)
  parser.add_option('-b', dest='build_number',
                    help='Chrome official build number to pick up.')
  parser.add_option('--gs', dest='use_gs', action='store_true', default=False,
                    help='Use Google storage for official builds. Used with -b '
                         'option. Default is false (i.e. use internal storage.')
  parser.add_option('-p', dest='platforms',
                    default='Win,Mac,Linux,Linux_x64',
                    help='Comma separated list of platforms to download '
                         '(as defined by the chromium builders).')
  parser.add_option('-r', dest='revision',
                    help='Revision to pick up.')

  (options, _) = parser.parse_args(argv)
  if not options.revision and not options.build_number:
    logging.critical('Must specify either -r or -b.\n')
    sys.exit(1)
  if options.revision and options.build_number:
    logging.critical('Must specify either -r or -b but not both.\n')
    sys.exit(1)
  if options.use_gs and not options.build_number:
    logging.critical('Can only use --gs with -b option.\n')
    sys.exit(1)

  return options


def main(argv):
  logging.getLogger().setLevel(logging.DEBUG)
  options = ParseOptions(argv)
  b = BuildUpdater(options)
  b.DownloadAndUpdateBuilds()
  logging.info('Successfully updated reference builds. Move to '
               'reference_builds/reference_builds and make a change with gcl.')

if __name__ == '__main__':
  sys.exit(main(sys.argv))
