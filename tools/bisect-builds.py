#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Snapshot Build Bisect Tool

This script bisects a snapshot archive using binary search. It starts at
a bad revision (it will try to guess HEAD) and asks for a last known-good
revision. It will then binary search across this revision range by downloading,
unzipping, and opening Chromium for you. After testing the specific revision,
it will ask you whether it is good or bad before continuing the search.
"""

# The root URL for storage.
BASE_URL = 'http://commondatastorage.googleapis.com/chromium-browser-snapshots'

# URL to the ViewVC commit page.
BUILD_VIEWVC_URL = 'http://src.chromium.org/viewvc/chrome?view=rev&revision=%d'

# Changelogs URL.
CHANGELOG_URL = 'http://build.chromium.org/f/chromium/' \
                'perf/dashboard/ui/changelog.html?url=/trunk/src&range=%d:%d'

# DEPS file URL.
DEPS_FILE= 'http://src.chromium.org/viewvc/chrome/trunk/src/DEPS?revision=%d'

# WebKit Changelogs URL.
WEBKIT_CHANGELOG_URL = 'http://trac.webkit.org/log/' \
                       'trunk/?rev=%d&stop_rev=%d&verbose=on'

###############################################################################

import math
import optparse
import os
import pipes
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import urllib
from xml.etree import ElementTree
import zipfile


class PathContext(object):
  """A PathContext is used to carry the information used to construct URLs and
  paths when dealing with the storage server and archives."""
  def __init__(self, platform, good_revision, bad_revision):
    super(PathContext, self).__init__()
    # Store off the input parameters.
    self.platform = platform  # What's passed in to the '-a/--archive' option.
    self.good_revision = good_revision
    self.bad_revision = bad_revision

    # The name of the ZIP file in a revision directory on the server.
    self.archive_name = None

    # Set some internal members:
    #   _listing_platform_dir = Directory that holds revisions. Ends with a '/'.
    #   _archive_extract_dir = Uncompressed directory in the archive_name file.
    #   _binary_name = The name of the executable to run.
    if self.platform == 'linux' or self.platform == 'linux64':
      self._listing_platform_dir = 'Linux/'
      self.archive_name = 'chrome-linux.zip'
      self._archive_extract_dir = 'chrome-linux'
      self._binary_name = 'chrome'
      # Linux and x64 share all the same path data except for the archive dir.
      if self.platform == 'linux64':
        self._listing_platform_dir = 'Linux_x64/'
    elif self.platform == 'mac':
      self._listing_platform_dir = 'Mac/'
      self.archive_name = 'chrome-mac.zip'
      self._archive_extract_dir = 'chrome-mac'
      self._binary_name = 'Chromium.app/Contents/MacOS/Chromium'
    elif self.platform == 'win':
      self._listing_platform_dir = 'Win/'
      self.archive_name = 'chrome-win32.zip'
      self._archive_extract_dir = 'chrome-win32'
      self._binary_name = 'chrome.exe'
    else:
      raise Exception('Invalid platform: %s' % self.platform)

  def GetListingURL(self, marker=None):
    """Returns the URL for a directory listing, with an optional marker."""
    marker_param = ''
    if marker:
      marker_param = '&marker=' + str(marker)
    return BASE_URL + '/?delimiter=/&prefix=' + self._listing_platform_dir + \
        marker_param

  def GetDownloadURL(self, revision):
    """Gets the download URL for a build archive of a specific revision."""
    return "%s/%s%d/%s" % (
        BASE_URL, self._listing_platform_dir, revision, self.archive_name)

  def GetLastChangeURL(self):
    """Returns a URL to the LAST_CHANGE file."""
    return BASE_URL + '/' + self._listing_platform_dir + 'LAST_CHANGE'

  def GetLaunchPath(self):
    """Returns a relative path (presumably from the archive extraction location)
    that is used to run the executable."""
    return os.path.join(self._archive_extract_dir, self._binary_name)

  def ParseDirectoryIndex(self):
    """Parses the Google Storage directory listing into a list of revision
    numbers. The range starts with self.good_revision and goes until
    self.bad_revision."""

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
        raise Exception("Could not locate end namespace for directory index")
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
      for prefix in all_prefixes:
        revnum = prefix.text[prefix_len:-1]
        try:
          revnum = int(revnum)
          revisions.append(revnum)
        except ValueError:
          pass
      return (revisions, next_marker)

    # Fetch the first list of revisions.
    (revisions, next_marker) = _FetchAndParse(self.GetListingURL())

    # If the result list was truncated, refetch with the next marker. Do this
    # until an entire directory listing is done.
    while next_marker:
      next_url = self.GetListingURL(next_marker)
      (new_revisions, next_marker) = _FetchAndParse(next_url)
      revisions.extend(new_revisions)

    return revisions

  def GetRevList(self):
    """Gets the list of revision numbers between self.good_revision and
    self.bad_revision."""
    # Download the revlist and filter for just the range between good and bad.
    minrev = self.good_revision
    maxrev = self.bad_revision
    revlist = map(int, self.ParseDirectoryIndex())
    revlist = [x for x in revlist if x >= minrev and x <= maxrev]
    revlist.sort()
    return revlist


def UnzipFilenameToDir(filename, dir):
  """Unzip |filename| to directory |dir|."""
  cwd = os.getcwd()
  if not os.path.isabs(filename):
    filename = os.path.join(cwd, filename)
  zf = zipfile.ZipFile(filename)
  # Make base.
  try:
    if not os.path.isdir(dir):
      os.mkdir(dir)
    os.chdir(dir)
    # Extract files.
    for info in zf.infolist():
      name = info.filename
      if name.endswith('/'):  # dir
        if not os.path.isdir(name):
          os.makedirs(name)
      else:  # file
        dir = os.path.dirname(name)
        if not os.path.isdir(dir):
          os.makedirs(dir)
        out = open(name, 'wb')
        out.write(zf.read(name))
        out.close()
      # Set permissions. Permission info in external_attr is shifted 16 bits.
      os.chmod(name, info.external_attr >> 16L)
    os.chdir(cwd)
  except Exception, e:
    print >>sys.stderr, e
    sys.exit(1)


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
     raise RuntimeError("Aborting download of revision %d" % rev)
    if progress_event and progress_event.isSet():
      size = blocknum * blocksize
      if totalsize == -1:  # Total size not known.
        progress = "Received %d bytes" % size
      else:
        size = min(totalsize, size)
        progress = "Received %d of %d bytes, %.2f%%" % (
            size, totalsize, 100.0 * size / totalsize)
      # Send a \r to let all progress messages use just one line of output.
      sys.stdout.write("\r" + progress)
      sys.stdout.flush()

  download_url = context.GetDownloadURL(rev)
  try:
    urllib.urlretrieve(download_url, filename, ReportHook)
    if progress_event and progress_event.isSet():
      print
  except RuntimeError, e:
    pass


def RunRevision(context, revision, zipfile, profile, args):
  """Given a zipped revision, unzip it and run the test."""
  print "Trying revision %d..." % revision

  # Create a temp directory and unzip the revision into it.
  cwd = os.getcwd()
  tempdir = tempfile.mkdtemp(prefix='bisect_tmp')
  UnzipFilenameToDir(zipfile, tempdir)
  os.chdir(tempdir)

  # Run the build.
  testargs = [context.GetLaunchPath(), '--user-data-dir=%s' % profile] + args
  subproc = subprocess.Popen(testargs,
                             bufsize=-1,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
  (stdout, stderr) = subproc.communicate()

  os.chdir(cwd)
  try:
    shutil.rmtree(tempdir, True)
  except Exception, e:
    pass

  return (subproc.returncode, stdout, stderr)


def AskIsGoodBuild(rev, status, stdout, stderr):
  """Ask the user whether build |rev| is good or bad."""
  # Loop until we get a response that we can parse.
  while True:
    response = raw_input('Revision %d is [(g)ood/(b)ad/(q)uit]: ' % int(rev))
    if response and response in ('g', 'b'):
      return response == 'g'
    if response and response == 'q':
      raise SystemExit()


def Bisect(platform,
           good_rev=0,
           bad_rev=0,
           try_args=(),
           profile=None,
           predicate=AskIsGoodBuild):
  """Given known good and known bad revisions, run a binary search on all
  archived revisions to determine the last known good revision.

  @param platform Which build to download/run ('mac', 'win', 'linux64', etc.).
  @param good_rev Number/tag of the last known good revision.
  @param bad_rev Number/tag of the first known bad revision.
  @param try_args A tuple of arguments to pass to the test application.
  @param profile The name of the user profile to run with.
  @param predicate A predicate function which returns True iff the argument
                   chromium revision is good.

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

  context = PathContext(platform, good_rev, bad_rev)
  cwd = os.getcwd()

  _GetDownloadPath = lambda rev: os.path.join(cwd,
      '%d-%s' % (rev, context.archive_name))

  print "Downloading list of known revisions..."

  revlist = context.GetRevList()

  # Get a list of revisions to bisect across.
  if len(revlist) < 2:  # Don't have enough builds to bisect.
    msg = 'We don\'t have enough builds to bisect. revlist: %s' % revlist
    raise RuntimeError(msg)

  # Figure out our bookends and first pivot point; fetch the pivot revision.
  good = 0
  bad = len(revlist) - 1
  pivot = bad / 2
  rev = revlist[pivot]
  zipfile = _GetDownloadPath(rev)
  progress_event = threading.Event()
  progress_event.set()
  print "Downloading revision %d..." % rev
  FetchRevision(context, rev, zipfile,
                quit_event=None, progress_event=progress_event)

  # Binary search time!
  while zipfile and bad - good > 1:
    # Pre-fetch next two possible pivots
    #   - down_pivot is the next revision to check if the current revision turns
    #     out to be bad.
    #   - up_pivot is the next revision to check if the current revision turns
    #     out to be good.
    down_pivot = int((pivot - good) / 2) + good
    down_thread = None
    if down_pivot != pivot and down_pivot != good:
      down_rev = revlist[down_pivot]
      down_zipfile = _GetDownloadPath(down_rev)
      down_quit_event = threading.Event()
      down_progress_event = threading.Event()
      fetchargs = (context,
                   down_rev,
                   down_zipfile,
                   down_quit_event,
                   down_progress_event)
      down_thread = threading.Thread(target=FetchRevision,
                                     name='down_fetch',
                                     args=fetchargs)
      down_thread.start()

    up_pivot = int((bad - pivot) / 2) + pivot
    up_thread = None
    if up_pivot != pivot and up_pivot != bad:
      up_rev = revlist[up_pivot]
      up_zipfile = _GetDownloadPath(up_rev)
      up_quit_event = threading.Event()
      up_progress_event = threading.Event()
      fetchargs = (context,
                   up_rev,
                   up_zipfile,
                   up_quit_event,
                   up_progress_event)
      up_thread = threading.Thread(target=FetchRevision,
                                   name='up_fetch',
                                   args=fetchargs)
      up_thread.start()

    # Run test on the pivot revision.
    (status, stdout, stderr) = RunRevision(context,
                                           rev,
                                           zipfile,
                                           profile,
                                           try_args)
    os.unlink(zipfile)
    zipfile = None

    # Call the predicate function to see if the current revision is good or bad.
    # On that basis, kill one of the background downloads and complete the
    # other, as described in the comments above.
    try:
      if predicate(rev, status, stdout, stderr):
        good = pivot
        if down_thread:
          down_quit_event.set()  # Kill the download of older revision.
          down_thread.join()
          os.unlink(down_zipfile)
        if up_thread:
          print "Downloading revision %d..." % up_rev
          up_progress_event.set()  # Display progress of download.
          up_thread.join()  # Wait for newer revision to finish downloading.
          pivot = up_pivot
          zipfile = up_zipfile
      else:
        bad = pivot
        if up_thread:
          up_quit_event.set()  # Kill download of newer revision.
          up_thread.join()
          os.unlink(up_zipfile)
        if down_thread:
          print "Downloading revision %d..." % down_rev
          down_progress_event.set()  # Display progress of download.
          down_thread.join()  # Wait for older revision to finish downloading.
          pivot = down_pivot
          zipfile = down_zipfile
    except SystemExit:
      print "Cleaning up..."
      for f in [down_zipfile, up_zipfile]:
        try:
          os.unlink(f)
        except OSError:
          pass
      sys.exit(0)

    rev = revlist[pivot]

  return (revlist[good], revlist[bad])


def GetWebKitRevisionForChromiumRevision(rev):
  """Returns the webkit revision that was in chromium's DEPS file at
  chromium revision |rev|."""
  # . doesn't match newlines without re.DOTALL, so this is safe.
  webkit_re = re.compile(r'webkit_revision.:\D*(\d+)')
  url = urllib.urlopen(DEPS_FILE % rev)
  m = webkit_re.search(url.read())
  url.close()
  if m:
    return int(m.group(1))
  else:
    raise Exception('Could not get webkit revision for cr rev %d' % rev)


def main():
  usage = ('%prog [options] [-- chromium-options]\n'
           'Perform binary search on the snapshot builds.\n'
           '\n'
           'Tip: add "-- --no-first-run" to bypass the first run prompts.')
  parser = optparse.OptionParser(usage=usage)
  # Strangely, the default help output doesn't include the choice list.
  choices = ['mac', 'win', 'linux', 'linux64']
            # linux-chromiumos lacks a continuous archive http://crbug.com/78158
  parser.add_option('-a', '--archive',
                    choices = choices,
                    help = 'The buildbot archive to bisect [%s].' %
                           '|'.join(choices))
  parser.add_option('-b', '--bad', type = 'int',
                    help = 'The bad revision to bisect to.')
  parser.add_option('-g', '--good', type = 'int',
                    help = 'The last known good revision to bisect from.')
  parser.add_option('-p', '--profile', '--user-data-dir', type = 'str',
                    help = 'Profile to use; this will not reset every run. ' +
                    'Defaults to a clean profile.', default = 'profile')
  (opts, args) = parser.parse_args()

  if opts.archive is None:
    print 'Error: missing required parameter: --archive'
    print
    parser.print_help()
    return 1

  if opts.bad and opts.good and (opts.good > opts.bad):
    print ('The good revision (%d) must precede the bad revision (%d).\n' %
           (opts.good, opts.bad))
    parser.print_help()
    return 1

  # Create the context. Initialize 0 for the revisions as they are set below.
  context = PathContext(opts.archive, 0, 0)

  # Pick a starting point, try to get HEAD for this.
  if opts.bad:
    bad_rev = opts.bad
  else:
    bad_rev = 0
    try:
      # Location of the latest build revision number
      nh = urllib.urlopen(context.GetLastChangeURL())
      latest = int(nh.read())
      nh.close()
      bad_rev = raw_input('Bad revision [HEAD:%d]: ' % latest)
      if (bad_rev == ''):
        bad_rev = latest
      bad_rev = int(bad_rev)
    except Exception, e:
      print('Could not determine latest revision. This could be bad...')
      bad_rev = int(raw_input('Bad revision: '))

  # Find out when we were good.
  if opts.good:
    good_rev = opts.good
  else:
    good_rev = 0
    try:
      good_rev = int(raw_input('Last known good [0]: '))
    except Exception, e:
      pass

  (last_known_good_rev, first_known_bad_rev) = Bisect(
      opts.archive, good_rev, bad_rev, args, opts.profile)

  # Get corresponding webkit revisions.
  try:
    last_known_good_webkit_rev = GetWebKitRevisionForChromiumRevision(
        last_known_good_rev)
    first_known_bad_webkit_rev = GetWebKitRevisionForChromiumRevision(
        first_known_bad_rev)
  except Exception, e:
    # Silently ignore the failure.
    last_known_good_webkit_rev, first_known_bad_webkit_rev = 0, 0

  # We're done. Let the user know the results in an official manner.
  print('You are probably looking for build %d.' % first_known_bad_rev)
  if last_known_good_webkit_rev != first_known_bad_webkit_rev:
    print 'WEBKIT CHANGELOG URL:'
    print WEBKIT_CHANGELOG_URL % (first_known_bad_webkit_rev,
                                  last_known_good_webkit_rev)
  print 'CHANGELOG URL:'
  print CHANGELOG_URL % (last_known_good_rev, first_known_bad_rev)
  print 'Built at revision:'
  print BUILD_VIEWVC_URL % first_known_bad_rev


if __name__ == '__main__':
  sys.exit(main())
