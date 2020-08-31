# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
from __future__ import division
from __future__ import absolute_import

import sys
import unittest

import mock

from google.appengine.ext import ndb

from dashboard import find_anomalies
from dashboard import find_change_points
from dashboard.common import testing_common
from dashboard.common import utils
from dashboard.models import alert_group
from dashboard.models import anomaly
from dashboard.models import anomaly_config
from dashboard.models import graph_data
from dashboard.models import histogram
from dashboard.models.subscription import Subscription
from dashboard.models.subscription import VISIBILITY
from tracing.value.diagnostics import reserved_infos
from dashboard.sheriff_config_client import SheriffConfigClient

# Sample time series.
_TEST_ROW_DATA = [
    (241105, 2136.7), (241116, 2140.3), (241151, 2149.1),
    (241154, 2147.2), (241156, 2130.6), (241160, 2136.2),
    (241188, 2146.7), (241201, 2141.8), (241226, 2140.6),
    (241247, 2128.1), (241249, 2134.2), (241254, 2130.0),
    (241262, 2136.0), (241268, 2142.6), (241271, 2149.1),
    (241282, 2156.6), (241294, 2125.3), (241298, 2155.5),
    (241303, 2148.5), (241317, 2146.2), (241323, 2123.3),
    (241330, 2121.5), (241342, 2141.2), (241355, 2145.2),
    (241371, 2136.3), (241386, 2144.0), (241405, 2138.1),
    (241420, 2147.6), (241432, 2140.7), (241441, 2132.2),
    (241452, 2138.2), (241455, 2139.3), (241471, 2134.0),
    (241488, 2137.2), (241503, 2152.5), (241520, 2136.3),
    (241524, 2139.3), (241529, 2143.5), (241532, 2145.5),
    (241535, 2147.0), (241537, 2184.1), (241546, 2180.8),
    (241553, 2181.5), (241559, 2176.8), (241566, 2174.0),
    (241577, 2182.8), (241579, 2184.8), (241582, 2190.5),
    (241584, 2183.1), (241609, 2178.3), (241620, 2178.1),
    (241645, 2190.8), (241653, 2177.7), (241666, 2185.3),
    (241697, 2173.8), (241716, 2172.1), (241735, 2172.5),
    (241757, 2174.7), (241766, 2196.7), (241782, 2184.1),
]

def _MakeSampleChangePoint(x_value, median_before, median_after):
  """Makes a sample find_change_points.ChangePoint for use in these tests."""
  # The only thing that matters in these tests is the revision number
  # and the values before and after.
  return find_change_points.ChangePoint(
      x_value=x_value,
      median_before=median_before,
      median_after=median_after,
      window_start=1,
      window_end=8,
      size_before=None,
      size_after=None,
      relative_change=None,
      std_dev_before=None,
      t_statistic=None,
      degrees_of_freedom=None,
      p_value=None)


class EndRevisionMatcher(object):
  """Custom matcher to test if an anomaly matches a given end rev."""

  def __init__(self, end_revision):
    """Initializes with the end time to check."""
    self._end_revision = end_revision

  def __eq__(self, rhs):
    """Checks to see if RHS has the same end time."""
    return self._end_revision == rhs.end_revision

  def __repr__(self):
    """Shows a readable revision which can be printed when assert fails."""
    return '<IsEndRevision %d>' % self._end_revision


class ModelMatcher(object):
  """Custom matcher to check if two ndb entity names match."""

  def __init__(self, name):
    """Initializes with the name of the entity."""
    self._name = name

  def __eq__(self, rhs):
    """Checks to see if RHS has the same name."""
    return (rhs.key.string_id() if rhs.key else rhs.name) == self._name

  def __repr__(self):
    """Shows a readable revision which can be printed when assert fails."""
    return '<IsModel %s>' % self._name


@ndb.tasklet
def _MockTasklet(*_):
  raise ndb.Return(None)


@mock.patch.object(SheriffConfigClient, '__init__',
                   mock.MagicMock(return_value=None))
class ProcessAlertsTest(testing_common.TestCase):

  def setUp(self):
    super(ProcessAlertsTest, self).setUp()
    self.SetCurrentUser('foo@bar.com', is_admin=True)

  def _AddDataForTests(self, stats=None, masters=None):
    if not masters:
      masters = ['ChromiumGPU']
    testing_common.AddTests(
        masters,
        ['linux-release'], {
            'scrolling_benchmark': {
                'ref': {},
            },
        })

    for m in masters:
      ref = utils.TestKey(
          '%s/linux-release/scrolling_benchmark/ref' % m).get()
      ref.units = 'ms'
      for i in range(9000, 10070, 5):
        # Internal-only data should be found.
        test_container_key = utils.GetTestContainerKey(ref.key)
        r = graph_data.Row(
            id=i + 1, value=float(i * 3),
            parent=test_container_key, internal_only=True)
        if stats:
          for s in stats:
            setattr(r, s, i)
        r.put()

  def _DataSeries(self):
    return [(r.revision, r, r.value) for r in list(graph_data.Row.query())]

  @mock.patch.object(
      find_anomalies.find_change_points, 'FindChangePoints',
      mock.MagicMock(return_value=[
          _MakeSampleChangePoint(10011, 50, 100),
          _MakeSampleChangePoint(10041, 200, 100),
          _MakeSampleChangePoint(10061, 0, 100),
      ]))
  @mock.patch.object(find_anomalies.email_sheriff, 'EmailSheriff')
  def testProcessTest(self, mock_email_sheriff):
    self._AddDataForTests()
    test_path = 'ChromiumGPU/linux-release/scrolling_benchmark/ref'
    test = utils.TestKey(test_path).get()
    test.UpdateSheriff()
    test.put()

    alert_group_key = alert_group.AlertGroup(
        name='scrolling_benchmark',
        status=alert_group.AlertGroup.Status.untriaged,
        active=True,
        revision=alert_group.RevisionRange(
            repository='chromium', start=10000, end=10070),
    ).put()

    s1 = Subscription(name='sheriff1', visibility=VISIBILITY.PUBLIC)
    s2 = Subscription(name='sheriff2', visibility=VISIBILITY.PUBLIC)
    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([s1, s2], None))) as m:
      find_anomalies.ProcessTests([test.key])
      self.assertEqual(m.call_args_list, [mock.call(test.key.id())])
    self.ExecuteDeferredTasks('default')

    expected_calls = [
        mock.call([ModelMatcher('sheriff1'), ModelMatcher('sheriff2')],
                  ModelMatcher(
                      'ChromiumGPU/linux-release/scrolling_benchmark/ref'),
                  EndRevisionMatcher(10011)),
        mock.call([ModelMatcher('sheriff1'), ModelMatcher('sheriff2')],
                  ModelMatcher(
                      'ChromiumGPU/linux-release/scrolling_benchmark/ref'),
                  EndRevisionMatcher(10041)),
        mock.call([ModelMatcher('sheriff1'), ModelMatcher('sheriff2')],
                  ModelMatcher(
                      'ChromiumGPU/linux-release/scrolling_benchmark/ref'),
                  EndRevisionMatcher(10061))]
    self.assertEqual(expected_calls, mock_email_sheriff.call_args_list)

    anomalies = anomaly.Anomaly.query().fetch()
    self.assertEqual(len(anomalies), 3)
    for a in anomalies:
      self.assertEqual(a.groups, [alert_group_key])

    def AnomalyExists(
        anomalies, test, percent_changed, direction,
        start_revision, end_revision, subscription_names, internal_only, units,
        absolute_delta, statistic):
      for a in anomalies:
        if (a.test == test and
            a.percent_changed == percent_changed and
            a.direction == direction and
            a.start_revision == start_revision and
            a.end_revision == end_revision and
            a.subscription_names == subscription_names and
            a.internal_only == internal_only and
            a.units == units and
            a.absolute_delta == absolute_delta and
            a.statistic == statistic):
          return True
      return False

    self.assertTrue(
        AnomalyExists(
            anomalies, test.key, percent_changed=100, direction=anomaly.UP,
            start_revision=10007, end_revision=10011,
            subscription_names=['sheriff1', 'sheriff2'],
            internal_only=False, units='ms', absolute_delta=50,
            statistic='avg'))

    self.assertTrue(
        AnomalyExists(
            anomalies, test.key, percent_changed=-50, direction=anomaly.DOWN,
            start_revision=10037, end_revision=10041,
            subscription_names=['sheriff1', 'sheriff2'],
            internal_only=False, units='ms', absolute_delta=-100,
            statistic='avg'))

    self.assertTrue(
        AnomalyExists(
            anomalies, test.key, percent_changed=sys.float_info.max,
            direction=anomaly.UP, start_revision=10057, end_revision=10061,
            internal_only=False, units='ms',
            subscription_names=['sheriff1', 'sheriff2'],
            absolute_delta=100, statistic='avg'))

    # This is here just to verify that AnomalyExists returns False sometimes.
    self.assertFalse(
        AnomalyExists(
            anomalies, test.key, percent_changed=100, direction=anomaly.DOWN,
            start_revision=10037, end_revision=10041,
            subscription_names=['sheriff1', 'sheriff2'],
            internal_only=False, units='ms', absolute_delta=500,
            statistic='avg'))

  @mock.patch.object(
      find_anomalies, '_ProcessTestStat')
  def testProcessTest_SkipsClankInternal(self, mock_process_stat):
    mock_process_stat.side_effect = _MockTasklet

    self._AddDataForTests(masters=['ClankInternal'])
    test_path = 'ClankInternal/linux-release/scrolling_benchmark/ref'
    test = utils.TestKey(test_path).get()

    a = anomaly.Anomaly(
        test=test.key, start_revision=10061, end_revision=10062,
        statistic='avg')
    a.put()

    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([], None))) as m:
      find_anomalies.ProcessTests([test.key])
      self.assertEqual(m.call_args_list, [])
    self.ExecuteDeferredTasks('default')

    self.assertFalse(mock_process_stat.called)

  @mock.patch.object(
      find_anomalies, '_ProcessTestStat')
  def testProcessTest_UsesLastAlert_Avg(self, mock_process_stat):
    mock_process_stat.side_effect = _MockTasklet

    self._AddDataForTests()
    test_path = 'ChromiumGPU/linux-release/scrolling_benchmark/ref'
    test = utils.TestKey(test_path).get()

    a = anomaly.Anomaly(
        test=test.key, start_revision=10061, end_revision=10062,
        statistic='avg')
    a.put()

    test.UpdateSheriff()
    test.put()

    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([], None))):
      find_anomalies.ProcessTests([test.key])
    self.ExecuteDeferredTasks('default')

    query = graph_data.Row.query(projection=['revision', 'value'])
    query = query.filter(graph_data.Row.revision > 10062)
    query = query.filter(
        graph_data.Row.parent_test == utils.OldStyleTestKey(test.key))
    row_data = query.fetch()
    rows = [(r.revision, r, r.value) for r in row_data]
    mock_process_stat.assert_called_with(
        mock.ANY, mock.ANY, mock.ANY, rows, None)

    anomalies = anomaly.Anomaly.query().fetch()
    self.assertEqual(len(anomalies), 1)

  @mock.patch.object(
      find_anomalies, '_ProcessTestStat')
  def testProcessTest_SkipsLastAlert_NotAvg(self, mock_process_stat):
    self._AddDataForTests(stats=('count',))
    test_path = 'ChromiumGPU/linux-release/scrolling_benchmark/ref'
    test = utils.TestKey(test_path).get()

    a = anomaly.Anomaly(
        test=test.key, start_revision=10061, end_revision=10062,
        statistic='count')
    a.put()

    test.UpdateSheriff()
    test.put()

    @ndb.tasklet
    def _AssertParams(
        config, test_entity, stat, rows, ref_rows):
      del config
      del test_entity
      del stat
      del ref_rows
      assert rows[0][0] < a.end_revision

    mock_process_stat.side_effect = _AssertParams
    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([], None))):
      find_anomalies.ProcessTests([test.key])
    self.ExecuteDeferredTasks('default')

  @mock.patch.object(
      find_anomalies.find_change_points, 'FindChangePoints',
      mock.MagicMock(return_value=[
          _MakeSampleChangePoint(10011, 100, 50)
      ]))
  def testProcessTest_ImprovementMarkedAsImprovement(self):
    self._AddDataForTests()
    test = utils.TestKey(
        'ChromiumGPU/linux-release/scrolling_benchmark/ref').get()
    test.improvement_direction = anomaly.DOWN
    test.UpdateSheriff()
    test.put()
    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([], None))) as m:
      find_anomalies.ProcessTests([test.key])
      self.assertEqual(m.call_args_list, [mock.call(test.key.id())])
    anomalies = anomaly.Anomaly.query().fetch()
    self.assertEqual(len(anomalies), 1)
    self.assertTrue(anomalies[0].is_improvement)

  @mock.patch('logging.warning')
  def testProcessTest_NoSheriff_ErrorLogged(self, mock_logging_error):
    self._AddDataForTests()
    ref = utils.TestKey(
        'ChromiumGPU/linux-release/scrolling_benchmark/ref').get()
    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([], None))):
      find_anomalies.ProcessTests([ref.key])
    mock_logging_error.assert_called_with('No subscription for %s',
                                          ref.key.string_id())

  @mock.patch.object(
      find_anomalies.find_change_points, 'FindChangePoints',
      mock.MagicMock(return_value=[
          _MakeSampleChangePoint(10026, 55.2, 57.8),
          _MakeSampleChangePoint(10041, 45.2, 37.8),
      ]))
  @mock.patch.object(find_anomalies.email_sheriff, 'EmailSheriff')
  def testProcessTest_FiltersOutImprovements(self, mock_email_sheriff):
    self._AddDataForTests()
    test = utils.TestKey(
        'ChromiumGPU/linux-release/scrolling_benchmark/ref').get()
    test.improvement_direction = anomaly.UP
    test.UpdateSheriff()
    test.put()
    s = Subscription(name='sheriff', visibility=VISIBILITY.PUBLIC)
    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([s], None))) as m:
      find_anomalies.ProcessTests([test.key])
      self.assertEqual(m.call_args_list, [mock.call(test.key.id())])
    self.ExecuteDeferredTasks('default')
    mock_email_sheriff.assert_called_once_with(
        [ModelMatcher('sheriff')],
        ModelMatcher('ChromiumGPU/linux-release/scrolling_benchmark/ref'),
        EndRevisionMatcher(10041))

  @mock.patch.object(
      find_anomalies.find_change_points, 'FindChangePoints',
      mock.MagicMock(return_value=[
          _MakeSampleChangePoint(10011, 50, 100),
      ]))
  @mock.patch.object(find_anomalies.email_sheriff, 'EmailSheriff')
  def testProcessTest_InternalOnlyTest(self, mock_email_sheriff):
    self._AddDataForTests()
    test = utils.TestKey(
        'ChromiumGPU/linux-release/scrolling_benchmark/ref').get()
    test.internal_only = True
    test.UpdateSheriff()
    test.put()

    s = Subscription(name='sheriff', visibility=VISIBILITY.PUBLIC)
    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([s], None))) as m:
      find_anomalies.ProcessTests([test.key])
      self.assertEqual(m.call_args_list, [mock.call(test.key.id())])
    self.ExecuteDeferredTasks('default')
    expected_calls = [
        mock.call([ModelMatcher('sheriff')],
                  ModelMatcher(
                      'ChromiumGPU/linux-release/scrolling_benchmark/ref'),
                  EndRevisionMatcher(10011))]
    self.assertEqual(expected_calls, mock_email_sheriff.call_args_list)

    anomalies = anomaly.Anomaly.query().fetch()
    self.assertEqual(len(anomalies), 1)
    self.assertEqual(test.key, anomalies[0].test)
    self.assertEqual(100, anomalies[0].percent_changed)
    self.assertEqual(anomaly.UP, anomalies[0].direction)
    self.assertEqual(10007, anomalies[0].start_revision)
    self.assertEqual(10011, anomalies[0].end_revision)
    self.assertTrue(anomalies[0].internal_only)

  def testProcessTest_CreatesAnAnomaly_RefMovesToo_BenchmarkDuration(self):
    testing_common.AddTests(
        ['ChromiumGPU'], ['linux-release'], {
            'foo': {'benchmark_duration': {'ref': {}}},
        })
    ref = utils.TestKey(
        'ChromiumGPU/linux-release/foo/benchmark_duration/ref').get()
    non_ref = utils.TestKey(
        'ChromiumGPU/linux-release/foo/benchmark_duration').get()
    test_container_key = utils.GetTestContainerKey(ref.key)
    test_container_key_non_ref = utils.GetTestContainerKey(non_ref.key)
    for row in _TEST_ROW_DATA:
      graph_data.Row(id=row[0], value=row[1], parent=test_container_key).put()
      graph_data.Row(id=row[0], value=row[1],
                     parent=test_container_key_non_ref).put()
    ref.UpdateSheriff()
    ref.put()
    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([], None))) as m:
      find_anomalies.ProcessTests([ref.key])
      self.assertEqual(m.call_args_list, [mock.call(ref.key.id())])
    new_anomalies = anomaly.Anomaly.query().fetch()
    self.assertEqual(1, len(new_anomalies))

  def testProcessTest_AnomaliesMatchRefSeries_NoAlertCreated(self):
    # Tests that a Anomaly entity is not created if both the test and its
    # corresponding ref build series have the same data.
    testing_common.AddTests(
        ['ChromiumGPU'], ['linux-release'], {
            'scrolling_benchmark': {'ref': {}},
        })
    ref = utils.TestKey(
        'ChromiumGPU/linux-release/scrolling_benchmark/ref').get()
    non_ref = utils.TestKey(
        'ChromiumGPU/linux-release/scrolling_benchmark').get()
    test_container_key = utils.GetTestContainerKey(ref.key)
    test_container_key_non_ref = utils.GetTestContainerKey(non_ref.key)
    for row in _TEST_ROW_DATA:
      graph_data.Row(id=row[0], value=row[1], parent=test_container_key).put()
      graph_data.Row(id=row[0], value=row[1],
                     parent=test_container_key_non_ref).put()
    ref.UpdateSheriff()
    ref.put()
    non_ref.UpdateSheriff()
    non_ref.put()
    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([], None))):
      find_anomalies.ProcessTests([non_ref.key])
    new_anomalies = anomaly.Anomaly.query().fetch()
    self.assertEqual(0, len(new_anomalies))

  def testProcessTest_AnomalyDoesNotMatchRefSeries_AlertCreated(self):
    # Tests that an Anomaly entity is created when non-ref series goes up, but
    # the ref series stays flat.
    testing_common.AddTests(
        ['ChromiumGPU'], ['linux-release'], {
            'scrolling_benchmark': {'ref': {}},
        })
    ref = utils.TestKey(
        'ChromiumGPU/linux-release/scrolling_benchmark/ref').get()
    non_ref = utils.TestKey(
        'ChromiumGPU/linux-release/scrolling_benchmark').get()
    test_container_key = utils.GetTestContainerKey(ref.key)
    test_container_key_non_ref = utils.GetTestContainerKey(non_ref.key)
    for row in _TEST_ROW_DATA:
      graph_data.Row(id=row[0], value=2125.375, parent=test_container_key).put()
      graph_data.Row(id=row[0], value=row[1],
                     parent=test_container_key_non_ref).put()
    ref.UpdateSheriff()
    ref.put()
    non_ref.UpdateSheriff()
    non_ref.put()
    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([], None))) as m:
      find_anomalies.ProcessTests([non_ref.key])
      self.assertEqual(m.call_args_list, [mock.call(non_ref.key.id())])
    new_anomalies = anomaly.Anomaly.query().fetch()
    self.assertEqual(len(new_anomalies), 1)

  def testProcessTest_CreatesAnAnomaly(self):
    testing_common.AddTests(
        ['ChromiumGPU'], ['linux-release'], {
            'scrolling_benchmark': {'ref': {}},
        })
    ref = utils.TestKey(
        'ChromiumGPU/linux-release/scrolling_benchmark/ref').get()
    test_container_key = utils.GetTestContainerKey(ref.key)
    for row in _TEST_ROW_DATA:
      graph_data.Row(id=row[0], value=row[1], parent=test_container_key).put()
    ref.UpdateSheriff()
    ref.put()
    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([], None))) as m:
      find_anomalies.ProcessTests([ref.key])
      self.assertEqual(m.call_args_list, [mock.call(ref.key.id())])
    new_anomalies = anomaly.Anomaly.query().fetch()
    self.assertEqual(1, len(new_anomalies))
    self.assertEqual(anomaly.UP, new_anomalies[0].direction)
    self.assertEqual(241536, new_anomalies[0].start_revision)
    self.assertEqual(241537, new_anomalies[0].end_revision)

  def testProcessTest_RefineAnomalyPlacement_OffByOneBefore(self):
    testing_common.AddTests(
        ['ChromiumPerf'], ['linux-perf'],
        {'blink_perf.layout': {
            'nested-percent-height-tables': {}
        }})
    test = utils.TestKey(
        'ChromiumPerf/linux-perf/blink_perf.layout/nested-percent-height-tables'
    ).get()
    test_container_key = utils.GetTestContainerKey(test.key)
    sample_data = [
        (728446, 480.2504),
        (728462, 487.685),
        (728469, 486.6389),
        (728480, 477.6597),
        (728492, 471.2238),
        (728512, 480.4379),
        (728539, 464.5573),
        (728594, 489.0594),
        (728644, 484.4796),
        (728714, 489.5986),
        (728751, 489.474),
        (728788, 481.9336),
        (728835, 484.089),
        (728869, 485.4287),
        (728883, 476.8234),
        (728907, 487.4736),
        (728938, 490.601),
        (728986, 483.5039),
        (729021, 485.176),
        (729066, 484.5855),
        (729105, 483.9114),
        (729119, 483.559),
        (729161, 477.6875),
        (729201, 484.9668),
        (729240, 480.7091),
        (729270, 484.5506),
        (729292, 495.1445),
        (729309, 479.9111),
        (729329, 479.8815),
        (729391, 487.5683),
        (729430, 476.7355),
        (729478, 487.7251),
        (729525, 493.1012),
        (729568, 497.7565),
        (729608, 499.6481),
        (729642, 496.1591),
        (729658, 493.4581),
        (729687, 486.1097),
        (729706, 478.036),
        (729730, 480.4222),  # In crbug/1041688 this was the original placement.
        (729764, 421.0342),  # We instead should be setting it here.
        (729795, 428.0284),
        (729846, 433.8261),
        (729883, 429.49),
        (729920, 436.3342),
        (729975, 434.3996),
        (730011, 428.3672),
        (730054, 436.309),
        (730094, 435.3792),
        (730128, 433.0537),
    ]
    for row in sample_data:
      graph_data.Row(id=row[0], value=row[1], parent=test_container_key).put()
    test.UpdateSheriff()
    test.put()
    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([], None))) as m:
      find_anomalies.ProcessTests([test.key])
      self.assertEqual(m.call_args_list, [mock.call(test.test_path)])
    new_anomalies = anomaly.Anomaly.query().fetch()
    self.assertEqual(1, len(new_anomalies))
    self.assertEqual(anomaly.DOWN, new_anomalies[0].direction)
    self.assertEqual(729731, new_anomalies[0].start_revision)
    self.assertEqual(729764, new_anomalies[0].end_revision)

  def testProcessTest_RefineAnomalyPlacement_OffByOneStable(self):
    testing_common.AddTests(
        ['ChromiumPerf'], ['linux-perf'], {
            'memory.desktop': {
                ('memory:chrome:all_processes:'
                 'reported_by_chrome:v8:effective_size_avg'): {}
            }
        })
    test = utils.TestKey(
        ('ChromiumPerf/linux-perf/memory.desktop/'
         'memory:chrome:all_processes:reported_by_chrome:v8:effective_size_avg'
        )).get()
    test_container_key = utils.GetTestContainerKey(test.key)
    sample_data = [
        (733480, 1381203.0),
        (733494, 1381220.0),
        (733504, 1381212.0),
        (733524, 1381220.0),
        (733538, 1381211.0),
        (733544, 1381212.0),
        (733549, 1381220.0),
        (733563, 1381220.0),
        (733581, 1381220.0),
        (733597, 1381212.0),
        (733611, 1381228.0),
        (733641, 1381212.0),
        (733675, 1381204.0),
        (733721, 1381212.0),
        (733766, 1381211.0),
        (733804, 1381204.0),
        (733835, 1381219.0),
        (733865, 1381211.0),
        (733885, 1381219.0),
        (733908, 1381204.0),
        (733920, 1381211.0),
        (733937, 1381220.0),
        (734091, 1381211.0),
        (734133, 1381219.0),
        (734181, 1381204.0),
        (734211, 1381720.0),
        (734248, 1381712.0),
        (734277, 1381696.0),
        (734311, 1381704.0),
        (734341, 1381703.0),
        (734372, 1381704.0),
        (734405, 1381703.0),
        (734431, 1381711.0),
        (734456, 1381720.0),
        (734487, 1381703.0),
        (734521, 1381704.0),
        (734554, 1381726.0),
        (734598, 1381704.0),
        (734630, 1381703.0),  # In crbug/1041688 this is where it was placed.
        (734673, 1529888.0),  # This is where it should be.
        (734705, 1529888.0),
        (734739, 1529860.0),
        (734770, 1529860.0),
        (734793, 1529888.0),
        (734829, 1529860.0),
    ]
    for row in sample_data:
      graph_data.Row(id=row[0], value=row[1], parent=test_container_key).put()
    test.UpdateSheriff()
    test.put()
    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([], None))) as m:
      find_anomalies.ProcessTests([test.key])
      self.assertEqual(m.call_args_list, [mock.call(test.test_path)])
    new_anomalies = anomaly.Anomaly.query().fetch()
    self.assertEqual(1, len(new_anomalies))
    self.assertEqual(anomaly.UP, new_anomalies[0].direction)
    self.assertEqual(734631, new_anomalies[0].start_revision)
    self.assertEqual(734673, new_anomalies[0].end_revision)

  def testProcessTest_RefineAnomalyPlacement_MinSize0Max2Elements(self):
    testing_common.AddTests(['ChromiumPerf'], ['linux-perf'],
                            {'sizes': {
                                'method_count': {}
                            }})
    test = utils.TestKey(('ChromiumPerf/linux-perf/sizes/method_count')).get()
    test_container_key = utils.GetTestContainerKey(test.key)
    custom_config = {
        'max_window_size': 10,
        'min_absolute_change': 50,
        'min_relative_change': 0,
        'min_segment_size': 0,
    }
    anomaly_config.AnomalyConfig(
        config=custom_config, patterns=[test.test_path]).put()
    test.UpdateSheriff()
    test.put()
    self.assertEqual(custom_config, anomaly_config.GetAnomalyConfigDict(test))
    sample_data = [
        (6990, 100),
        (6991, 100),
        (6992, 100),
        (6993, 100),
        (6994, 100),
        (6995, 100),
        (6996, 100),
        (6997, 100),
        (6998, 100),
        (6999, 100),
        (7000, 100),
        (7001, 155),
        (7002, 155),
        (7003, 155),
    ]
    for row in sample_data:
      graph_data.Row(id=row[0], value=row[1], parent=test_container_key).put()
    test.UpdateSheriff()
    test.put()
    with mock.patch.object(SheriffConfigClient, 'Match',
                           mock.MagicMock(return_value=([], None))) as m:
      find_anomalies.ProcessTests([test.key])
      self.assertEqual(m.call_args_list, [mock.call(test.test_path)])
    new_anomalies = anomaly.Anomaly.query().fetch()
    self.assertEqual(1, len(new_anomalies))
    self.assertEqual(anomaly.UP, new_anomalies[0].direction)
    self.assertEqual(7001, new_anomalies[0].start_revision)
    self.assertEqual(7001, new_anomalies[0].end_revision)

  def testMakeAnomalyEntity_NoRefBuild(self):
    testing_common.AddTests(
        ['ChromiumPerf'],
        ['linux'], {
            'page_cycler_v2': {
                'cnn': {},
                'yahoo': {},
                'nytimes': {},
            },
        })
    test = utils.TestKey('ChromiumPerf/linux/page_cycler_v2').get()
    testing_common.AddRows(test.test_path, [100, 200, 300, 400])

    alert = find_anomalies._MakeAnomalyEntity(
        _MakeSampleChangePoint(10011, 50, 100),
        test, 'avg',
        self._DataSeries()).get_result()
    self.assertIsNone(alert.ref_test)

  def testMakeAnomalyEntity_RefBuildSlash(self):
    testing_common.AddTests(
        ['ChromiumPerf'],
        ['linux'], {
            'page_cycler_v2': {
                'ref': {},
                'cnn': {},
                'yahoo': {},
                'nytimes': {},
            },
        })
    test = utils.TestKey('ChromiumPerf/linux/page_cycler_v2').get()
    testing_common.AddRows(test.test_path, [100, 200, 300, 400])

    alert = find_anomalies._MakeAnomalyEntity(
        _MakeSampleChangePoint(10011, 50, 100),
        test, 'avg',
        self._DataSeries()).get_result()
    self.assertEqual(alert.ref_test.string_id(),
                     'ChromiumPerf/linux/page_cycler_v2/ref')

  def testMakeAnomalyEntity_RefBuildUnderscore(self):
    testing_common.AddTests(
        ['ChromiumPerf'],
        ['linux'], {
            'page_cycler_v2': {
                'cnn': {},
                'cnn_ref': {},
                'yahoo': {},
                'nytimes': {},
            },
        })
    test = utils.TestKey('ChromiumPerf/linux/page_cycler_v2/cnn').get()
    testing_common.AddRows(test.test_path, [100, 200, 300, 400])

    alert = find_anomalies._MakeAnomalyEntity(
        _MakeSampleChangePoint(10011, 50, 100),
        test, 'avg',
        self._DataSeries()).get_result()
    self.assertEqual(alert.ref_test.string_id(),
                     'ChromiumPerf/linux/page_cycler_v2/cnn_ref')
    self.assertIsNone(alert.display_start)
    self.assertIsNone(alert.display_end)

  def testMakeAnomalyEntity_RevisionRanges(self):
    testing_common.AddTests(
        ['ClankInternal'],
        ['linux'], {
            'page_cycler_v2': {
                'cnn': {},
                'cnn_ref': {},
                'yahoo': {},
                'nytimes': {},
            },
        })
    test = utils.TestKey('ClankInternal/linux/page_cycler_v2/cnn').get()
    testing_common.AddRows(test.test_path, [100, 200, 300, 400])
    for row in graph_data.Row.query():
      # Different enough to ensure it is picked up properly.
      row.r_commit_pos = int(row.value) + 2
      row.put()

    alert = find_anomalies._MakeAnomalyEntity(
        _MakeSampleChangePoint(300, 50, 100),
        test, 'avg',
        self._DataSeries()).get_result()
    self.assertEqual(alert.display_start, 203)
    self.assertEqual(alert.display_end, 302)

  def testMakeAnomalyEntity_AddsOwnership(self):
    data_samples = [
        {
            'type': 'GenericSet',
            'guid': 'eb212e80-db58-4cbd-b331-c2245ecbb826',
            'values': ['alice@chromium.org', 'bob@chromium.org']
        },
        {
            'type': 'GenericSet',
            'guid': 'eb212e80-db58-4cbd-b331-c2245ecbb827',
            'values': ['abc']
        }]

    test_key = utils.TestKey('ChromiumPerf/linux/page_cycler_v2/cnn')
    testing_common.AddTests(
        ['ChromiumPerf'],
        ['linux'], {
            'page_cycler_v2': {
                'cnn': {},
                'cnn_ref': {},
                'yahoo': {},
                'nytimes': {},
            },
        })
    test = test_key.get()
    testing_common.AddRows(test.test_path, [100, 200, 300, 400])

    suite_key = utils.TestKey('ChromiumPerf/linux/page_cycler_v2')
    entity = histogram.SparseDiagnostic(
        data=data_samples[0], test=suite_key, start_revision=1,
        end_revision=sys.maxsize, id=data_samples[0]['guid'],
        name=reserved_infos.OWNERS.name)
    entity.put()

    entity = histogram.SparseDiagnostic(
        data=data_samples[1], test=suite_key, start_revision=1,
        end_revision=sys.maxsize, id=data_samples[1]['guid'],
        name=reserved_infos.BUG_COMPONENTS.name)
    entity.put()

    alert = find_anomalies._MakeAnomalyEntity(
        _MakeSampleChangePoint(10011, 50, 100),
        test, 'avg',
        self._DataSeries()).get_result()

    self.assertEqual(alert.ownership['component'], 'abc')
    self.assertListEqual(alert.ownership['emails'],
                         ['alice@chromium.org', 'bob@chromium.org'])

if __name__ == '__main__':
  unittest.main()
