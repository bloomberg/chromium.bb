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
from chromite.lib import classifier
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
  parser.add_argument('builds', type=str, nargs='*', action='store',
                      metavar='WATERFALL,TREE,SEVERITY|BUILD_ID,SEVERITY',
                      help='Builds to report on.  eg chromeos,elm-release,1000 '
                           'or chromeos,master-paladin,1001 or 1234567,,1000')
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
  if annotation_stream and annotation_stream['name']:
    url = logdog_client.ConstructViewerURL(waterfall,
                                           logdog_prefix,
                                           annotation_stream['name'])
    logs_links.append(som.Link(name, url))


def GenerateAlertStage(build, stage, exceptions, buildinfo, logdog_client):
  """Generate alert details for a single build stage.

  Args:
    build: Dictionary of build details from CIDB.
    stage: Dictionary fo stage details from CIDB.
    exceptions: Dictionary of build failures from CIDB.
    buildinfo: BuildInfo build JSON file from MILO.
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

  # Generate links to the logs of the stage and use them for classification.
  if buildinfo and stage['name'] in buildinfo['steps']:
    prefix = buildinfo['annotationStream']['prefix']
    annotation = buildinfo['steps'][stage['name']]
    AddLogsLink(logdog_client, 'stdout', build['waterfall'],
                prefix, annotation.get('stdoutStream', None), logs_links)
    AddLogsLink(logdog_client, 'stderr', build['waterfall'],
                prefix, annotation.get('stderrStream', None), logs_links)

    # Use the logs in an attempt to classify the failure.
    if annotation['stdoutStream'] and annotation['stdoutStream']['name']:
      path = '%s/+/%s' % (prefix, annotation['stdoutStream']['name'])
      try:
        logs = logdog_client.GetLines(build['waterfall'], path)
        classification = classifier.ClassifyFailure(stage['name'], logs)
        for c in classification or []:
          notes.append('Classification: %s' % (c))
      except Exception as e:
        logging.exception('Could not classify logs: %s', e)
        notes.append('Warning: unable to classify logs: %s' % (e))
  else:
    notes.append('Warning: stage logs unavailable')

  # Copy the links from the buildbot build JSON.
  stage_links = []
  if buildinfo:
    if stage['status'] == constants.BUILDER_STATUS_FORGIVEN:
      # TODO: Include these links but hide them by default in frontend.
      pass
    elif stage['name'] in buildinfo['steps']:
      step = buildinfo['steps'][stage['name']]
      stage_links = [som.Link(l['label'], l['url'])
                     for l in step.get('otherLinks', [])]
    else:
      steps = [s for s in buildinfo['steps'].keys()
               if s is not None and not isinstance(s, tuple)]
      logging.warn('Could not find stage %s in: %s',
                   stage['name'], ', '.join(steps))
  else:
    notes.append('Warning: stage details unavailable')

  # Limit the number of links that will be displayed for a single stage.
  # Let there be one extra since it doesn't make sense to have a line
  # saying there is one more.
  # TODO: Move this to frontend so they can be unhidden by clicking.
  if len(stage_links) > MAX_STAGE_LINKS + 1:
    # Insert at the beginning of the notes which come right after the links.
    notes.insert(0, '... and %d more URLs' % (len(stage_links) -
                                              MAX_STAGE_LINKS))
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

  # Access the BuildInfo for per-stage links of failed stages.
  try:
    buildinfo = milo_client.BuildInfoGetBuildbot(build['waterfall'],
                                                 build['builder_name'],
                                                 build['build_number'])
  except prpc.PRPCResponseException as e:
    logging.warning('Unable to retrieve BuildInfo: %s', e)
    buildinfo = None

  # Highlight the problematic stages.
  stages = []
  for stage in slave_stages:
    alert_stage = GenerateAlertStage(build, stage, exceptions,
                                     buildinfo,
                                     logdog_client)
    if alert_stage:
      stages.append(alert_stage)

  # Add the alert to the summary.
  key = '%s:%s:%d' % (build['waterfall'], build['build_config'],
                      build['build_number'])
  alert_name = '%s:%d %s' % (build['build_config'], build['build_number'],
                             MapCIDBToSOMStatus(build['status']))
  return som.Alert(key, alert_name, alert_name, int(severity),
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

  # Iterate over relevant masters.
  # build_tuple is either: waterfall, build_config, severity
  #  or: build_id, severity
  for build_tuple in builds:
    # Find the specified build.
    if len(build_tuple) == 2:
      # pylint: disable=unbalanced-tuple-unpacking
      build_id, severity = build_tuple
      # pylint: enable=unbalanced-tuple-unpacking
      master = db.GetBuildStatus(build_id)
      waterfall = master['waterfall']
      build_config = master['build_config']
    elif len(build_tuple) == 3:
      waterfall, build_config, severity = build_tuple
      master = db.GetMostRecentBuild(waterfall, build_config)
    else:
      logging.error('Invalid build tuple: %s' % str(build_tuple))
      continue

    # Find any slave builds, and the individual slave stages.
    statuses = db.GetSlaveStatuses(master['id'])
    if len(statuses):
      stages = db.GetSlaveStages(master['id'])
      exceptions = db.GetSlaveFailures(master['id'])
      logging.info('%s %s (id %d): %d slaves, %d slave stages',
                   waterfall, build_config, master['id'],
                   len(statuses), len(stages))
    else:
      # Didn't find any slaves, so treat as a singular build.
      statuses = [master]
      stages = db.GetBuildStages(master['id'])
      exceptions = db.GetBuildsFailures([master['id']])
      logging.info('%s %s (id %d): single build, %d stages',
                   waterfall, build_config, master['id'],
                   len(stages))

    # Look for failing and inflight (signifying timeouts) slave builds.
    for build in sorted(statuses, key=lambda s: s['builder_name']):
      alert = GenerateBuildAlert(build, stages, exceptions,
                                 severity, now,
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
  builds = [tuple(x.split(',')) for x in options.builds]

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
    summary_json = GenerateAlertsSummary(db, builds=builds,
                                         logdog_client=logdog_client,
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
