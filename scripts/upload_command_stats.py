#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Upload a single build command stats file to appengine."""

import logging
import optparse
import os
import re
import sys
import urllib
import urllib2

from chromite.lib import cros_build_lib
from chromite.lib import git

# To test with an app engine instance on localhost, set envvar
# export CROS_BUILD_STATS_SITE="http://localhost:8080"

PAGE = 'upload_command_stats'
DEFAULT_SITE = 'https://chromiumos-build-stats.appspot.com'
SITE = os.environ.get('CROS_BUILD_STATS_SITE', DEFAULT_SITE)
URL = '%s/%s' % (SITE, PAGE)
UPLOAD_TIMEOUT = 5

DISABLE_FILE = os.path.join(os.environ['HOME'], '.disable_build_stats_upload')

DOMAIN_RE = re.compile(r'(?:^|\.)(?:corp\.google\.com|golo\.chromium\.org)$')
GIT_ID_RE = re.compile(r'(?:@google\.com|@chromium\.org)$')


class StatsError(RuntimeError):
  """Any error during uploading of stats."""


class Stats(object):
  """Manage one set of build command stats."""

  __slots__ = (
    'data',
    'loaders',
    )

  def __init__(self):
    self.data = {}
    self.loaders = [None,
                    Stats._LoadLinesV1,  # Version 1 loader (at index 1)
                    ]

  def LoadFile(self, stat_file):
    """Load command stats file at |stat_file| into |self.data|."""

    with open(stat_file, 'r') as f:
      first_line = f.readline().rstrip()
      match = re.match(r'Chromium OS .+ Version (\d+)$', first_line)
      if not match:
        raise StatsError('Stats file not in expected format')

      version = int(match.group(1))
      loader = self._GetLinesLoader(version)

      if loader:
        loader(self, f.readlines())
      else:
        raise StatsError('Stats file version %s not supported.' % version)

  def _GetLinesLoader(self, version):
    if version < len(self.loaders) and version >= 0:
      return self.loaders[version]

    return None

  def _LoadLinesV1(self, stat_lines):
    """Load stat lines in Version 1 format."""
    self.data = {}

    for line in stat_lines:
      # Each line has following format:
      # attribute_name Rest of line is value for attribute_name
      # Note that some attributes may have no value after their name.
      attr, _sep, value = line.rstrip().partition(' ')
      if not attr:
        attr = line.rstrip()

      self.data[attr] = value

  def Upload(self, url):
    """Upload stats in |self.data| to |url|."""
    logging.info('Uploading command stats to %r', url)
    data = urllib.urlencode(self.data)
    request = urllib2.Request(url)

    try:
      urllib2.urlopen(request, data)
    except (urllib2.HTTPError, urllib2.URLError) as ex:
      raise StatsError('Failed to upload command stats to %r: "%s"' % (url, ex))

def UploadConditionsMet():
  """Return True if upload conditions are met."""

  # Verify that host domain is in golo.chromium.org or corp.google.com.
  domain = cros_build_lib.GetHostDomain()
  if not domain or not DOMAIN_RE.search(domain):
    return False

  # Verify that git user email is from chromium.org or google.com.
  cwd = os.path.dirname(os.path.realpath(__file__))
  git_id = git.GetProjectUserEmail(cwd)
  if not git_id:
    logging.warning('Unable to determine current "git id".  Skipping stats'
                    ' upload.')
    return False
  if not GIT_ID_RE.search(git_id):
    return False

  # Check for presence of file that disables stats upload.
  if os.path.exists(DISABLE_FILE):
    return False

  return True

def main(argv):
  """Main function."""
  # This is not meant to be a user-friendly script.  It takes one and
  # only one argument, which is a build stats file to be uploaded
  usage = 'Usage: %prog [-h|--help] build_stats_file'
  epilog = (
    'This script is not intended to be run manually.  It is used as'
    ' part of the build command statistics project.'
    )

  parser = optparse.OptionParser(usage=usage, epilog=epilog)
  (_options, args) = parser.parse_args(argv)

  logging.getLogger().setLevel(logging.INFO)

  if len(args) > 1:
    logging.error('Only one argument permitted.')
    sys.exit(1)
  if len(args) < 1:
    logging.error('Missing command stats file argument.')
    sys.exit(1)

  # Silently do nothing if the conditions for uploading are not met.
  if UploadConditionsMet():
    try:
      stats = Stats()
      stats.LoadFile(args[0])

      with cros_build_lib.SubCommandTimeout(UPLOAD_TIMEOUT):
        stats.Upload(URL)

    except cros_build_lib.TimeoutError:
      logging.error('Timed out during upload - waited %i seconds',
                    UPLOAD_TIMEOUT)
      sys.exit(1)

    except StatsError as ex:
      logging.error(ex)
      sys.exit(1)
