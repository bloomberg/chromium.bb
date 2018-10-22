# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides the web interface for displaying an overview of alerts."""

import json

from google.appengine.datastore.datastore_query import Cursor
from google.appengine.ext import ndb

from dashboard import email_template
from dashboard.common import request_handler
from dashboard.common import utils
from dashboard.models import anomaly
from dashboard.models import sheriff

_MAX_ANOMALIES_TO_COUNT = 5000
_MAX_ANOMALIES_TO_SHOW = 500


class AlertsHandler(request_handler.RequestHandler):
  """Shows an overview of recent anomalies for perf sheriffing."""

  def get(self):
    """Renders the UI for listing alerts."""
    self.RenderStaticHtml('alerts.html')

  def post(self):
    """Returns dynamic data for listing alerts in response to XHR.

    Request parameters:
      sheriff: The name of a sheriff (optional).
      triaged: Whether to include triaged alerts (i.e. with a bug ID).
      improvements: Whether to include improvement anomalies.
      anomaly_cursor: Where to begin a paged query for anomalies (optional).

    Outputs:
      JSON data for an XHR request to show a table of alerts.
    """
    sheriff_name = self.request.get('sheriff', 'Chromium Perf Sheriff')
    if not _SheriffIsFound(sheriff_name):
      self.response.out.write(json.dumps({
          'error': 'Sheriff "%s" not found.' % sheriff_name
      }))
      return

    # Cursors are used to fetch paged queries. If none is supplied, then the
    # first 500 alerts will be returned. If a cursor is given, the next
    # 500 alerts (starting at the given cursor) will be returned.
    anomaly_cursor = self.request.get('anomaly_cursor', None)
    if anomaly_cursor:
      anomaly_cursor = Cursor(urlsafe=anomaly_cursor)

    is_improvement = None
    if not bool(self.request.get('improvements')):
      is_improvement = False

    bug_id = None
    recovered = None
    if not bool(self.request.get('triaged')):
      bug_id = ''
      recovered = False

    max_anomalies_to_show = _MAX_ANOMALIES_TO_SHOW
    if self.request.get('max_anomalies_to_show'):
      max_anomalies_to_show = int(self.request.get('max_anomalies_to_show'))

    anomalies, next_cursor, count = anomaly.Anomaly.QueryAsync(
        start_cursor=anomaly_cursor,
        sheriff=sheriff_name,
        bug_id=bug_id,
        is_improvement=is_improvement,
        recovered=recovered,
        count_limit=_MAX_ANOMALIES_TO_COUNT,
        limit=max_anomalies_to_show).get_result()

    values = {
        'anomaly_list': AnomalyDicts(anomalies),
        'anomaly_count': count,
        'sheriff_list': _GetSheriffList(),
        'anomaly_cursor': (next_cursor.urlsafe() if next_cursor else None),
        'show_more_anomalies': next_cursor != None,
    }
    self.GetDynamicVariables(values)
    self.response.out.write(json.dumps(values))


def _SheriffIsFound(sheriff_name):
  """Checks whether the sheriff can be found for the current user."""
  sheriff_key = ndb.Key('Sheriff', sheriff_name)
  try:
    sheriff_entity = sheriff_key.get()
  except AssertionError:
    # This assertion is raised in InternalOnlyModel._post_get_hook,
    # and indicates an internal-only Sheriff but an external user.
    return False
  return sheriff_entity is not None


def _GetSheriffList():
  """Returns a list of sheriff names for all sheriffs in the datastore."""
  sheriff_keys = sheriff.Sheriff.query().fetch(keys_only=True)
  return [key.string_id() for key in sheriff_keys]


def AnomalyDicts(anomalies):
  """Makes a list of dicts with properties of Anomaly entities."""
  bisect_statuses = _GetBisectStatusDict(anomalies)
  return [GetAnomalyDict(a, bisect_statuses.get(a.bug_id)) for a in anomalies]


def GetAnomalyDict(anomaly_entity, bisect_status=None):
  """Returns a dictionary for an Anomaly which can be encoded as JSON.

  Args:
    anomaly_entity: An Anomaly entity.
    bisect_status: String status of bisect run.

  Returns:
    A dictionary which is safe to be encoded as JSON.
  """
  alert_dict = _AlertDict(anomaly_entity)
  alert_dict.update({
      'median_after_anomaly': anomaly_entity.median_after_anomaly,
      'median_before_anomaly': anomaly_entity.median_before_anomaly,
      'percent_changed': '%s' % anomaly_entity.GetDisplayPercentChanged(),
      'absolute_delta': '%s' % anomaly_entity.GetDisplayAbsoluteChanged(),
      'improvement': anomaly_entity.is_improvement,
      'bisect_status': bisect_status,
      'recovered': anomaly_entity.recovered,
      'ref_test': anomaly_entity.GetRefTestPath(),
      'type': 'anomaly',
      'units': anomaly_entity.units,
  })
  return alert_dict


def _AlertDict(alert_entity):
  """Returns a base dictionary with properties common to all alerts."""
  test_path = utils.TestPath(alert_entity.GetTestMetadataKey())
  test_path_parts = test_path.split('/')
  dashboard_link = email_template.GetReportPageLink(
      test_path, rev=alert_entity.end_revision, add_protocol_and_host=False)
  return {
      'key': alert_entity.key.urlsafe(),
      'start_revision': alert_entity.start_revision,
      'end_revision': alert_entity.end_revision,
      'date': str(alert_entity.timestamp.date()),
      'timestamp': alert_entity.timestamp.isoformat(),
      'master': test_path_parts[0],
      'bot': test_path_parts[1],
      'testsuite': test_path_parts[2],
      'test': '/'.join(test_path_parts[3:]),
      'bug_id': alert_entity.bug_id,
      'dashboard_link': dashboard_link,
      'display_start': alert_entity.display_start,
      'display_end': alert_entity.display_end,
  }


def _GetBisectStatusDict(anomalies):
  """Returns a dictionary of bug ID to bisect status string."""
  bug_id_list = {a.bug_id for a in anomalies if a.bug_id > 0}
  bugs = ndb.get_multi(ndb.Key('Bug', b) for b in bug_id_list)
  return {b.key.id(): b.latest_bisect_status for b in bugs if b}
