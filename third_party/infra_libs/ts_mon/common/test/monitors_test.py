# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import httplib2
import json
import os
import tempfile
import unittest

from googleapiclient import errors
import mock

from infra_libs import httplib2_utils
from infra_libs.ts_mon.common import interface
from infra_libs.ts_mon.common import monitors
from infra_libs.ts_mon.common import pb_to_popo
from infra_libs.ts_mon.common import targets
from infra_libs.ts_mon.protos.current import metrics_pb2
from infra_libs.ts_mon.protos.new import metrics_pb2 as new_metrics_pb2
import infra_libs


class MonitorTest(unittest.TestCase):

  def test_send(self):
    m = monitors.Monitor()
    metric1 = metrics_pb2.MetricsData(name='m1')
    with self.assertRaises(NotImplementedError):
      m.send(metric1)

class HttpsMonitorTest(unittest.TestCase):

  def setUp(self):
    interface.state.reset_for_unittest()
    interface.state.use_new_proto = False

  def message(self, pb):
    pb = monitors.Monitor._wrap_proto(pb)
    return json.dumps({'resource': pb_to_popo.convert(pb) })

  def _test_send(self, http):
    mon = monitors.HttpsMonitor('endpoint',
        monitors.CredentialFactory.from_string(':gce'), http=http)
    resp = mock.MagicMock(spec=httplib2.Response, status=200)
    mon._http.request = mock.MagicMock(return_value=[resp, ""])

    metric1 = metrics_pb2.MetricsData(name='m1')
    mon.send(metric1)
    metric2 = metrics_pb2.MetricsData(name='m2')
    mon.send([metric1, metric2])
    collection = metrics_pb2.MetricsCollection(data=[metric1, metric2])
    mon.send(collection)

    mon._http.request.assert_has_calls([
      mock.call('endpoint', method='POST', body=self.message(metric1),
                headers={'Content-Type': 'application/json'}),
      mock.call('endpoint', method='POST',
                body=self.message([metric1, metric2]),
                headers={'Content-Type': 'application/json'}),
      mock.call('endpoint', method='POST', body=self.message(collection),
                headers={'Content-Type': 'application/json'}),
    ])

  def test_default_send(self):
    self._test_send(None)

  def test_http_send(self):
    self._test_send(httplib2.Http())

  def test_instrumented_http_send(self):
    self._test_send(httplib2_utils.InstrumentedHttp('test'))

  @mock.patch('infra_libs.ts_mon.common.monitors.CredentialFactory.'
              'from_string')
  def test_send_resp_failure(self, _load_creds):
    mon = monitors.HttpsMonitor('endpoint',
        monitors.CredentialFactory.from_string('/path/to/creds.p8.json'))
    resp = mock.MagicMock(spec=httplib2.Response, status=400)
    mon._http.request = mock.MagicMock(return_value=[resp, ""])

    metric1 = metrics_pb2.MetricsData(name='m1')
    mon.send(metric1)

    mon._http.request.assert_called_once_with(
        'endpoint',
        method='POST',
        body=self.message(metric1),
        headers={'Content-Type': 'application/json'})

  @mock.patch('infra_libs.ts_mon.common.monitors.CredentialFactory.'
              'from_string')
  def test_send_http_failure(self, _load_creds):
    mon = monitors.HttpsMonitor('endpoint',
        monitors.CredentialFactory.from_string('/path/to/creds.p8.json'))
    mon._http.request = mock.MagicMock(side_effect=ValueError())

    metric1 = metrics_pb2.MetricsData(name='m1')
    mon.send(metric1)

    mon._http.request.assert_called_once_with(
        'endpoint',
        method='POST',
        body=self.message(metric1),
        headers={'Content-Type': 'application/json'})

  def test_send_new(self):
    interface.state.use_new_proto = True
    mon = monitors.HttpsMonitor('endpoint',
        monitors.CredentialFactory.from_string(':gce'), http=httplib2.Http())
    resp = mock.MagicMock(spec=httplib2.Response, status=200)
    mon._http.request = mock.MagicMock(return_value=[resp, ""])

    payload = new_metrics_pb2.MetricsPayload()
    mon.send(payload)

    mon._http.request.assert_has_calls([
      mock.call('endpoint', method='POST',
                body=json.dumps({'payload': pb_to_popo.convert(payload)}),
                headers={'Content-Type': 'application/json'}),
    ])


class PubSubMonitorTest(unittest.TestCase):

  def setUp(self):
    super(PubSubMonitorTest, self).setUp()
    interface.state.target = targets.TaskTarget(
        'test_service', 'test_job', 'test_region', 'test_host')

  @mock.patch('infra_libs.httplib2_utils.InstrumentedHttp', autospec=True)
  @mock.patch('infra_libs.ts_mon.common.monitors.discovery', autospec=True)
  @mock.patch('infra_libs.ts_mon.common.monitors.GoogleCredentials',
              autospec=True)
  def test_init_service_account(self, gc, discovery, instrumented_http):
    m_open = mock.mock_open(read_data='{"type": "service_account"}')
    creds = gc.from_stream.return_value
    scoped_creds = creds.create_scoped.return_value
    http_mock = instrumented_http.return_value
    metric1 = metrics_pb2.MetricsData(name='m1')
    with mock.patch('infra_libs.ts_mon.common.monitors.open', m_open,
                    create=True):
      mon = monitors.PubSubMonitor(
          monitors.CredentialFactory.from_string('/path/to/creds.p8.json'),
          'myproject', 'mytopic')
      mon.send(metric1)

    m_open.assert_called_once_with('/path/to/creds.p8.json', 'r')
    creds.create_scoped.assert_called_once_with(monitors.PubSubMonitor._SCOPES)
    scoped_creds.authorize.assert_called_once_with(http_mock)
    discovery.build.assert_called_once_with('pubsub', 'v1', http=http_mock)
    self.assertEquals(mon._topic, 'projects/myproject/topics/mytopic')

  @mock.patch('infra_libs.httplib2_utils.InstrumentedHttp', autospec=True)
  @mock.patch('infra_libs.ts_mon.common.monitors.discovery', autospec=True)
  @mock.patch('infra_libs.ts_mon.common.monitors.gce.AppAssertionCredentials',
              autospec=True)
  def test_init_gce_credential(self, aac, discovery, instrumented_http):
    creds = aac.return_value
    http_mock = instrumented_http.return_value
    metric1 = metrics_pb2.MetricsData(name='m1')
    mon = monitors.PubSubMonitor(
        monitors.CredentialFactory.from_string(':gce'), 'myproject', 'mytopic')
    mon.send(metric1)

    aac.assert_called_once_with(monitors.PubSubMonitor._SCOPES)
    creds.authorize.assert_called_once_with(http_mock)
    discovery.build.assert_called_once_with('pubsub', 'v1', http=http_mock)
    self.assertEquals(mon._topic, 'projects/myproject/topics/mytopic')

  @mock.patch('infra_libs.httplib2_utils.InstrumentedHttp', autospec=True)
  @mock.patch('infra_libs.ts_mon.common.monitors.discovery', autospec=True)
  @mock.patch('infra_libs.ts_mon.common.monitors.Storage', autospec=True)
  def test_init_storage(self, storage, discovery, instrumented_http):
    storage_inst = mock.Mock()
    storage.return_value = storage_inst
    creds = storage_inst.get.return_value

    m_open = mock.mock_open(read_data='{}')
    http_mock = instrumented_http.return_value
    metric1 = metrics_pb2.MetricsData(name='m1')
    with mock.patch('infra_libs.ts_mon.common.monitors.open', m_open,
                    create=True):
      mon = monitors.PubSubMonitor(
          monitors.CredentialFactory.from_string('/path/to/creds.p8.json'),
          'myproject', 'mytopic')
      mon.send(metric1)

    m_open.assert_called_once_with('/path/to/creds.p8.json', 'r')
    storage_inst.get.assert_called_once_with()
    creds.authorize.assert_called_once_with(http_mock)
    discovery.build.assert_called_once_with('pubsub', 'v1', http=http_mock)
    self.assertEquals(mon._topic, 'projects/myproject/topics/mytopic')

  @mock.patch('infra_libs.ts_mon.common.monitors.CredentialFactory.'
              'from_string')
  @mock.patch('googleapiclient.discovery.build', autospec=True)
  def test_send(self, _discovery, _load_creds):
    mon = monitors.PubSubMonitor(
        monitors.CredentialFactory.from_string('/path/to/creds.p8.json'),
        'myproject', 'mytopic')
    mon._api = mock.MagicMock()
    topic = 'projects/myproject/topics/mytopic'

    metric1 = metrics_pb2.MetricsData(name='m1')
    mon.send(metric1)
    metric2 = metrics_pb2.MetricsData(name='m2')
    mon.send([metric1, metric2])
    collection = metrics_pb2.MetricsCollection(data=[metric1, metric2])
    mon.send(collection)

    def message(pb):
      pb = monitors.Monitor._wrap_proto(pb)
      return {'messages': [{'data': base64.b64encode(pb.SerializeToString())}]}
    publish = mon._api.projects.return_value.topics.return_value.publish
    publish.assert_has_calls([
        mock.call(topic=topic, body=message(metric1)),
        mock.call().execute(num_retries=5),
        mock.call(topic=topic, body=message([metric1, metric2])),
        mock.call().execute(num_retries=5),
        mock.call(topic=topic, body=message(collection)),
        mock.call().execute(num_retries=5),
        ])

  @mock.patch('infra_libs.ts_mon.common.monitors.CredentialFactory.'
              'from_string')
  @mock.patch('googleapiclient.discovery.build', autospec=True)
  def test_send_uninitialized(self, discovery, _load_creds):
    """Test initialization retry logic, and also un-instrumented http path."""
    discovery.side_effect = EnvironmentError()  # Fail initialization.
    mon = monitors.PubSubMonitor(
        monitors.CredentialFactory.from_string('/path/to/creds.p8.json'),
        'myproject', 'mytopic', use_instrumented_http=False)

    metric1 = metrics_pb2.MetricsData(name='m1')
    mon.send(metric1)
    self.assertIsNone(mon._api)

     # Another retry: initialization succeeds.
    discovery.side_effect = None
    mon.send(metric1)

    def message(pb):
      pb = monitors.Monitor._wrap_proto(pb)
      return {'messages': [{'data': base64.b64encode(pb.SerializeToString())}]}

    topic = 'projects/myproject/topics/mytopic'

    publish = mon._api.projects.return_value.topics.return_value.publish
    publish.assert_has_calls([
        mock.call(topic=topic, body=message(metric1)),
        mock.call().execute(num_retries=5),
    ])

  @mock.patch('infra_libs.ts_mon.common.monitors.CredentialFactory.'
              'from_string')
  @mock.patch('googleapiclient.discovery.build', autospec=True)
  def test_send_fails(self, _discovery, _load_creds):
    # Test for an occasional flake of .publish().execute().
    mon = monitors.PubSubMonitor(
        monitors.CredentialFactory.from_string('/path/to/creds.p8.json'),
        'myproject', 'mytopic')
    mon._api = mock.MagicMock()
    topic = 'projects/myproject/topics/mytopic'

    metric1 = metrics_pb2.MetricsData(name='m1')
    mon.send(metric1)

    publish = mon._api.projects.return_value.topics.return_value.publish
    publish.side_effect = ValueError()

    metric2 = metrics_pb2.MetricsData(name='m2')
    mon.send([metric1, metric2])
    collection = metrics_pb2.MetricsCollection(data=[metric1, metric2])
    publish.side_effect = errors.HttpError(
        mock.Mock(status=404, reason='test'), '')
    mon.send(collection)

    # Test that all caught exceptions are specified without errors.
    # When multiple exceptions are specified in the 'except' clause,
    # they are evaluated lazily, and may contain syntax errors.
    # Throwing an uncaught exception forces all exception specs to be
    # evaluated, catching more runtime errors.
    publish.side_effect = Exception('uncaught')
    with self.assertRaises(Exception):
      mon.send(collection)

    def message(pb):
      pb = monitors.Monitor._wrap_proto(pb)
      return {'messages': [{'data': base64.b64encode(pb.SerializeToString())}]}
    publish.assert_has_calls([
        mock.call(topic=topic, body=message(metric1)),
        mock.call().execute(num_retries=5),
        mock.call(topic=topic, body=message([metric1, metric2])),
        mock.call(topic=topic, body=message(collection)),
        ])



class DebugMonitorTest(unittest.TestCase):

  def test_send_file(self):
    with infra_libs.temporary_directory() as temp_dir:
      filename = os.path.join(temp_dir, 'out')
      m = monitors.DebugMonitor(filename)
      metric1 = metrics_pb2.MetricsData(name='m1')
      m.send(metric1)
      metric2 = metrics_pb2.MetricsData(name='m2')
      m.send([metric1, metric2])
      collection = metrics_pb2.MetricsCollection(data=[metric1, metric2])
      m.send(collection)
      with open(filename) as fh:
        output = fh.read()
    self.assertEquals(output.count('data {\n  name: "m1"\n}'), 3)
    self.assertEquals(output.count('data {\n  name: "m2"\n}'), 2)

  def test_send_log(self):
    m = monitors.DebugMonitor()
    metric1 = metrics_pb2.MetricsData(name='m1')
    m.send(metric1)
    metric2 = metrics_pb2.MetricsData(name='m2')
    m.send([metric1, metric2])
    collection = metrics_pb2.MetricsCollection(data=[metric1, metric2])
    m.send(collection)


class NullMonitorTest(unittest.TestCase):

  def test_send(self):
    m = monitors.NullMonitor()
    metric1 = metrics_pb2.MetricsData(name='m1')
    m.send(metric1)


class CredentialFactoryTest(unittest.TestCase):

  def test_from_string(self):
    self.assertIsInstance(monitors.CredentialFactory.from_string(':gce'),
        monitors.GCECredentials)
    self.assertIsInstance(monitors.CredentialFactory.from_string(':appengine'),
        monitors.AppengineCredentials)
    self.assertIsInstance(monitors.CredentialFactory.from_string('/foo'),
        monitors.FileCredentials)

  @mock.patch('infra_libs.httplib2_utils.DelegateServiceAccountCredentials',
              autospec=True)
  def test_actor_credentials(self, mock_creds):
    base = mock.Mock(monitors.CredentialFactory)
    c = monitors.DelegateServiceAccountCredentials('test@example.com', base)

    creds = c.create(['foo'])
    base.create.assert_called_once_with(['https://www.googleapis.com/auth/iam'])
    base.create.return_value.authorize.assert_called_once_with(mock.ANY)
    mock_creds.assert_called_once_with(
        base.create.return_value.authorize.return_value,
        'test@example.com', ['foo'])
    self.assertEqual(mock_creds.return_value, creds)

