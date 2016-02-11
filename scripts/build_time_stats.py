# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that shows build timing for a build, and it's stages.

This script shows how long a build took, how long each stage took, and when each
stage started relative to the start of the build.
"""

from __future__ import print_function

import datetime

from chromite.cbuildbot import constants
from chromite.lib import build_time_stats
from chromite.lib import cidb
from chromite.lib import commandline
from chromite.lib import cros_logging as logging


# MUST be kept in sync with GetParser's build-type option.
BUILD_TYPE_MAP = {
    'cq': constants.CQ_MASTER,
    'canary': constants.CANARY_MASTER,
    'chrome-pfq': constants.PFQ_MASTER,
}


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('--cred-dir', action='store', required=True,
                      metavar='CIDB_CREDENTIALS_DIR',
                      help='Database credentials directory with certificates '
                           'and other connection information. Obtain your '
                           'credentials at go/cros-cidb-admin .')

  ex_group = parser.add_mutually_exclusive_group(required=True)

  # Ask for pull out a single build id to compare against other values.
  ex_group.add_argument('--build-id', action='store', type=int, default=None,
                        help="Single build with comparison to 'normal'.")

  # Look at all builds for a build_config.
  ex_group.add_argument('--build-config', action='store', default=None,
                        help='Build config to gather stats for.')

  # Look at all builds for a master/slave group.
  ex_group.add_argument('--build-type', default=None,
                        choices=['cq', 'chrome-pfq', 'canary'],
                        help='Build master/salves to gather stats for: cq.')

  # How many builds are included, for builder/build-types.
  start_group = parser.add_mutually_exclusive_group()
  start_group.add_argument('--start-date', action='store', type='date',
                           default=None,
                           help='Limit scope to a start date in the past.')
  start_group.add_argument('--past-month', action='store_true', default=False,
                           help='Limit scope to the past 30 days up to now.')
  start_group.add_argument('--past-week', action='store_true', default=False,
                           help='Limit scope to the past week up to now.')
  start_group.add_argument('--past-day', action='store_true', default=False,
                           help='Limit scope to the past day up to now.')

  parser.add_argument('--end-date', action='store', type='date', default=None,
                      help='Limit scope to an end date in the past.')

  return parser


def OptionsToStartEndDates(options):
  end_date = options.end_date or datetime.datetime.now().date()
  start_date = end_date - datetime.timedelta(days=7)

  if options.past_month:
    start_date = end_date - datetime.timedelta(days=30)
  elif options.past_day:
    start_date = end_date - datetime.timedelta(days=1)
  else:
    # Default of past_week.
    start_date = end_date - datetime.timedelta(days=7)

  return start_date, end_date


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  # Timeframe for discovering builds, if options.build_id not used.
  start_date, end_date = OptionsToStartEndDates(options)

  db = cidb.CIDBConnection(options.cred_dir)

  # Data about a single build (optional).
  focus_build = None

  if options.build_id:
    logging.info('Gathering data for %s', options.build_id)
    focus_status = build_time_stats.BuildIdToBuildStatus(db, options.build_id)
    focus_build = build_time_stats.GetBuildTimings(focus_status)

    builds_statuses = build_time_stats.BuildConfigToStatuses(
        db, focus_status['build_config'], start_date, end_date)

  elif options.build_config:
    builds_statuses = build_time_stats.BuildConfigToStatuses(
        db, options.build_config, start_date, end_date)

  elif options.build_type:
    builds_statuses = build_time_stats.MasterConfigToStatuses(
        db, BUILD_TYPE_MAP[options.build_type], start_date, end_date)

  # Compute per-build timing.
  builds_timings = [build_time_stats.GetBuildTimings(status)
                    for status in builds_statuses]

  # Report average data.
  print(build_time_stats.Report(focus_build, builds_timings))
