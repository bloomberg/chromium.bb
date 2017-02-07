# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate alerts of failing builds to Sheriff-o-Matic."""

from __future__ import print_function

import datetime
import json

from chromite.cbuildbot import topology
from chromite.cbuildbot import tree_status
from chromite.lib import cidb
from chromite.lib import commandline
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import logdog
from chromite.lib import milo
from chromite.lib import prpc
from chromite.lib import som


# Only display this many links per stage
MAX_STAGE_LINKS = 7


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--service_acct_json', type=str, action='store',
                      help='Path to service account credentials JSON file.')
  parser.add_argument('cred_dir', action='store',
                      metavar='CIDB_CREDENTIALS_DIR',
                      help='Database credentials directory with certificates '
                           'and other connection information. Obtain your '
                           'credentials at go/cros-cidb-admin .')
  parser.add_argument('--logdog_host', type=str, action='store',
                      help='URL of Logdog host.')
  parser.add_argument('--milo_host', type=str, action='store',
                      help='URL of MILO host.')
  parser.add_argument('--som_host', type=str, action='store',
                      help='Sheriff-o-Matic host to post alerts to.')
  parser.add_argument('--som_insecure', action='store_true', default=False,
                      help='Use insecure Sheriff-o-Matic connection.')
  parser.add_argument('--output_json', type=str, action='store',
                      help='Filename to write JSON to.')
  parser.add_argument('--json_file', type=str, action='store',
                      help='JSON file to send.')
  return parser


class ObjectEncoder(json.JSONEncoder):
  """Helper object to encode object dictionaries to JSON."""
  # pylint: disable=method-hidden
  def default(self, obj):
    return obj.__dict__


def MapCIDBToSOMStatus(status):
  """Map CIDB status to Sheriff-o-Matic display status.

  In particular, maps inflight stages to being timed out since if they're
  being reported, the master has finished and the inflight stage has not
  completed, likely due to the entire build timing out.

  Args:
    status: A status string from CIDB.

  Returns:
    A string suitable for displaying by Sheriff-o-Matic.
  """
  STATUS_MAP = {
      constants.BUILDER_STATUS_FAILED: 'failed',
      constants.BUILDER_STATUS_INFLIGHT: 'timed out',
  }
  if status in STATUS_MAP:
    status = STATUS_MAP[status]
  return status


def AddLogsLink(logdog_client, name,
                waterfall, logdog_prefix, annotation_stream, logs_links):
  """Helper to add a Logdog link.

  Args:
    logdog_client: logdog.LogdogClient object.
    name: A name for the the link.
    waterfall: Waterfall for the Logdog stream
    logdog_prefix: Logdog prefix of the stream.
    annotation_stream: Logdog annotation for the stream.
    logs_links: List to add to if the stream is valid.
  """
  if annotation_stream and annotation_stream.name:
    url = logdog_client.ConstructViewerURL(waterfall,
                                           logdog_prefix,
                                           annotation_stream.name)
    logs_links.append(som.Link(name, url))


def GenerateAlertStage(build, stage, exceptions,
                       buildbot, logdog_prefix, annotation_steps,
                       logdog_client):
  """Generate alert details for a single build stage.

  Args:
    build: Dictionary of build details from CIDB.
    stage: Dictionary fo stage details from CIDB.
    exceptions: Dictionary of build failures from CIDB.
    buildbot: Buildbot build JSON file from MILO.
    logdog_prefix: Logdog prefix for the build.
    annotation_steps: Full set of Logdog annotations for the build.
    logdog_client: logdog.LogdogClient object.

  Returns:
    som.CrosStageFailure object if stage requires alert.  None otherwise.
  """
  STAGE_IGNORE_STATUSES = frozenset([constants.BUILDER_STATUS_PASSED,
                                     constants.BUILDER_STATUS_PLANNED,
                                     constants.BUILDER_STATUS_SKIPPED])
  if (stage['build_id'] != build['id'] or
      stage['status'] in STAGE_IGNORE_STATUSES):
    return None

  logging.info('    stage %s (id %d): %s', stage['name'], stage['id'],
               stage['status'])
  logs_links = []
  notes = []

  # Generate links to the logs of the stage.
  if logdog_prefix and annotation_steps and stage['name'] in annotation_steps:
    annotation = annotation_steps[stage['name']]
    AddLogsLink(logdog_client, 'stdout', build['waterfall'],
                logdog_prefix, annotation.stdout_stream, logs_links)
    AddLogsLink(logdog_client, 'stderr', build['waterfall'],
                logdog_prefix, annotation.stderr_stream, logs_links)
  else:
    notes.append('stage logs unavailable')

  # Copy the links from the buildbot build JSON.
  stage_links = []
  if buildbot:
    if stage['status'] == constants.BUILDER_STATUS_FORGIVEN:
      # TODO: Include these links but hide them by default in frontend.
      pass
    elif stage['name'] in buildbot['steps']:
      step = buildbot['steps'][stage['name']]
      stage_links = [som.Link(url, step['urls'][url]) for url in step['urls']]
    else:
      logging.warn('Could not find stage %s in: %s',
                   stage['name'], ', '.join(buildbot['steps'].keys()))
  else:
    notes.append('stage details unavailable')

  # Limit the number of links that will be displayed for a single stage.
  # Let there be one extra since it doesn't make sense to have a line
  # saying there is one more.
  # TODO: Move this to frontend so they can be unhidden by clicking.
  if len(stage_links) > MAX_STAGE_LINKS + 1:
    notes.append('... and %d more URLs' %
                 (len(stage_links) - MAX_STAGE_LINKS))
    del stage_links[MAX_STAGE_LINKS:]

  # Add all exceptions recording in CIDB as notes.
  notes.extend('%s: %s' % (e['exception_type'], e['exception_message'])
               for e in exceptions
               if e['build_stage_id'] == stage['id'])

  # Add the stage to the alert.
  return som.CrosStageFailure(stage['name'],
                              MapCIDBToSOMStatus(stage['status']),
                              logs_links, stage_links, notes)


def GenerateBuildAlert(build, slave_stages, exceptions, severity, now,
                       logdog_client, milo_client):
  """Generate an alert for a single build.

  Args:
    build: Dictionary of build details from CIDB.
    slave_stages: Dictionary of stage details from CIDB.
    exceptions: Dictionary of build failures from CIDB.
    severity: Sheriff-o-Matic severity to use for the alert.
    now: Current datettime.
    logdog_client: logdog.LogdogClient object.
    milo_client: milo.MiloClient object.

  Returns:
    som.Alert object if build requires alert.  None otherwise.
  """
  BUILD_IGNORE_STATUSES = frozenset([constants.BUILDER_STATUS_PASSED])
  if not build['important'] or build['status'] in BUILD_IGNORE_STATUSES:
    return None

  logging.info('  %s:%d (id %d) %s', build['builder_name'],
               build['build_number'], build['id'], build['status'])

  # Create links for details on the build.
  dashboard_url = tree_status.ConstructDashboardURL(build['waterfall'],
                                                    build['builder_name'],
                                                    build['build_number'])
  links = [
      som.Link('build details', dashboard_url),
      som.Link('viceroy',
               tree_status.ConstructViceroyBuildDetailsURL(build['id'])),
      som.Link('buildbot',
               tree_status.ConstructBuildStageURL(
                   constants.WATERFALL_TO_DASHBOARD[build['waterfall']],
                   build['builder_name'], build['build_number'])),
  ]

  # TODO: Gather similar failures.
  # TODO: Report of how many builds failed in a row.
  builders = [som.AlertedBuilder(build['builder_name'], dashboard_url,
                                 ToEpoch(build['finish_time'] or now),
                                 build['build_number'], build['build_number'])]

  # Access the buildbot build JSON for per-stage links of failed stages.
  try:
    buildbot = milo_client.GetBuildbotBuildJSON(build['waterfall'],
                                                build['builder_name'],
                                                build['build_number'])
  except prpc.PRPCResponseException as e:
    logging.warning('Unable to retrieve buildbot build JSON: %s', e)
    buildbot = None

  # Logdog prefix and annotations to determine log stream name of stages.
  try:
    annotations, prefix = logdog_client.GetAnnotations(build['waterfall'],
                                                       build['builder_name'],
                                                       build['build_number'])
  except (prpc.PRPCResponseException, logdog.LogdogResponseException) as e:
    logging.warning('Unable to retrieve log annotations: %s', e)
    annotations = None
    prefix = None

  if annotations:
    annotation_steps = {s.step.name: s.step for s in annotations.substep}
  else:
    annotation_steps = None

  # Highlight the problematic stages.
  stages = []
  for stage in slave_stages:
    alert_stage = GenerateAlertStage(build, stage, exceptions,
                                     buildbot, prefix, annotation_steps,
                                     logdog_client)
    if alert_stage:
      stages.append(alert_stage)

  # Add the alert to the summary.
  alert_name = '%s:%d %s' % (build['build_config'], build['build_number'],
                             MapCIDBToSOMStatus(build['status']))
  return som.Alert(build['build_config'], alert_name, alert_name, severity,
                   ToEpoch(now), ToEpoch(build['finish_time'] or now),
                   links, [], 'cros-failure',
                   som.CrosBuildFailure(stages, builders))


def GenerateAlertsSummary(db, builds=None,
                          logdog_client=None, milo_client=None):
  """Generates the full set of alerts to send to Sheriff-o-Matic.

  Args:
    db: cidb.CIDBConnection object.
    builds: A list of (waterfall, builder_name, severity) tuples to summarize.
      Defaults to SOM_IMPORTANT_BUILDS.
    logdog_client: logdog.LogdogClient object.
    milo_client: milo.MiloClient object.

  Returns:
    JSON-marshalled AlertsSummary object.
  """
  if not builds:
    builds = constants.SOM_IMPORTANT_BUILDS
  if not logdog_client:
    logdog_client = logdog.LogdogClient()
  if not milo_client:
    milo_client = milo.MiloClient()

  alerts = []
  now = datetime.datetime.utcnow()

  # Iterate over relvevant masters.
  for build_tuple in builds:
    # Find the most recent build, their slaves, and the individual slave stages.
    master = db.GetMostRecentBuild(build_tuple[0], build_tuple[1])
    slaves = db.GetSlaveStatuses(master['id'])
    slave_stages = db.GetSlaveStages(master['id'])
    exceptions = db.GetSlaveFailures(master['id'])
    logging.info('%s %s (id %d): %d slaves, %d slave stages',
                 build_tuple[0], build_tuple[1], master['id'],
                 len(slaves), len(slave_stages))

    # Look for failing and inflight (signifying timeouts) slave builds.
    for slave in sorted(slaves, key=lambda s: s['builder_name']):
      alert = GenerateBuildAlert(slave, slave_stages, exceptions,
                                 build_tuple[2], now,
                                 logdog_client, milo_client)
      if alert:
        alerts.append(alert)

  revision_summaries = {}
  summary = som.AlertsSummary(alerts, revision_summaries, ToEpoch(now))

  return json.dumps(summary, cls=ObjectEncoder)


def ToEpoch(value):
  """Convert a datetime to number of seconds past epoch."""
  epoch = datetime.datetime(1970, 1, 1)
  return (value - epoch).total_seconds()


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  # Determine which hosts to connect to.
  db = cidb.CIDBConnection(options.cred_dir)
  topology.FetchTopologyFromCIDB(db)

  if options.json_file:
    # Use the specified alerts.
    logging.info('Using JSON file %s', options.json_file)
    with open(options.json_file) as f:
      summary_json = f.read()
      print(summary_json)
  else:
    # Generate the set of alerts to send.
    logdog_client = logdog.LogdogClient(options.service_acct_json,
                                        host=options.logdog_host)
    milo_client = milo.MiloClient(options.service_acct_json,
                                  host=options.milo_host)
    summary_json = GenerateAlertsSummary(db, logdog_client=logdog_client,
                                         milo_client=milo_client)
    if options.output_json:
      with open(options.output_json, 'w') as f:
        logging.info('Writing JSON file %s', options.output_json)
        f.write(summary_json)

  # Authenticate and send the alerts.
  som_client = som.SheriffOMaticClient(options.service_acct_json,
                                       insecure=options.som_insecure,
                                       host=options.som_host)
  som_client.SendAlerts(summary_json)
