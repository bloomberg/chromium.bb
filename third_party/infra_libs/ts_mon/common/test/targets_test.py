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

  def test_update_to_dict(self):
    target = targets.DeviceTarget('reg', 'role', 'net', 'host')
    self.assertEqual({
      'region': 'reg',
      'role': 'role',
      'network': 'net',
      'hostname': 'host'}, target.to_dict())
    target.update({'region': 'other', 'hostname': 'guest'})
    self.assertEqual({
      'region': 'other',
      'role': 'role',
      'network': 'net',
      'hostname': 'guest'}, target.to_dict())

  def test_update_private_field(self):
    target = targets.DeviceTarget('reg', 'role', 'net', 'host')
    with self.assertRaises(AttributeError):
      target.update({'realm': 'boo'})

  def test_update_nonexistent_field(self):
    target = targets.DeviceTarget('reg', 'role', 'net', 'host')
    # Simulate a bug: exporting a non-existent field.
    target._fields += ('bad',)
    with self.assertRaises(AttributeError):
      target.update({'bad': 'boo'})


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

  def test_update_to_dict(self):
    target = targets.TaskTarget('serv', 'job', 'reg', 'host', 5)
    self.assertEqual({
      'service_name': 'serv',
      'job_name': 'job',
      'region': 'reg',
      'hostname': 'host',
      'task_num': 5}, target.to_dict())
    target.update({'region': 'other', 'hostname': 'guest'})
    self.assertEqual({
      'service_name': 'serv',
      'job_name': 'job',
      'region': 'other',
      'hostname': 'guest',
      'task_num': 5}, target.to_dict())

  def test_update_private_field(self):
    target = targets.TaskTarget('serv', 'job', 'reg', 'host')
    with self.assertRaises(AttributeError):
      target.update({'realm': 'boo'})

  def test_update_nonexistent_field(self):
    target = targets.TaskTarget('serv', 'job', 'reg', 'host')
    # Simulate a bug: exporting a non-existent field.
    target._fields += ('bad',)
    with self.assertRaises(AttributeError):
      target.update({'bad': 'boo'})
