#!/usr/bin/python
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
BASE_URL = 'http://commondatastorage.googleapis.com/chromium-browser-continuous'

# URL to the ViewVC commit page.
BUILD_VIEWVC_URL = 'http://src.chromium.org/viewvc/chrome?view=rev&revision=%d'

# Changelogs URL.
CHANGELOG_URL = 'http://build.chromium.org/f/chromium/' \
                'perf/dashboard/ui/changelog.html?url=/trunk/src&range=%d:%d'

###############################################################################

import math
import optparse
import os
import pipes
import re
import shutil
import sys
import tempfile
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
    if self.platform == 'linux' or self.platform == 'linux-64':
      self._listing_platform_dir = 'Linux/'
      self.archive_name = 'chrome-linux.zip'
      self._archive_extract_dir = 'chrome-linux'
      self._binary_name = 'chrome'
      # Linux and x64 share all the same path data except for the archive dir.
      if self.platform == 'linux-64':
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
      raise Exception("Invalid platform")

  def GetListingURL(self, marker=None):
    """Returns the URL for a directory listing, with an optional marker."""
    marker_param = ''
    if marker:
      marker_param = '&marker=' + str(marker)
    return BASE_URL + '/?delimiter=/&prefix=' + self._listing_platform_dir + \
        marker_param

  def GetDownloadURL(self, revision):
    """Gets the download URL for a build archive of a specific revision."""
    return BASE_URL + '/' + self._listing_platform_dir + str(revision) + '/' + \
        self.archive_name

  def GetLastChangeURL(self):
    """Returns a URL to the LAST_CHANGE file."""
    return BASE_URL + '/' + self._listing_platform_dir + 'LAST_CHANGE'

  def GetLaunchPath(self):
    """Returns a relative path (presumably from the archive extraction location)
    that is used to run the executable."""
    return os.path.join(self._archive_extract_dir, self._binary_name)


def UnzipFilenameToDir(filename, dir):
  """Unzip |filename| to directory |dir|."""
  zf = zipfile.ZipFile(filename)
  # Make base.
  pushd = os.getcwd()
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
    os.chdir(pushd)
  except Exception, e:
    print >>sys.stderr, e
    sys.exit(1)


def ParseDirectoryIndex(context):
  """Parses the Google Storage directory listing into a list of revision
  numbers. The range starts with context.good_revision and goes until the latest
  revision."""
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
    prefix = document.find(namespace + 'Prefix').text
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
    revisions = map(lambda x: x.text[len(prefix):-1], all_prefixes)
    return (revisions, next_marker)

  # Fetch the first list of revisions.
  (revisions, next_marker) = _FetchAndParse(context.GetListingURL())
  # If the result list was truncated, refetch with the next marker. Do this
  # until an entire directory listing is done.
  while next_marker:
    (new_revisions, next_marker) = _FetchAndParse(
        context.GetListingURL(next_marker))
    revisions.extend(new_revisions)

  return revisions


def GetRevList(context):
  """Gets the list of revision numbers between |good_revision| and
  |bad_revision| of the |context|."""
  # Download the revlist and filter for just the range between good and bad.
  rev_range = range(context.good_revision, context.bad_revision)
  revlist = map(int, ParseDirectoryIndex(context))
  revlist = filter(lambda r: r in rev_range, revlist)
  revlist.sort()
  return revlist


def TryRevision(context, rev, profile, args):
  """Downloads revision |rev|, unzips it, and opens it for the user to test.
  |profile| is the profile to use."""
  # Do this in a temp dir so we don't collide with user files.
  cwd = os.getcwd()
  tempdir = tempfile.mkdtemp(prefix='bisect_tmp')
  os.chdir(tempdir)

  # Download the file.
  download_url = context.GetDownloadURL(rev)
  def _ReportHook(blocknum, blocksize, totalsize):
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
  try:
    print 'Fetching ' + download_url
    urllib.urlretrieve(download_url, context.archive_name, _ReportHook)
    print
  except Exception, e:
    print('Could not retrieve the download. Sorry.')
    sys.exit(-1)

  # Unzip the file.
  print 'Unzipping ...'
  UnzipFilenameToDir(context.archive_name, os.curdir)

  # Tell the system to open the app.
  args = ['--user-data-dir=%s' % profile] + args
  flags = ' '.join(map(pipes.quote, args))
  cmd = '%s %s' % (context.GetLaunchPath(), flags)
  print 'Running %s' % cmd
  os.system(cmd)

  os.chdir(cwd)
  print 'Cleaning temp dir ...'
  try:
    shutil.rmtree(tempdir, True)
  except Exception, e:
    pass


def AskIsGoodBuild(rev):
  """Ask the user whether build |rev| is good or bad."""
  # Loop until we get a response that we can parse.
  while True:
    response = raw_input('\nBuild %d is [(g)ood/(b)ad]: ' % int(rev))
    if response and response in ('g', 'b'):
      return response == 'g'


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
                    'Defaults to a clean profile.')
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

  # Set the input parameters now that they've been validated.
  context.good_revision = good_rev
  context.bad_revision = bad_rev

  # Get a list of revisions to bisect across.
  revlist = GetRevList(context)
  if len(revlist) < 2:  # Don't have enough builds to bisect
    print 'We don\'t have enough builds to bisect. revlist: %s' % revlist
    sys.exit(1)

  # If we don't have a |good_rev|, set it to be the first revision possible.
  if good_rev == 0:
    good_rev = revlist[0]

  # These are indexes of |revlist|.
  good = 0
  bad = len(revlist) - 1
  last_known_good_rev = revlist[good]

  # Binary search time!
  while good < bad:
    candidates = revlist[good:bad]
    num_poss = len(candidates)
    if num_poss > 10:
      print('%d candidates. %d tries left.' %
          (num_poss, round(math.log(num_poss, 2))))
    else:
      print('Candidates: %s' % revlist[good:bad])

    # Cut the problem in half...
    test = int((bad - good) / 2) + good
    test_rev = revlist[test]

    # Let the user give this rev a spin (in her own profile, if she wants).
    profile = opts.profile
    if not profile:
      profile = 'profile'  # In a temp dir.
    TryRevision(context, test_rev, profile, args)
    if AskIsGoodBuild(test_rev):
      last_known_good_rev = revlist[good]
      good = test + 1
    else:
      bad = test

  # We're done. Let the user know the results in an official manner.
  print('You are probably looking for build %d.' % revlist[bad])
  print('CHANGELOG URL:')
  print(CHANGELOG_URL % (last_known_good_rev, revlist[bad]))
  print('Built at revision:')
  print(BUILD_VIEWVC_URL % revlist[bad])

if __name__ == '__main__':
  sys.exit(main())
