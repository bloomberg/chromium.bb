# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that shows build timing for a build, and it's stages.

This script shows how long a build took, how long each stage took, and when each
stage started relative to the start of the build.
"""

from __future__ import print_function

import datetime
import sys

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
  start_group.add_argument('--month', action='store_true', default=False,
                           help='Limit scope to the past 30 days up to now.')
  start_group.add_argument('--week', action='store_true', default=False,
                           help='Limit scope to the past week up to now.')
  start_group.add_argument('--day', action='store_true', default=False,
                           help='Limit scope to the past day up to now.')
  start_group.add_argument('--trending', action='store_true', default=False,
                           help='Show stats for all builds by month.')

  parser.add_argument('--end-date', action='store', type='date', default=None,
                      help='Limit scope to an end date in the past.')

  parser.add_argument('--csv', action='store_true', default=False,
                      help='Output in CSV format.')

  parser.add_argument('--stages', action='store_true', default=True,
                      help='Do not show stats for all stages run.')
  parser.add_argument('--nostages', dest='stages', action='store_false',
                      help='Do not show stats for all stages run.')

  report_group = parser.add_mutually_exclusive_group()
  report_group.add_argument('--report', default='standard',
                            choices=['standard', 'stability', 'success'],
                            help='What type of report to generate?')

  return parser


def OptionsToStartEndDates(options):
  end_date = options.end_date or datetime.datetime.now().date()

  if options.month:
    start_date = end_date - datetime.timedelta(days=30)
  elif options.day:
    start_date = end_date - datetime.timedelta(days=1)
  elif options.start_date:
    start_date = options.start_date
  elif options.trending:
    start_date, end_date = None, None
  else:
    # Default of past_week.
    start_date = end_date - datetime.timedelta(days=7)

  return start_date, end_date


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  db = cidb.CIDBConnection(options.cred_dir)

  # Timeframe for discovering builds, if options.build_id not used.
  start_date, end_date = OptionsToStartEndDates(options)

  # Trending is sufficiently different to be handled on it's own.
  if not options.trending and options.report != 'success':
    assert not options.csv, (
        '--csv can only be used with --trending or --report success.')

  # Data about a single build (optional).
  focus_build = None

  if options.build_id:
    logging.info('Gathering data for %s', options.build_id)
    focus_status = build_time_stats.BuildIdToBuildStatus(db, options.build_id)
    focus_build = build_time_stats.GetBuildTimings(focus_status)

    build_config = focus_status['build_config']
    builds_statuses = build_time_stats.BuildConfigToStatuses(
        db, build_config, start_date, end_date)
    description = 'Focus %d - %s' % (options.build_id, build_config)

  elif options.build_config:
    builds_statuses = build_time_stats.BuildConfigToStatuses(
        db, options.build_config, start_date, end_date)
    description = 'Config %s' % options.build_config

  elif options.build_type:
    builds_statuses = build_time_stats.MasterConfigToStatuses(
        db, BUILD_TYPE_MAP[options.build_type], start_date, end_date)
    description = 'Type %s' % options.build_type

  if not builds_statuses:
    logging.critical('No Builds Found For: %s', description)
    return 1

  if options.report == 'success':
    # Calculate per-build success rates and per-stage success rates.
    build_success_rates = build_time_stats.GetBuildSuccessRates(builds_statuses)
    stage_success_rates = (
        build_time_stats.GetStageSuccessRates(builds_statuses) if options.stages
        else {})
    if options.csv:
      build_time_stats.SuccessReportCsv(
          sys.stdout, build_success_rates, stage_success_rates)
    else:
      build_time_stats.SuccessReport(
          sys.stdout, description, build_success_rates, stage_success_rates)
    return 0

  # Compute per-build timing.
  builds_timings = [build_time_stats.GetBuildTimings(status)
                    for status in builds_statuses]

  if not builds_timings:
    logging.critical('No timing results For: %s', description)
    return 1

  # Report results.
  if options.report == 'standard':
    build_time_stats.Report(
        sys.stdout,
        description,
        focus_build,
        builds_timings,
        options.stages,
        options.trending,
        options.csv)
  elif options.report == 'stability':
    build_time_stats.StabilityReport(
        sys.stdout,
        description,
        builds_timings)
