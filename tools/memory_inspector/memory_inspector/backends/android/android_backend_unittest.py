# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the backends.android modules."""

import os
import unittest

from memory_inspector.backends import prebuilts_fetcher
from memory_inspector.backends.android import android_backend
from memory_inspector.unittest.mock_adb import mock_adb


_MOCK_ADB_PATH = os.path.dirname(mock_adb.__file__)

_MOCK_DEVICES_OUT = """List of devices attached
0000000000000001\tdevice
0000000000000002\tdevice
"""

_MOCK_MEMDUMP_OUT = ("""[ PID=1]
8000-33000 r-xp 0 private_unevictable=8192 private=8192 shared_app=[] """
    """shared_other_unevictable=147456 shared_other=147456 "/init" [v///fv0D]
33000-35000 r--p 2a000 private_unevictable=4096 private=4096 shared_app=[] """
    """shared_other_unevictable=4096 shared_other=4096 "/init" [Aw==]
35000-37000 rw-p 2c000 private_unevictable=8192 private=8192 shared_app=[] """
    """shared_other_unevictable=0 shared_other=0 "/init" [Aw==]
37000-6d000 rw-p 0 private_unevictable=221184 private=221184 shared_app=[] """
    """shared_other_unevictable=0 shared_other=0 "[heap]" [////////Pw==]
400c5000-400c7000 rw-p 0 private_unevictable=0 private=0 shared_app=[] """
    """shared_other_unevictable=0 shared_other=0 "" [AA==]
400f4000-400f5000 r--p 0 private_unevictable=4096 private=4096 shared_app=[] """
    """shared_other_unevictable=0 shared_other=0 "" [AQ==]
40127000-40147000 rw-s 0 private_unevictable=4096 private=4096 shared_app=[] """
    """shared_other_unevictable=32768 shared_other=32768 "/X" [/////w==]
be8b7000-be8d8000 rw-p 0 private_unevictable=8192 private=8192 shared_app=[] """
    """shared_other_unevictable=4096 shared_other=12288 "[stack]" [AAAA4AE=]
ffff0000-ffff1000 r-xp 0 private_unevictable=0 private=0 shared_app=[] """
    """shared_other_unevictable=0 shared_other=0 "[vectors]" [AA==]""")

_MOCK_DUMPHEAP_OUT = """Android Native Heap Dump v1.0

Total memory: 1608601
Allocation records: 2

z 1  sz    100    num    2  bt 9dcd1000 9dcd2000 b570e100 b570e000
z 0  sz    1000   num    3  bt b570e100 00001234 b570e200
MAPS
9dcd0000-9dcd6000 r-xp 00000000 103:00 815       /system/lib/libnbaio.so
9dcd6000-9dcd7000 r--p 00005000 103:00 815       /system/lib/libnbaio.so
9dcd7000-9dcd8000 rw-p 00006000 103:00 815       /system/lib/libnbaio.so
b570e000-b598c000 r-xp 00000000 103:00 680       /system/lib/libart.so
b598c000-b598d000 ---p 00000000 00:00 0
b598d000-b5993000 r--p 0027e000 103:00 680       /system/lib/libart.so
b5993000-b5994000 rw-p 00284000 103:00 680       /system/lib/libart.so
END"""

_MOCK_PS_EXT_OUT = """
{
  "time": { "ticks": 1000100, "rate": 100},
  "mem": {
      "MemTotal:": 998092, "MemFree:": 34904,
      "Buffers:": 24620, "Cached:": 498916 },
  "cpu": [{"usr": 32205, "sys": 49826, "idle": 7096196}],
  "processes": {
    "1": {
       "name": "foo", "n_threads": 42, "start_time": 1000000, "user_time": 82,
       "sys_time": 445, "min_faults": 0, "maj_faults": 0, "vm_rss": 528},
    "2": {
       "name": "bar", "n_threads": 142, "start_time": 1, "user_time": 82,
       "sys_time": 445, "min_faults": 0, "maj_faults": 0, "vm_rss": 528}}
}
"""


class AndroidBackendTest(unittest.TestCase):
  def setUp(self):
    prebuilts_fetcher.in_test_harness = True
    self._mock_adb = mock_adb.MockAdb()
    planned_adb_responses = {
      'devices': _MOCK_DEVICES_OUT,
      'shell getprop ro.product.model': 'Mock device',
      'shell getprop ro.build.type': 'userdebug',
      'root': 'adbd is already running as root',
      'shell /data/local/tmp/ps_ext': _MOCK_PS_EXT_OUT,
      'shell /data/local/tmp/memdump': _MOCK_MEMDUMP_OUT,
      'shell cat "/data/local/tmp/heap': _MOCK_DUMPHEAP_OUT,
      'shell test -e "/data/local/tmp/heap': '0',
    }
    for (cmd, response) in planned_adb_responses.iteritems():
      self._mock_adb.PrepareResponse(cmd, response)
    self._mock_adb.Start()

  def runTest(self):
    ab = android_backend.AndroidBackend()

    # Test settings load/store logic and setup the mock adb path.
    self.assertTrue('adb_path' in ab.settings.expected_keys)
    ab.settings['adb_path'] = _MOCK_ADB_PATH
    self.assertEqual(ab.settings['adb_path'], _MOCK_ADB_PATH)

    # Test device enumeration.
    devices = list(ab.EnumerateDevices())
    self.assertEqual(len(devices), 2)
    self.assertEqual(devices[0].name, 'Mock device')
    self.assertEqual(devices[0].id, '0000000000000001')
    self.assertEqual(devices[1].name, 'Mock device')
    self.assertEqual(devices[1].id, '0000000000000002')

    # Initialize device (checks that sha1 are checked in).
    device = devices[0]
    device.Initialize()

    # Test process enumeration.
    processes = list(device.ListProcesses())
    self.assertEqual(len(processes), 2)
    self.assertEqual(processes[0].pid, 1)
    self.assertEqual(processes[0].name, 'foo')
    self.assertEqual(processes[1].pid, 2)
    self.assertEqual(processes[1].name, 'bar')

    # Test process stats.
    stats = processes[0].GetStats()
    self.assertEqual(stats.threads, 42)
    self.assertEqual(stats.cpu_usage, 0)
    self.assertEqual(stats.run_time, 1)
    self.assertEqual(stats.vm_rss, 528)
    self.assertEqual(stats.page_faults, 0)

    # Test memdump parsing.
    mmaps = processes[0].DumpMemoryMaps()
    self.assertEqual(len(mmaps), 9)
    self.assertIsNone(mmaps.Lookup(0))
    self.assertIsNone(mmaps.Lookup(7999))
    self.assertIsNotNone(mmaps.Lookup(0x8000))
    self.assertIsNotNone(mmaps.Lookup(0x32FFF))
    self.assertIsNotNone(mmaps.Lookup(0x33000))
    self.assertIsNone(mmaps.Lookup(0x6d000))
    self.assertIsNotNone(mmaps.Lookup(0xffff0000))

    entry = mmaps.Lookup(0xbe8b7000)
    self.assertIsNotNone(entry)
    self.assertEqual(entry.start, 0xbe8b7000)
    self.assertEqual(entry.end, 0xbe8d7fff)
    self.assertEqual(entry.prot_flags, 'rw-p')
    self.assertEqual(entry.priv_dirty_bytes, 8192)
    self.assertEqual(entry.priv_clean_bytes, 0)
    self.assertEqual(entry.shared_dirty_bytes, 4096)
    self.assertEqual(entry.shared_clean_bytes, 8192)
    for i in xrange(29):
      self.assertFalse(entry.IsPageResident(i))
    for i in xrange(29, 33):
      self.assertTrue(entry.IsPageResident(i))

    # Test dumpheap parsing.
    heap = processes[0].DumpNativeHeap()
    self.assertEqual(len(heap.allocations), 2)

    alloc_1 = heap.allocations[0]
    self.assertEqual(alloc_1.size, 100)
    self.assertEqual(alloc_1.count, 2)
    self.assertEqual(alloc_1.total_size, 200)
    self.assertEqual(alloc_1.stack_trace.depth, 4)
    self.assertEqual(alloc_1.stack_trace[0].exec_file_rel_path,
                     '/system/lib/libnbaio.so')
    self.assertEqual(alloc_1.stack_trace[0].address, 0x9dcd1000)
    self.assertEqual(alloc_1.stack_trace[0].offset, 0x1000)
    self.assertEqual(alloc_1.stack_trace[1].offset, 0x2000)
    self.assertEqual(alloc_1.stack_trace[2].exec_file_rel_path,
                     '/system/lib/libart.so')
    self.assertEqual(alloc_1.stack_trace[2].offset, 0x100)
    self.assertEqual(alloc_1.stack_trace[3].offset, 0)

    alloc_2 = heap.allocations[1]
    self.assertEqual(alloc_2.size, 1000)
    self.assertEqual(alloc_2.count, 3)
    self.assertEqual(alloc_2.total_size, 3000)
    self.assertEqual(alloc_2.stack_trace.depth, 3)
    # 0x00001234 is not present in the maps. It should be parsed anyways but
    # no executable info is expected.
    self.assertEqual(alloc_2.stack_trace[1].address, 0x00001234)
    self.assertIsNone(alloc_2.stack_trace[1].exec_file_rel_path)
    self.assertIsNone(alloc_2.stack_trace[1].offset)

  def tearDown(self):
    self._mock_adb.Stop()
