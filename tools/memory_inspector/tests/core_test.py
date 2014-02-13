#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the core backends modules."""

import logging
import os
import sys
import unittest

sys.path.append(os.path.abspath(os.path.join(__file__, os.pardir, os.pardir)))

from memory_inspector.core import backends
from memory_inspector.core import memory_map
from memory_inspector.core import stacktrace


class MockDevice(backends.Device):  # pylint: disable=W0223
  def __init__(self, backend, device_id):
    super(MockDevice, self).__init__(backend)
    self.device_id = device_id

  @property
  def name(self):
    return "Mock Device %s" % self.device_id

  @property
  def id(self):
    return self.device_id


class MockBackend(backends.Backend):
  _SETTINGS = {'key_1': 'key descritpion 1'}
  def __init__(self, backend_name):
    super(MockBackend, self).__init__(MockBackend._SETTINGS)
    self.backend_name = backend_name

  def EnumerateDevices(self):
    yield MockDevice(self, 'device-1')
    yield MockDevice(self, 'device-2')

  @property
  def name(self):
    return self.backend_name


class BackendRegisterTest(unittest.TestCase):
  def runTest(self):
    mock_backend_1 = MockBackend('mock-backend-1')
    mock_backend_2 = MockBackend('mock-backend-2')
    self.assertEqual(mock_backend_1.settings['key_1'], 'key descritpion 1')
    backends.Register(mock_backend_1)
    backends.Register(mock_backend_2)
    devices = list(backends.ListDevices())
    self.assertEqual(len(devices), 4)
    self.assertIsNotNone(backends.GetDevice('mock-backend-1', 'device-1'))
    self.assertIsNotNone(backends.GetDevice('mock-backend-1', 'device-2'))
    self.assertIsNotNone(backends.GetDevice('mock-backend-2', 'device-1'))
    self.assertIsNotNone(backends.GetDevice('mock-backend-2', 'device-1'))
    self.assertTrue('key_1' in mock_backend_1.settings)


class MemoryMapTest(unittest.TestCase):
  def runTest(self):
    mmap = memory_map.Map()
    map_entry1 = memory_map.MapEntry(4096, 8191, 'rw--', '/foo', 0)
    map_entry2 = memory_map.MapEntry(65536, 81919, 'rw--', '/bar', 4096)

    # Test the de-offset logic.
    self.assertEqual(map_entry1.GetRelativeOffset(4096), 0)
    self.assertEqual(map_entry1.GetRelativeOffset(4100), 4)
    self.assertEqual(map_entry2.GetRelativeOffset(65536), 4096)

    # Test the page-resident logic.
    map_entry2.resident_pages = [5] # 5 -> 101b.
    self.assertTrue(map_entry2.IsPageResident(0))
    self.assertFalse(map_entry2.IsPageResident(1))
    self.assertTrue(map_entry2.IsPageResident(2))

    # Test the lookup logic.
    mmap.Add(map_entry1)
    mmap.Add(map_entry2)
    self.assertIsNone(mmap.Lookup(1024))
    self.assertEqual(mmap.Lookup(4096), map_entry1)
    self.assertEqual(mmap.Lookup(6000), map_entry1)
    self.assertEqual(mmap.Lookup(8191), map_entry1)
    self.assertIsNone(mmap.Lookup(8192))
    self.assertIsNone(mmap.Lookup(65535))
    self.assertEqual(mmap.Lookup(65536), map_entry2)
    self.assertEqual(mmap.Lookup(67000), map_entry2)
    self.assertEqual(mmap.Lookup(81919), map_entry2)
    self.assertIsNone(mmap.Lookup(81920))


class StacktraceTest(unittest.TestCase):
  def runTest(self):
    st = stacktrace.Stacktrace()
    frame_1 = stacktrace.Frame(20)
    frame_2 = stacktrace.Frame(24)
    frame_2.SetExecFileInfo('/foo/bar.so', 0)
    self.assertEqual(frame_2.exec_file_name, 'bar.so')
    st.Add(frame_1)
    st.Add(frame_1)
    st.Add(frame_2)
    st.Add(frame_1)
    self.assertEqual(st.depth, 4)


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR,
      format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  unittest.main()
