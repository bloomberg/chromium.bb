# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from infra_libs.ts_mon.common import targets
from infra_libs.ts_mon.protos import metrics_pb2


class DeviceTargetTest(unittest.TestCase):

  def test_populate_target(self):
    pb = metrics_pb2.MetricsData()
    t = targets.DeviceTarget('reg', 'role', 'net', 'host')
    t._populate_target_pb(pb)
    self.assertEquals(pb.network_device.metro, 'reg')
    self.assertEquals(pb.network_device.role, 'role')
    self.assertEquals(pb.network_device.hostgroup, 'net')
    self.assertEquals(pb.network_device.hostname, 'host')
    self.assertEquals(pb.network_device.realm, 'ACQ_CHROME')
    self.assertEquals(pb.network_device.alertable, True)


class TaskTargetTest(unittest.TestCase):

  def test_populate_target(self):
    pb = metrics_pb2.MetricsData()
    t = targets.TaskTarget('serv', 'job', 'reg', 'host')
    t._populate_target_pb(pb)
    self.assertEquals(pb.task.service_name, 'serv')
    self.assertEquals(pb.task.job_name, 'job')
    self.assertEquals(pb.task.data_center, 'reg')
    self.assertEquals(pb.task.host_name, 'host')
    self.assertEquals(pb.task.task_num, 0)
