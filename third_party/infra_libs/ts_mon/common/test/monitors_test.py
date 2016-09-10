# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import os
import tempfile
import unittest

import mock

from infra_libs.ts_mon.common import monitors
from infra_libs.ts_mon.protos import metrics_pb2
import infra_libs


class MonitorTest(unittest.TestCase):

  def test_send(self):
    m = monitors.Monitor()
    metric1 = metrics_pb2.MetricsData(name='m1')
    with self.assertRaises(NotImplementedError):
      m.send(metric1)


class PubSubMonitorTest(unittest.TestCase):

  @mock.patch('infra_libs.httplib2_utils.InstrumentedHttp', autospec=True)
  @mock.patch('infra_libs.ts_mon.common.monitors.discovery', autospec=True)
  @mock.patch('infra_libs.ts_mon.common.monitors.GoogleCredentials',
              autospec=True)
  def test_init_service_account(self, gc, discovery, instrumented_http):
    m_open = mock.mock_open(read_data='{"type": "service_account"}')
    creds = gc.from_stream.return_value
    scoped_creds = creds.create_scoped.return_value
    http_mock = instrumented_http.return_value
    with mock.patch('infra_libs.ts_mon.common.monitors.open', m_open,
                    create=True):
      mon = monitors.PubSubMonitor('/path/to/creds.p8.json', 'myproject',
                                   'mytopic')

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
    mon = monitors.PubSubMonitor(':gce', 'myproject', 'mytopic')

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
    with mock.patch('infra_libs.ts_mon.common.monitors.open', m_open,
                    create=True):
      mon = monitors.PubSubMonitor('/path/to/creds.p8.json', 'myproject',
                                   'mytopic')

    m_open.assert_called_once_with('/path/to/creds.p8.json', 'r')
    storage_inst.get.assert_called_once_with()
    creds.authorize.assert_called_once_with(http_mock)
    discovery.build.assert_called_once_with('pubsub', 'v1', http=http_mock)
    self.assertEquals(mon._topic, 'projects/myproject/topics/mytopic')

  @mock.patch('infra_libs.ts_mon.common.monitors.PubSubMonitor.'
              '_load_credentials', autospec=True)
  @mock.patch('apiclient.discovery.build', autospec=True)
  def test_send(self, _discovery, _load_creds):
    mon = monitors.PubSubMonitor('/path/to/creds.p8.json', 'myproject',
                                 'mytopic')
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

  @mock.patch('infra_libs.ts_mon.common.monitors.PubSubMonitor.'
              '_load_credentials', autospec=True)
  @mock.patch('apiclient.discovery.build', autospec=True)
  def test_send_uninitialized(self, discovery, _load_creds):
    """Test initialization retry logic, and also un-instrumented http path."""
    discovery.side_effect = EnvironmentError()  # Fail initialization.
    mon = monitors.PubSubMonitor('/path/to/creds.p8.json', 'myproject',
                                 'mytopic', use_instrumented_http=False)

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
