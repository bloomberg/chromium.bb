#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Snapshot Build Bisect Tool

This script bisects a snapshot archive using binary search. It starts at
a bad revision (it will try to guess HEAD) and asks for a last known-good
revision. It will then binary search across this revision range by downloading,
unzipping, and opening Chromium for you. After testing the specific revision,
it will ask you whether it is good or bad before continuing the search.
"""

# The base URL for stored build archives.
CHROMIUM_BASE_URL = ('http://commondatastorage.googleapis.com'
                     '/chromium-browser-snapshots')
WEBKIT_BASE_URL = ('http://commondatastorage.googleapis.com'
                   '/chromium-webkit-snapshots')

# The base URL for official builds.
OFFICIAL_BASE_URL = 'http://master.chrome.corp.google.com/official_builds'

# URL template for viewing changelogs between revisions.
CHANGELOG_URL = ('http://build.chromium.org'
                 '/f/chromium/perf/dashboard/ui/changelog.html'
                 '?url=/trunk/src&range=%d%%3A%d')

# URL template for viewing changelogs between official versions.
OFFICIAL_CHANGELOG_URL = ('http://omahaproxy.appspot.com/changelog'
                          '?old_version=%s&new_version=%s')

# DEPS file URL.
DEPS_FILE = 'http://src.chromium.org/viewvc/chrome/trunk/src/DEPS?revision=%d'

# Blink changelogs URL.
BLINK_CHANGELOG_URL = ('http://build.chromium.org'
                      '/f/chromium/perf/dashboard/ui/changelog_blink.html'
                      '?url=/trunk&range=%d%%3A%d')

DONE_MESSAGE_GOOD_MIN = ('You are probably looking for a change made after %s ('
                         'known good), but no later than %s (first known bad).')
DONE_MESSAGE_GOOD_MAX = ('You are probably looking for a change made after %s ('
                         'known bad), but no later than %s (first known good).')

CHROMIUM_GITHASH_TO_SVN_URL = (
    'https://chromium.googlesource.com/chromium/src/+/%s?format=json')

BLINK_GITHASH_TO_SVN_URL = (
    'https://chromium.googlesource.com/chromium/blink/+/%s?format=json')

GITHASH_TO_SVN_URL = {
    'chromium': CHROMIUM_GITHASH_TO_SVN_URL,
    'blink': BLINK_GITHASH_TO_SVN_URL,
}

# Search pattern to be matched in the JSON output from
# CHROMIUM_GITHASH_TO_SVN_URL to get the chromium revision (svn revision).
CHROMIUM_SEARCH_PATTERN = (
    r'.*git-svn-id: svn://svn.chromium.org/chrome/trunk/src@(\d+) ')

# Search pattern to be matched in the json output from
# BLINK_GITHASH_TO_SVN_URL to get the blink revision (svn revision).
BLINK_SEARCH_PATTERN = (
    r'.*git-svn-id: svn://svn.chromium.org/blink/trunk@(\d+) ')

SEARCH_PATTERN = {
    'chromium': CHROMIUM_SEARCH_PATTERN,
    'blink': BLINK_SEARCH_PATTERN,
}

###############################################################################

import json
import optparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import threading
import urllib
from distutils.version import LooseVersion
from xml.etree import ElementTree
import zipfile


class PathContext(object):
  """A PathContext is used to carry the information used to construct URLs and
  paths when dealing with the storage server and archives."""
  def __init__(self, base_url, platform, good_revision, bad_revision,
               is_official, is_aura, use_local_repo, flash_path = None,
               pdf_path = None):
    super(PathContext, self).__init__()
    # Store off the input parameters.
    self.base_url = base_url
    self.platform = platform  # What's passed in to the '-a/--archive' option.
    self.good_revision = good_revision
    self.bad_revision = bad_revision
    self.is_official = is_official
    self.is_aura = is_aura
    self.flash_path = flash_path
    # Dictionary which stores svn revision number as key and it's
    # corresponding git hash as value. This data is populated in
    # _FetchAndParse and used later in GetDownloadURL while downloading
    # the build.
    self.githash_svn_dict = {}
    self.pdf_path = pdf_path

    # The name of the ZIP file in a revision directory on the server.
    self.archive_name = None

    # If the script is run from a local Chromium checkout,
    # "--use-local-repo" option can be used to make the script run faster.
    # It uses "git svn find-rev <SHA1>" command to convert git hash to svn
    # revision number.
    self.use_local_repo = use_local_repo

    # Set some internal members:
    #   _listing_platform_dir = Directory that holds revisions. Ends with a '/'.
    #   _archive_extract_dir = Uncompressed directory in the archive_name file.
    #   _binary_name = The name of the executable to run.
    if self.platform in ('linux', 'linux64', 'linux-arm'):
      self._binary_name = 'chrome'
    elif self.platform == 'mac':
      self.archive_name = 'chrome-mac.zip'
      self._archive_extract_dir = 'chrome-mac'
    elif self.platform == 'win':
      self.archive_name = 'chrome-win32.zip'
      self._archive_extract_dir = 'chrome-win32'
      self._binary_name = 'chrome.exe'
    else:
      raise Exception('Invalid platform: %s' % self.platform)

    if is_official:
      if self.platform == 'linux':
        self._listing_platform_dir = 'precise32bit/'
        self.archive_name = 'chrome-precise32bit.zip'
        self._archive_extract_dir = 'chrome-precise32bit'
      elif self.platform == 'linux64':
        self._listing_platform_dir = 'precise64bit/'
        self.archive_name = 'chrome-precise64bit.zip'
        self._archive_extract_dir = 'chrome-precise64bit'
      elif self.platform == 'mac':
        self._listing_platform_dir = 'mac/'
        self._binary_name = 'Google Chrome.app/Contents/MacOS/Google Chrome'
      elif self.platform == 'win':
        if self.is_aura:
          self._listing_platform_dir = 'win-aura/'
        else:
          self._listing_platform_dir = 'win/'
    else:
      if self.platform in ('linux', 'linux64', 'linux-arm'):
        self.archive_name = 'chrome-linux.zip'
        self._archive_extract_dir = 'chrome-linux'
        if self.platform == 'linux':
          self._listing_platform_dir = 'Linux/'
        elif self.platform == 'linux64':
          self._listing_platform_dir = 'Linux_x64/'
        elif self.platform == 'linux-arm':
          self._listing_platform_dir = 'Linux_ARM_Cross-Compile/'
      elif self.platform == 'mac':
        self._listing_platform_dir = 'Mac/'
        self._binary_name = 'Chromium.app/Contents/MacOS/Chromium'
      elif self.platform == 'win':
        self._listing_platform_dir = 'Win/'

  def GetListingURL(self, marker=None):
    """Returns the URL for a directory listing, with an optional marker."""
    marker_param = ''
    if marker:
      marker_param = '&marker=' + str(marker)
    return (self.base_url + '/?delimiter=/&prefix=' +
            self._listing_platform_dir + marker_param)

  def GetDownloadURL(self, revision):
    """Gets the download URL for a build archive of a specific revision."""
    if self.is_official:
      return '%s/%s/%s%s' % (
          OFFICIAL_BASE_URL, revision, self._listing_platform_dir,
          self.archive_name)
    else:
      if str(revision) in self.githash_svn_dict:
        revision = self.githash_svn_dict[str(revision)]
      return '%s/%s%s/%s' % (self.base_url, self._listing_platform_dir,
                             revision, self.archive_name)

  def GetLastChangeURL(self):
    """Returns a URL to the LAST_CHANGE file."""
    return self.base_url + '/' + self._listing_platform_dir + 'LAST_CHANGE'

  def GetLaunchPath(self):
    """Returns a relative path (presumably from the archive extraction location)
    that is used to run the executable."""
    return os.path.join(self._archive_extract_dir, self._binary_name)

  @staticmethod
  def IsAuraBuild(build):
    """Checks whether the given build is an Aura build."""
    return build.split('.')[3] == '1'

  @staticmethod
  def IsASANBuild(build):
    """Checks whether the given build is an ASAN build."""
    return build.split('.')[3] == '2'

  def ParseDirectoryIndex(self):
    """Parses the Google Storage directory listing into a list of revision
    numbers."""

    def _FetchAndParse(url):
      """Fetches a URL and returns a 2-Tuple of ([revisions], next-marker). If
      next-marker is not None, then the listing is a partial listing and another
      fetch should be performed with next-marker being the marker= GET
      parameter."""
      handle = urllib.urlopen(url)
      document = ElementTree.parse(handle)

      # All nodes in the tree are namespaced. Get the root's tag name to extract
      # the namespace. Etree does namespaces as |{namespace}tag|.
      root_tag = document.getroot().tag
      end_ns_pos = root_tag.find('}')
      if end_ns_pos == -1:
        raise Exception('Could not locate end namespace for directory index')
      namespace = root_tag[:end_ns_pos + 1]

      # Find the prefix (_listing_platform_dir) and whether or not the list is
      # truncated.
      prefix_len = len(document.find(namespace + 'Prefix').text)
      next_marker = None
      is_truncated = document.find(namespace + 'IsTruncated')
      if is_truncated is not None and is_truncated.text.lower() == 'true':
        next_marker = document.find(namespace + 'NextMarker').text
      # Get a list of all the revisions.
      all_prefixes = document.findall(namespace + 'CommonPrefixes/' +
                                      namespace + 'Prefix')
      # The <Prefix> nodes have content of the form of
      # |_listing_platform_dir/revision/|. Strip off the platform dir and the
      # trailing slash to just have a number.
      revisions = []
      githash_svn_dict = {}
      for prefix in all_prefixes:
        revnum = prefix.text[prefix_len:-1]
        try:
          if not revnum.isdigit():
            git_hash = revnum
            revnum = self.GetSVNRevisionFromGitHash(git_hash)
            githash_svn_dict[revnum] = git_hash
          if revnum is not None:
            revnum = int(revnum)
            revisions.append(revnum)
        except ValueError:
          pass
      return (revisions, next_marker, githash_svn_dict)

    # Fetch the first list of revisions.
    (revisions, next_marker, self.githash_svn_dict) = _FetchAndParse(
        self.GetListingURL())
    # If the result list was truncated, refetch with the next marker. Do this
    # until an entire directory listing is done.
    while next_marker:
      next_url = self.GetListingURL(next_marker)
      (new_revisions, next_marker, new_dict) = _FetchAndParse(next_url)
      revisions.extend(new_revisions)
      self.githash_svn_dict.update(new_dict)
    return revisions

  def _GetSVNRevisionFromGitHashWithoutGitCheckout(self, git_sha1, depot):
    json_url = GITHASH_TO_SVN_URL[depot] % git_sha1
    try:
      response = urllib.urlopen(json_url)
    except urllib.HTTPError as error:
      msg = 'HTTP Error %d for %s' % (error.getcode(), git_sha1)
      return None
    data = json.loads(response.read()[4:])
    if 'message' in data:
      message = data['message'].split('\n')
      message = [line for line in message if line.strip()]
      search_pattern = re.compile(SEARCH_PATTERN[depot])
      result = search_pattern.search(message[len(message)-1])
      if result:
        return result.group(1)
    print 'Failed to get svn revision number for %s' % git_sha1
    raise ValueError

  def _GetSVNRevisionFromGitHashFromGitCheckout(self, git_sha1, depot):
    def _RunGit(command, path):
      command = ['git'] + command
      if path:
        original_path = os.getcwd()
        os.chdir(path)
      shell = sys.platform.startswith('win')
      proc = subprocess.Popen(command, shell=shell, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE)
      (output, _) = proc.communicate()

      if path:
        os.chdir(original_path)
      return (output, proc.returncode)

    path = None
    if depot == 'blink':
      path = os.path.join(os.getcwd(), 'third_party', 'WebKit')
    if os.path.basename(os.getcwd()) == 'src':
      command = ['svn', 'find-rev', git_sha1]
      (git_output, return_code) = _RunGit(command, path)
      if not return_code:
        return git_output.strip('\n')
      raise ValueError
    else:
      print ('Script should be run from src folder. ' +
             'Eg: python tools/bisect-builds.py -g 280588 -b 280590' +
             '--archive linux64 --use-local-repo')
      sys.exit(1)

  def GetSVNRevisionFromGitHash(self, git_sha1, depot='chromium'):
    if not self.use_local_repo:
      return self._GetSVNRevisionFromGitHashWithoutGitCheckout(git_sha1, depot)
    else:
      return self._GetSVNRevisionFromGitHashFromGitCheckout(git_sha1, depot)

  def GetRevList(self):
    """Gets the list of revision numbers between self.good_revision and
    self.bad_revision."""
    # Download the revlist and filter for just the range between good and bad.
    minrev = min(self.good_revision, self.bad_revision)
    maxrev = max(self.good_revision, self.bad_revision)
    revlist_all = map(int, self.ParseDirectoryIndex())

    revlist = [x for x in revlist_all if x >= int(minrev) and x <= int(maxrev)]
    revlist.sort()

    # Set good and bad revisions to be legit revisions.
    if revlist:
      if self.good_revision < self.bad_revision:
        self.good_revision = revlist[0]
        self.bad_revision = revlist[-1]
      else:
        self.bad_revision = revlist[0]
        self.good_revision = revlist[-1]

      # Fix chromium rev so that the deps blink revision matches REVISIONS file.
      if self.base_url == WEBKIT_BASE_URL:
        revlist_all.sort()
        self.good_revision = FixChromiumRevForBlink(revlist,
                                                    revlist_all,
                                                    self,
                                                    self.good_revision)
        self.bad_revision = FixChromiumRevForBlink(revlist,
                                                   revlist_all,
                                                   self,
                                                   self.bad_revision)
    return revlist

  def GetOfficialBuildsList(self):
    """Gets the list of official build numbers between self.good_revision and
    self.bad_revision."""
    # Download the revlist and filter for just the range between good and bad.
    minrev = min(self.good_revision, self.bad_revision)
    maxrev = max(self.good_revision, self.bad_revision)
    handle = urllib.urlopen(OFFICIAL_BASE_URL)
    dirindex = handle.read()
    handle.close()
    build_numbers = re.findall(r'<a href="([0-9][0-9].*)/">', dirindex)
    final_list = []
    i = 0
    parsed_build_numbers = [LooseVersion(x) for x in build_numbers]
    for build_number in sorted(parsed_build_numbers):
      path = (OFFICIAL_BASE_URL + '/' + str(build_number) + '/' +
              self._listing_platform_dir + self.archive_name)
      i = i + 1
      try:
        connection = urllib.urlopen(path)
        connection.close()
        if build_number > maxrev:
          break
        if build_number >= minrev:
          # If we are bisecting Aura, we want to include only builds which
          # ends with ".1".
          if self.is_aura:
            if self.IsAuraBuild(str(build_number)):
              final_list.append(str(build_number))
          # If we are bisecting only official builds (without --aura),
          # we can not include builds which ends with '.1' or '.2' since
          # they have different folder hierarchy inside.
          elif (not self.IsAuraBuild(str(build_number)) and
                not self.IsASANBuild(str(build_number))):
            final_list.append(str(build_number))
      except urllib.HTTPError:
        pass
    return final_list

def UnzipFilenameToDir(filename, directory):
  """Unzip |filename| to |directory|."""
  cwd = os.getcwd()
  if not os.path.isabs(filename):
    filename = os.path.join(cwd, filename)
  zf = zipfile.ZipFile(filename)
  # Make base.
  if not os.path.isdir(directory):
    os.mkdir(directory)
  os.chdir(directory)
  # Extract files.
  for info in zf.infolist():
    name = info.filename
    if name.endswith('/'):  # dir
      if not os.path.isdir(name):
        os.makedirs(name)
    else:  # file
      directory = os.path.dirname(name)
      if not os.path.isdir(directory):
        os.makedirs(directory)
      out = open(name, 'wb')
      out.write(zf.read(name))
      out.close()
    # Set permissions. Permission info in external_attr is shifted 16 bits.
    os.chmod(name, info.external_attr >> 16L)
  os.chdir(cwd)


def FetchRevision(context, rev, filename, quit_event=None, progress_event=None):
  """Downloads and unzips revision |rev|.
  @param context A PathContext instance.
  @param rev The Chromium revision number/tag to download.
  @param filename The destination for the downloaded file.
  @param quit_event A threading.Event which will be set by the master thread to
                    indicate that the download should be aborted.
  @param progress_event A threading.Event which will be set by the master thread
                    to indicate that the progress of the download should be
                    displayed.
  """
  def ReportHook(blocknum, blocksize, totalsize):
    if quit_event and quit_event.isSet():
      raise RuntimeError('Aborting download of revision %s' % str(rev))
    if progress_event and progress_event.isSet():
      size = blocknum * blocksize
      if totalsize == -1:  # Total size not known.
        progress = 'Received %d bytes' % size
      else:
        size = min(totalsize, size)
        progress = 'Received %d of %d bytes, %.2f%%' % (
            size, totalsize, 100.0 * size / totalsize)
      # Send a \r to let all progress messages use just one line of output.
      sys.stdout.write('\r' + progress)
      sys.stdout.flush()

  download_url = context.GetDownloadURL(rev)
  try:
    urllib.urlretrieve(download_url, filename, ReportHook)
    if progress_event and progress_event.isSet():
      print
  except RuntimeError:
    pass


def RunRevision(context, revision, zip_file, profile, num_runs, command, args):
  """Given a zipped revision, unzip it and run the test."""
  print 'Trying revision %s...' % str(revision)

  # Create a temp directory and unzip the revision into it.
  cwd = os.getcwd()
  tempdir = tempfile.mkdtemp(prefix='bisect_tmp')
  UnzipFilenameToDir(zip_file, tempdir)
  os.chdir(tempdir)

  # Run the build as many times as specified.
  testargs = ['--user-data-dir=%s' % profile] + args
  # The sandbox must be run as root on Official Chrome, so bypass it.
  if ((context.is_official or context.flash_path or context.pdf_path) and
      context.platform.startswith('linux')):
    testargs.append('--no-sandbox')
  if context.flash_path:
    testargs.append('--ppapi-flash-path=%s' % context.flash_path)
    # We have to pass a large enough Flash version, which currently needs not
    # be correct. Instead of requiring the user of the script to figure out and
    # pass the correct version we just spoof it.
    testargs.append('--ppapi-flash-version=99.9.999.999')

  # TODO(vitalybuka): Remove in the future. See crbug.com/395687.
  if context.pdf_path:
    shutil.copy(context.pdf_path, os.path.dirname(context.GetLaunchPath()))
    testargs.append('--enable-print-preview')

  runcommand = []
  for token in shlex.split(command):
    if token == '%a':
      runcommand.extend(testargs)
    else:
      runcommand.append(
          token.replace('%p',os.path.abspath(context.GetLaunchPath())).replace(
              '%s', ' '.join(testargs)))

  results = []
  for _ in range(num_runs):
    subproc = subprocess.Popen(runcommand,
                               bufsize=-1,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    (stdout, stderr) = subproc.communicate()
    results.append((subproc.returncode, stdout, stderr))

  os.chdir(cwd)
  try:
    shutil.rmtree(tempdir, True)
  except Exception:
    pass

  for (returncode, stdout, stderr) in results:
    if returncode:
      return (returncode, stdout, stderr)
  return results[0]


# The arguments official_builds, status, stdout and stderr are unused.
# They are present here because this function is passed to Bisect which then
# calls it with 5 arguments.
# pylint: disable=W0613
def AskIsGoodBuild(rev, official_builds, status, stdout, stderr):
  """Asks the user whether build |rev| is good or bad."""
  # Loop until we get a response that we can parse.
  while True:
    response = raw_input('Revision %s is '
                         '[(g)ood/(b)ad/(r)etry/(u)nknown/(q)uit]: ' %
                         str(rev))
    if response and response in ('g', 'b', 'r', 'u'):
      return response
    if response and response == 'q':
      raise SystemExit()


class DownloadJob(object):
  """DownloadJob represents a task to download a given Chromium revision."""

  def __init__(self, context, name, rev, zip_file):
    super(DownloadJob, self).__init__()
    # Store off the input parameters.
    self.context = context
    self.name = name
    self.rev = rev
    self.zip_file = zip_file
    self.quit_event = threading.Event()
    self.progress_event = threading.Event()
    self.thread = None

  def Start(self):
    """Starts the download."""
    fetchargs = (self.context,
                 self.rev,
                 self.zip_file,
                 self.quit_event,
                 self.progress_event)
    self.thread = threading.Thread(target=FetchRevision,
                                   name=self.name,
                                   args=fetchargs)
    self.thread.start()

  def Stop(self):
    """Stops the download which must have been started previously."""
    assert self.thread, 'DownloadJob must be started before Stop is called.'
    self.quit_event.set()
    self.thread.join()
    os.unlink(self.zip_file)

  def WaitFor(self):
    """Prints a message and waits for the download to complete. The download
    must have been started previously."""
    assert self.thread, 'DownloadJob must be started before WaitFor is called.'
    print 'Downloading revision %s...' % str(self.rev)
    self.progress_event.set()  # Display progress of download.
    self.thread.join()


def Bisect(base_url,
           platform,
           official_builds,
           is_aura,
           use_local_repo,
           good_rev=0,
           bad_rev=0,
           num_runs=1,
           command='%p %a',
           try_args=(),
           profile=None,
           flash_path=None,
           pdf_path=None,
           interactive=True,
           evaluate=AskIsGoodBuild):
  """Given known good and known bad revisions, run a binary search on all
  archived revisions to determine the last known good revision.

  @param platform Which build to download/run ('mac', 'win', 'linux64', etc.).
  @param official_builds Specify build type (Chromium or Official build).
  @param good_rev Number/tag of the known good revision.
  @param bad_rev Number/tag of the known bad revision.
  @param num_runs Number of times to run each build for asking good/bad.
  @param try_args A tuple of arguments to pass to the test application.
  @param profile The name of the user profile to run with.
  @param interactive If it is false, use command exit code for good or bad
                     judgment of the argument build.
  @param evaluate A function which returns 'g' if the argument build is good,
                  'b' if it's bad or 'u' if unknown.

  Threading is used to fetch Chromium revisions in the background, speeding up
  the user's experience. For example, suppose the bounds of the search are
  good_rev=0, bad_rev=100. The first revision to be checked is 50. Depending on
  whether revision 50 is good or bad, the next revision to check will be either
  25 or 75. So, while revision 50 is being checked, the script will download
  revisions 25 and 75 in the background. Once the good/bad verdict on rev 50 is
  known:

    - If rev 50 is good, the download of rev 25 is cancelled, and the next test
      is run on rev 75.

    - If rev 50 is bad, the download of rev 75 is cancelled, and the next test
      is run on rev 25.
  """

  if not profile:
    profile = 'profile'

  context = PathContext(base_url, platform, good_rev, bad_rev,
                        official_builds, is_aura, use_local_repo, flash_path,
                        pdf_path)
  cwd = os.getcwd()

  print 'Downloading list of known revisions...'
  _GetDownloadPath = lambda rev: os.path.join(cwd,
      '%s-%s' % (str(rev), context.archive_name))
  if official_builds:
    revlist = context.GetOfficialBuildsList()
  else:
    revlist = context.GetRevList()

  # Get a list of revisions to bisect across.
  if len(revlist) < 2:  # Don't have enough builds to bisect.
    msg = 'We don\'t have enough builds to bisect. revlist: %s' % revlist
    raise RuntimeError(msg)

  # Figure out our bookends and first pivot point; fetch the pivot revision.
  minrev = 0
  maxrev = len(revlist) - 1
  pivot = maxrev / 2
  rev = revlist[pivot]
  zip_file = _GetDownloadPath(rev)
  fetch = DownloadJob(context, 'initial_fetch', rev, zip_file)
  fetch.Start()
  fetch.WaitFor()

  # Binary search time!
  while fetch and fetch.zip_file and maxrev - minrev > 1:
    if bad_rev < good_rev:
      min_str, max_str = 'bad', 'good'
    else:
      min_str, max_str = 'good', 'bad'
    print 'Bisecting range [%s (%s), %s (%s)].' % (revlist[minrev], min_str,
                                                   revlist[maxrev], max_str)

    # Pre-fetch next two possible pivots
    #   - down_pivot is the next revision to check if the current revision turns
    #     out to be bad.
    #   - up_pivot is the next revision to check if the current revision turns
    #     out to be good.
    down_pivot = int((pivot - minrev) / 2) + minrev
    down_fetch = None
    if down_pivot != pivot and down_pivot != minrev:
      down_rev = revlist[down_pivot]
      down_fetch = DownloadJob(context, 'down_fetch', down_rev,
                               _GetDownloadPath(down_rev))
      down_fetch.Start()

    up_pivot = int((maxrev - pivot) / 2) + pivot
    up_fetch = None
    if up_pivot != pivot and up_pivot != maxrev:
      up_rev = revlist[up_pivot]
      up_fetch = DownloadJob(context, 'up_fetch', up_rev,
                             _GetDownloadPath(up_rev))
      up_fetch.Start()

    # Run test on the pivot revision.
    status = None
    stdout = None
    stderr = None
    try:
      (status, stdout, stderr) = RunRevision(context,
                                             rev,
                                             fetch.zip_file,
                                             profile,
                                             num_runs,
                                             command,
                                             try_args)
    except Exception, e:
      print >> sys.stderr, e

    # Call the evaluate function to see if the current revision is good or bad.
    # On that basis, kill one of the background downloads and complete the
    # other, as described in the comments above.
    try:
      if not interactive:
        if status:
          answer = 'b'
          print 'Bad revision: %s' % rev
        else:
          answer = 'g'
          print 'Good revision: %s' % rev
      else:
        answer = evaluate(rev, official_builds, status, stdout, stderr)
      if ((answer == 'g' and good_rev < bad_rev)
          or (answer == 'b' and bad_rev < good_rev)):
        fetch.Stop()
        minrev = pivot
        if down_fetch:
          down_fetch.Stop()  # Kill the download of the older revision.
          fetch = None
        if up_fetch:
          up_fetch.WaitFor()
          pivot = up_pivot
          fetch = up_fetch
      elif ((answer == 'b' and good_rev < bad_rev)
            or (answer == 'g' and bad_rev < good_rev)):
        fetch.Stop()
        maxrev = pivot
        if up_fetch:
          up_fetch.Stop()  # Kill the download of the newer revision.
          fetch = None
        if down_fetch:
          down_fetch.WaitFor()
          pivot = down_pivot
          fetch = down_fetch
      elif answer == 'r':
        pass  # Retry requires no changes.
      elif answer == 'u':
        # Nuke the revision from the revlist and choose a new pivot.
        fetch.Stop()
        revlist.pop(pivot)
        maxrev -= 1  # Assumes maxrev >= pivot.

        if maxrev - minrev > 1:
          # Alternate between using down_pivot or up_pivot for the new pivot
          # point, without affecting the range. Do this instead of setting the
          # pivot to the midpoint of the new range because adjacent revisions
          # are likely affected by the same issue that caused the (u)nknown
          # response.
          if up_fetch and down_fetch:
            fetch = [up_fetch, down_fetch][len(revlist) % 2]
          elif up_fetch:
            fetch = up_fetch
          else:
            fetch = down_fetch
          fetch.WaitFor()
          if fetch == up_fetch:
            pivot = up_pivot - 1  # Subtracts 1 because revlist was resized.
          else:
            pivot = down_pivot
          zip_file = fetch.zip_file

        if down_fetch and fetch != down_fetch:
          down_fetch.Stop()
        if up_fetch and fetch != up_fetch:
          up_fetch.Stop()
      else:
        assert False, 'Unexpected return value from evaluate(): ' + answer
    except SystemExit:
      print 'Cleaning up...'
      for f in [_GetDownloadPath(revlist[down_pivot]),
                _GetDownloadPath(revlist[up_pivot])]:
        try:
          os.unlink(f)
        except OSError:
          pass
      sys.exit(0)

    rev = revlist[pivot]

  return (revlist[minrev], revlist[maxrev])


def GetBlinkDEPSRevisionForChromiumRevision(rev):
  """Returns the blink revision that was in REVISIONS file at
  chromium revision |rev|."""
  # . doesn't match newlines without re.DOTALL, so this is safe.
  blink_re = re.compile(r'webkit_revision\D*(\d+)')
  url = urllib.urlopen(DEPS_FILE % rev)
  m = blink_re.search(url.read())
  url.close()
  if m:
    return int(m.group(1))
  else:
    raise Exception('Could not get Blink revision for Chromium rev %d' % rev)


def GetBlinkRevisionForChromiumRevision(self, rev):
  """Returns the blink revision that was in REVISIONS file at
  chromium revision |rev|."""
  def _IsRevisionNumber(revision):
    if isinstance(revision, int):
      return True
    else:
      return revision.isdigit()
  if str(rev) in self.githash_svn_dict:
    rev = self.githash_svn_dict[str(rev)]
  file_url = '%s/%s%s/REVISIONS' % (self.base_url,
                                    self._listing_platform_dir, rev)
  url = urllib.urlopen(file_url)
  data = json.loads(url.read())
  url.close()
  if 'webkit_revision' in data:
    blink_rev = data['webkit_revision']
    if not _IsRevisionNumber(blink_rev):
      blink_rev = self.GetSVNRevisionFromGitHash(blink_rev, 'blink')
    return blink_rev
  else:
    raise Exception('Could not get blink revision for cr rev %d' % rev)


def FixChromiumRevForBlink(revisions_final, revisions, self, rev):
  """Returns the chromium revision that has the correct blink revision
  for blink bisect, DEPS and REVISIONS file might not match since
  blink snapshots point to tip of tree blink.
  Note: The revisions_final variable might get modified to include
  additional revisions."""
  blink_deps_rev = GetBlinkDEPSRevisionForChromiumRevision(rev)

  while (GetBlinkRevisionForChromiumRevision(self, rev) > blink_deps_rev):
    idx = revisions.index(rev)
    if idx > 0:
      rev = revisions[idx-1]
      if rev not in revisions_final:
        revisions_final.insert(0, rev)

  revisions_final.sort()
  return rev


def GetChromiumRevision(context, url):
  """Returns the chromium revision read from given URL."""
  try:
    # Location of the latest build revision number
    latest_revision = urllib.urlopen(url).read()
    if latest_revision.isdigit():
      return int(latest_revision)
    return context.GetSVNRevisionFromGitHash(latest_revision)
  except Exception:
    print 'Could not determine latest revision. This could be bad...'
    return 999999999


def main():
  usage = ('%prog [options] [-- chromium-options]\n'
           'Perform binary search on the snapshot builds to find a minimal\n'
           'range of revisions where a behavior change happened. The\n'
           'behaviors are described as "good" and "bad".\n'
           'It is NOT assumed that the behavior of the later revision is\n'
           'the bad one.\n'
           '\n'
           'Revision numbers should use\n'
           '  Official versions (e.g. 1.0.1000.0) for official builds. (-o)\n'
           '  SVN revisions (e.g. 123456) for chromium builds, from trunk.\n'
           '    Use base_trunk_revision from http://omahaproxy.appspot.com/\n'
           '    for earlier revs.\n'
           '    Chrome\'s about: build number and omahaproxy branch_revision\n'
           '    are incorrect, they are from branches.\n'
           '\n'
           'Tip: add "-- --no-first-run" to bypass the first run prompts.')
  parser = optparse.OptionParser(usage=usage)
  # Strangely, the default help output doesn't include the choice list.
  choices = ['mac', 'win', 'linux', 'linux64', 'linux-arm']
            # linux-chromiumos lacks a continuous archive http://crbug.com/78158
  parser.add_option('-a', '--archive',
                    choices=choices,
                    help='The buildbot archive to bisect [%s].' %
                         '|'.join(choices))
  parser.add_option('-o',
                    action='store_true',
                    dest='official_builds',
                    help='Bisect across official Chrome builds (internal '
                         'only) instead of Chromium archives.')
  parser.add_option('-b', '--bad',
                    type='str',
                    help='A bad revision to start bisection. '
                         'May be earlier or later than the good revision. '
                         'Default is HEAD.')
  parser.add_option('-f', '--flash_path',
                    type='str',
                    help='Absolute path to a recent Adobe Pepper Flash '
                         'binary to be used in this bisection (e.g. '
                         'on Windows C:\...\pepflashplayer.dll and on Linux '
                         '/opt/google/chrome/PepperFlash/'
                         'libpepflashplayer.so).')
  parser.add_option('-d', '--pdf_path',
                    type='str',
                    help='Absolute path to a recent PDF plugin '
                         'binary to be used in this bisection (e.g. '
                         'on Windows C:\...\pdf.dll and on Linux '
                         '/opt/google/chrome/libpdf.so). Option also enables '
                         'print preview.')
  parser.add_option('-g', '--good',
                    type='str',
                    help='A good revision to start bisection. ' +
                         'May be earlier or later than the bad revision. ' +
                         'Default is 0.')
  parser.add_option('-p', '--profile', '--user-data-dir',
                    type='str',
                    default='profile',
                    help='Profile to use; this will not reset every run. '
                         'Defaults to a clean profile.')
  parser.add_option('-t', '--times',
                    type='int',
                    default=1,
                    help='Number of times to run each build before asking '
                         'if it\'s good or bad. Temporary profiles are reused.')
  parser.add_option('-c', '--command',
                    type='str',
                    default='%p %a',
                    help='Command to execute. %p and %a refer to Chrome '
                         'executable and specified extra arguments '
                         'respectively. Use %s to specify all extra arguments '
                         'as one string. Defaults to "%p %a". Note that any '
                         'extra paths specified should be absolute.')
  parser.add_option('-l', '--blink',
                    action='store_true',
                    help='Use Blink bisect instead of Chromium. ')
  parser.add_option('', '--not-interactive',
                    action='store_true',
                    default=False,
                    help='Use command exit code to tell good/bad revision.')
  parser.add_option('--aura',
                    dest='aura',
                    action='store_true',
                    default=False,
                    help='Allow the script to bisect aura builds')
  parser.add_option('--use-local-repo',
                    dest='use_local_repo',
                    action='store_true',
                    default=False,
                    help='Allow the script to convert git SHA1 to SVN '
                         'revision using "git svn find-rev <SHA1>" '
                         'command from a Chromium checkout.')

  (opts, args) = parser.parse_args()

  if opts.archive is None:
    print 'Error: missing required parameter: --archive'
    print
    parser.print_help()
    return 1

  if opts.aura:
    if opts.archive != 'win' or not opts.official_builds:
      print ('Error: Aura is supported only on Windows platform '
             'and official builds.')
      return 1

  if opts.blink:
    base_url = WEBKIT_BASE_URL
  else:
    base_url = CHROMIUM_BASE_URL

  # Create the context. Initialize 0 for the revisions as they are set below.
  context = PathContext(base_url, opts.archive, 0, 0,
                        opts.official_builds, opts.aura, opts.use_local_repo,
                        None)
  # Pick a starting point, try to get HEAD for this.
  if opts.bad:
    bad_rev = opts.bad
  else:
    bad_rev = '999.0.0.0'
    if not opts.official_builds:
      bad_rev = GetChromiumRevision(context, context.GetLastChangeURL())

  # Find out when we were good.
  if opts.good:
    good_rev = opts.good
  else:
    good_rev = '0.0.0.0' if opts.official_builds else 0

  if opts.flash_path:
    flash_path = opts.flash_path
    msg = 'Could not find Flash binary at %s' % flash_path
    assert os.path.exists(flash_path), msg

  if opts.pdf_path:
    pdf_path = opts.pdf_path
    msg = 'Could not find PDF binary at %s' % pdf_path
    assert os.path.exists(pdf_path), msg

  if opts.official_builds:
    good_rev = LooseVersion(good_rev)
    bad_rev = LooseVersion(bad_rev)
  else:
    good_rev = int(good_rev)
    bad_rev = int(bad_rev)

  if opts.times < 1:
    print('Number of times to run (%d) must be greater than or equal to 1.' %
          opts.times)
    parser.print_help()
    return 1

  (min_chromium_rev, max_chromium_rev) = Bisect(
      base_url, opts.archive, opts.official_builds, opts.aura,
      opts.use_local_repo, good_rev, bad_rev, opts.times, opts.command,
      args, opts.profile, opts.flash_path, opts.pdf_path,
      not opts.not_interactive)

  # Get corresponding blink revisions.
  try:
    min_blink_rev = GetBlinkRevisionForChromiumRevision(context,
                                                        min_chromium_rev)
    max_blink_rev = GetBlinkRevisionForChromiumRevision(context,
                                                        max_chromium_rev)
  except Exception:
    # Silently ignore the failure.
    min_blink_rev, max_blink_rev = 0, 0

  if opts.blink:
    # We're done. Let the user know the results in an official manner.
    if good_rev > bad_rev:
      print DONE_MESSAGE_GOOD_MAX % (str(min_blink_rev), str(max_blink_rev))
    else:
      print DONE_MESSAGE_GOOD_MIN % (str(min_blink_rev), str(max_blink_rev))

    print 'BLINK CHANGELOG URL:'
    print '  ' + BLINK_CHANGELOG_URL % (max_blink_rev, min_blink_rev)

  else:
    # We're done. Let the user know the results in an official manner.
    if good_rev > bad_rev:
      print DONE_MESSAGE_GOOD_MAX % (str(min_chromium_rev),
                                     str(max_chromium_rev))
    else:
      print DONE_MESSAGE_GOOD_MIN % (str(min_chromium_rev),
                                     str(max_chromium_rev))
    if min_blink_rev != max_blink_rev:
      print ('NOTE: There is a Blink roll in the range, '
             'you might also want to do a Blink bisect.')

    print 'CHANGELOG URL:'
    if opts.official_builds:
      print OFFICIAL_CHANGELOG_URL % (min_chromium_rev, max_chromium_rev)
    else:
      print '  ' + CHANGELOG_URL % (min_chromium_rev, max_chromium_rev)


if __name__ == '__main__':
  sys.exit(main())
