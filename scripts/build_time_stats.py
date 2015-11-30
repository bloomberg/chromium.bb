# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that shows build timing for a build, and it's stages.

This script shows how long a build took, how long each stage took, and when each
stage started relative to the start of the build.
"""

from __future__ import print_function

import collections

from chromite.lib import cidb
from chromite.lib import commandline
from chromite.lib import cros_logging as logging


BuildTiming = collections.namedtuple(
    'BuildTiming', ['duration', 'stages'])


StageTiming = collections.namedtuple(
    'StageTiming', ['name', 'start', 'finish', 'duration'])


class TimingStatsEngine(object):
  """Engine to generate stats about CL actions taken by the Commit Queue."""

  def __init__(self, db):
    self.db = db

  def GetBuildWithStages(self, build_id):
    """Fetches a build dictionary, with populated stages.

    Args:
      build_id: Id of the build to gather data for.
    """
    logging.info('Gathering data for %s', build_id)
    build_status = self.db.GetBuildStatus(build_id)
    build_status['stages'] = self.db.GetBuildStages(build_id)

    return build_status

  def GetBuildTimings(self, build_status):
    """Convert output of GetBuildWithStages into BuildTiming tuple."""
    start = build_status['start_time']

    build_duration = build_status['finish_time'] - start

    stage_times = []
    for stage in build_status['stages']:
      stage_times.append(
          StageTiming(stage['name'],
                      stage['start_time'] - start,
                      stage['finish_time'] - start,
                      stage['finish_time'] - stage['start_time']))

    return BuildTiming(build_duration, stage_times)

  def ReportBuild(self, build_status, build_timings):
    print('Build: %s (total %s)' % (build_status['id'], build_timings.duration))

    stages = sorted(build_timings.stages, key=lambda s: s.start)

    for stage in stages:
      print('  %(start)s - %(finish)s (%(duration)s): %(name)s' % vars(stage))


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('--cred-dir', action='store', required=True,
                      metavar='CIDB_CREDENTIALS_DIR',
                      help='Database credentials directory with certificates '
                           'and other connection information. Obtain your '
                           'credentials at go/cros-cidb-admin .')

  parser.add_argument('--build-id', action='store', type=int, required=True,
                      default=None, help='Build to gather stats for.')

  return parser


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  db = cidb.CIDBConnection(options.cred_dir)

  cl_stats_engine = TimingStatsEngine(db)
  build_status = cl_stats_engine.GetBuildWithStages(options.build_id)
  build_times = cl_stats_engine.GetBuildTimings(build_status)

  cl_stats_engine.ReportBuild(build_status, build_times)
