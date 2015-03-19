# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Upload a single build command stats file to appengine."""

from __future__ import print_function

import re
import sys

from chromite.cbuildbot import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import stats


FILE_LOAD_ERROR = 'Error loading %s'
UNCAUGHT_ERROR = 'Uncaught command stats exception.'


class LoadError(RuntimeError):
  """Error during loading of stats file."""


class StatsLoader(object):
  """Loads stats from a file."""

  @classmethod
  def LoadFile(cls, stat_file):
    """Return a Stats object constructed from contents of |stat_file|."""

    with open(stat_file, 'r') as f:
      first_line = f.readline().rstrip()
      match = re.match(r'Chromium OS .+ Version (\d+)$', first_line)
      if not match:
        raise LoadError('Stats file not in expected format')

      version = int(match.group(1))
      loader = cls._GetLinesLoader(version)
      if not loader:
        raise LoadError('Stats file version %s not supported.' % version)

      return loader(f.readlines())

  @classmethod
  def _GetLinesLoader(cls, version):
    LOADERS = (
        None,
        cls._LoadLinesV1,  # Version 1 loader (at index 1)
    )

    if version < len(LOADERS) and version >= 0:
      return LOADERS[version]

    return None

  @classmethod
  def _LoadLinesV1(cls, stat_lines):
    """Load stat lines in Version 1 format."""
    data = {}
    for line in stat_lines:
      # Each line has following format:
      # attribute_name Rest of line is value for attribute_name
      # Note that some attributes may have no value after their name.
      attr, _sep, value = line.rstrip().partition(' ')
      if not attr:
        attr = line.rstrip()

      data[attr] = value

    return stats.Stats(**data)


def main(argv):
  """Main function."""
  # This is not meant to be a user-friendly script.  It takes one and
  # only one argument, which is a build stats file to be uploaded
  epilog = (
      'This script is not intended to be run manually.  It is used as'
      ' part of the build command statistics project.'
  )
  in_golo = cros_build_lib.GetHostDomain().endswith(constants.GOLO_DOMAIN)
  debug_level = commandline.ArgumentParser.DEFAULT_LOG_LEVEL
  if in_golo:
    debug_level = 'debug'
  parser = commandline.ArgumentParser(
      epilog=epilog, default_log_level=debug_level)
  parser.add_argument(
      'build_stats_file', nargs=1, default=None)
  options = parser.parse_args(argv)

  try:
    cmd_stats = StatsLoader.LoadFile(options.build_stats_file[0])
  except LoadError:
    logging.error(FILE_LOAD_ERROR, options.build_stats_file[0],
                  exc_info=True)
    sys.exit(1)

  try:
    stats.StatsUploader.Upload(cmd_stats)
  except Exception:
    logging.error(UNCAUGHT_ERROR, exc_info=True)
    sys.exit(1)
