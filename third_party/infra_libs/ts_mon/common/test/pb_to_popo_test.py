# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from infra_libs.ts_mon.common import monitors
from infra_libs.ts_mon.common import pb_to_popo
from infra_libs.ts_mon.protos.current import acquisition_network_device_pb2
from infra_libs.ts_mon.protos.current import acquisition_task_pb2
from infra_libs.ts_mon.protos.current import metrics_pb2

class PbToPopoTest(unittest.TestCase):

  def test_convert(self):
    task = acquisition_task_pb2.Task(service_name='service')
    network_device = acquisition_network_device_pb2.NetworkDevice(
                         hostname='host', alertable=True)
    metric1 = metrics_pb2.MetricsData(
        name='m1', counter=200, task=task,
        units=metrics_pb2.MetricsData.Units.Value('SECONDS'))
    metric2 = metrics_pb2.MetricsData(name='m2', network_device=network_device,
                                      cumulative_double_value=123.456)
    collection = metrics_pb2.MetricsCollection(data=[metric1, metric2],
                                               start_timestamp_us=12345)

    popo = pb_to_popo.convert(collection)
    expected = {
      'data': [
        {
          'name': 'm1',
          'counter': 200L,
          'task': { 'service_name': 'service' },
          'units': 1
        },
        {
          'name': 'm2',
          'cumulative_double_value': 123.456,
          'network_device': { 'hostname': 'host', 'alertable': True }
        }
      ],
      'start_timestamp_us': 12345L
    }
    self.assertDictEqual(expected, popo)

