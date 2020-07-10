# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros buildresult: Look up results for a single build."""

from __future__ import print_function

import datetime
import json
import os

from chromite.cli import command
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib.buildstore import BuildStore


_FINISHED_STATUSES = (
    'fail', 'pass', 'missing', 'aborted', 'skipped', 'forgiven')


def FetchBuildStatuses(buildstore, options):
  """Fetch the requested build statuses.

  The results are NOT filtered or fixed up.

  Args:
    buildstore: BuildStore instance to make db calls.
    options: Parsed command line options set.

  Returns:
    List of build_status dicts from Buildbucket, or None.
  """
  if options.buildbucket_id:
    build_status = buildstore.GetBuildStatuses([options.buildbucket_id])[0]
    if build_status:
      return [build_status]
  elif options.build_config:
    start_date = options.start_date or options.date
    end_date = options.end_date or options.date
    return buildstore.GetBuildHistory(
        options.build_config, buildstore.NUM_RESULTS_NO_LIMIT,
        start_date=start_date, end_date=end_date)
  else:
    cros_build_lib.Die('You must specify which builds.')


def IsBuildStatusFinished(build_status):
  """Populates the 'artifacts_url' and 'stages' build_status fields.

  Args:
    build_status: Single build_status dict returned by any Fetch method.

  Returns:
    build_status dict with additional fields populated.
  """
  return build_status['status'] in _FINISHED_STATUSES


def FixUpBuildStatus(buildstore, build_status):
  """Add 'extra' build_status values we need.

  Populates the 'artifacts_url' and 'stages' build_status fields.

  Args:
    buildstore: BuildStore instance to make DB calls.
    build_status: Single build_status dict returned by any Fetch method.

  Returns:
    build_status dict with additional fields populated.
  """
  # We don't actually store the artifacts_url, but we store a URL for a specific
  # artifact we can use to derive it.
  build_status['artifacts_url'] = None
  if build_status['metadata_url']:
    build_status['artifacts_url'] = os.path.dirname(
        build_status['metadata_url'])

  # Find stage information.
  build_status['stages'] = buildstore.GetBuildsStages(
      buildbucket_ids=[build_status['buildbucket_id']])

  return build_status


def Report(build_statuses):
  """Generate the stdout description of a given build.

  Args:
    build_statuses: List of build_status dict's from FetchBuildStatus.

  Returns:
    str to display as the final report.
  """
  result = ''

  for build_status in build_statuses:
    result += '\n'.join([
        'buildbucket_id: %s' % build_status['buildbucket_id'],
        'status: %s' % build_status['status'],
        'artifacts_url: %s' % build_status['artifacts_url'],
        'toolchain_url: %s' % build_status['toolchain_url'],
        'stages:\n'
    ])
    for stage in build_status['stages']:
      result += '  %s: %s\n' % (stage['name'], stage['status'])
    result += '\n'  # Blank line between builds.

  return result


def ReportJson(build_statuses):
  """Generate the json description of a given build.

  Args:
    build_statuses: List of build_status dict's from FetchBuildStatus.

  Returns:
    str to display as the final report.
  """
  report = {}

  for build_status in build_statuses:
    report[build_status['buildbucket_id']] = {
        'buildbucket_id': build_status['buildbucket_id'],
        'status': build_status['status'],
        'stages': {s['name']: s['status'] for s in build_status['stages']},
        'artifacts_url': build_status['artifacts_url'],
        'toolchain_url': build_status['toolchain_url'],
    }

  return json.dumps(report)


@command.CommandDecorator('buildresult')
class BuildResultCommand(command.CliCommand):
  """Script that looks up results of finished builds."""

  EPILOG = """
Look up a single build result:
  cros buildresult --buildbucket-id 1234567890123

Look up results by build config name:
  cros buildresult --build-config samus-pre-cq
  cros buildresult --build-config samus-pre-cq --date 2018-1-2
  cros buildresult --build-config samus-pre-cq \
      --start-date 2018-1-2 --end-date 2018-1-7

Output can be json formatted with:
  cros buildresult --buildbucket-id 1234567890123 --report json

Note:
  This tool does NOT work for master-*-tryjob, precq-launcher-try, or
  builds on branches older than CL:942097.

Note:
  Exit code 1: A script error or bad options combination.
  Exit code 2: No matching finished builds were found.
"""

  @classmethod
  def AddParser(cls, parser):
    super(cls, BuildResultCommand).AddParser(parser)

    # What build do we report on?
    request_group = parser.add_mutually_exclusive_group()

    request_group.add_argument(
        '--buildbucket-id', help='Buildbucket ID of build to look up. '
        'It is a 19-digit long ID which can be found in Milo or GoldenEye URL.')
    request_group.add_argument(
        '--build-config', help='')

    #
    date_group = parser.add_argument_group()

    date_group.add_argument(
        '--date', action='store', type='date', default=datetime.date.today(),
        help='Request all finished builds on a given day. Default today.')

    date_group.add_argument(
        '--start-date', action='store', type='date', default=None,
        help='Request all builds between (inclusive) start and end dates.')

    date_group.add_argument(
        '--end-date', action='store', type='date', default=None,
        help='End of date range (inclusive) specified by --start-date.')

    # What kind of report do we generate?
    parser.add_argument('--report', default='standard',
                        choices=['standard', 'json'],
                        help='What format is the output in?')

  def Run(self):
    """Run cros buildresult."""
    self.options.Freeze()

    commandline.RunInsideChroot(self)

    buildstore = BuildStore(_write_to_cidb=False)
    build_statuses = FetchBuildStatuses(buildstore, self.options)

    if build_statuses:
      # Filter out builds that don't exist in Buildbucket,
      # or which aren't finished.
      build_statuses = [b for b in build_statuses if IsBuildStatusFinished(b)]

    # If we found no builds at all, return a different exit code to help
    # automated scripts know they should try waiting longer.
    if not build_statuses:
      logging.error('No build found. Perhaps not started?')
      return 2

    # Fixup all of the builds we have.
    build_statuses = [FixUpBuildStatus(buildstore, b) for b in build_statuses]

    # Produce our final result.
    if self.options.report == 'json':
      report = ReportJson(build_statuses)
    else:
      report = Report(build_statuses)

    print(report)
