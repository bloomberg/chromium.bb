# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from memory_inspector.core import memory_map


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
