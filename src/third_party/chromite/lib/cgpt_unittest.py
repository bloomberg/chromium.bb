# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test chromite.lib.cgpt"""

from __future__ import print_function

from chromite.lib import cgpt
from chromite.lib import cros_test_lib


CGPT_SHOW_OUTPUT = """       start        size    part  contents
   0           1          PMBR (Boot GUID: A3707625-23E2-2140-B798-94F309618569)
           1           1          Pri GPT header
           2          32          Pri GPT table
     5234688     4194304       1  Label: "STATE"
                                  Type: EBD0A0A2-B9E5-4433-87C0-68B6B72699C7
                                  UUID: 9B03635E-BD25-6E4F-91BE-141B60600DC6
                                  Attr: [0]
       20480       32768       2  Label: "KERN-A"
                                  Type: FE3A2A5D-4F32-41A7-B725-ACCC3285A309
                                  UUID: 3A975D8F-7D4D-F047-B9A5-8D8C0745798E
                                  Attr: [1ff]
      319488     4915200       3  Label: "ROOT-A"
                                  Type: 3CB8E202-3B7E-47DD-8A3C-7FF2A13CFCEC
                                  UUID: 3E167F70-198B-E947-8924-6812593E9BD9
                                  Attr: [0]
       53248       32768       4  Label: "KERN-B"
                                  Type: FE3A2A5D-4F32-41A7-B725-ACCC3285A309
                                  UUID: 15ADD9BD-05BE-434A-BB2D-3CB2D344F033
                                  Attr: [0]
      315392        4096       5  Label: "ROOT-B"
                                  Type: 3CB8E202-3B7E-47DD-8A3C-7FF2A13CFCEC
                                  UUID: 453443DB-E022-CA4F-A3A6-A92EC82A5EEE
                                  Attr: [0]
       16448           1       6  Label: "KERN-C"
                                  Type: FE3A2A5D-4F32-41A7-B725-ACCC3285A309
                                  UUID: 75BD5787-A268-FD42-BFA4-33BB7406723A
                                  Attr: [0]
       16449           1       7  Label: "ROOT-C"
                                  Type: 3CB8E202-3B7E-47DD-8A3C-7FF2A13CFCEC
                                  UUID: C4FBFA62-7AB2-144D-84A1-604EF03D5B4A
                                  Attr: [0]
       86016       32768       8  Label: "OEM"
                                  Type: EBD0A0A2-B9E5-4433-87C0-68B6B72699C7
                                  UUID: BF7488C0-C6B0-D446-B3D1-7A5D1A8DDB90
                                  Attr: [0]
       16450           1       9  Label: "reserved"
                                  Type: 2E0A753D-9E48-43B0-8337-B15192CB1B5E
                                  UUID: 2697A3F2-37A1-1348-8A90-52B83C812B69
                                  Attr: [0]
       16451           1      10  Label: "reserved"
                                  Type: 2E0A753D-9E48-43B0-8337-B15192CB1B5E
                                  UUID: D711DDF7-0456-BB41-8C74-B166151A3B45
                                  Attr: [0]
          64       16384      11  Label: "RWFW"
                                  Type: CAB6E88E-ABF3-4102-A07A-D4BB9BE3C1D3
                                  UUID: 27313CD1-E158-9D41-AD82-F13D2468C44C
                                  Attr: [0]
      249856       65536      12  Label: "EFI-SYSTEM"
                                  Type: C12A7328-F81F-11D2-BA4B-00A0C93EC93B
                                  UUID: A3707625-23E2-2140-B798-94F309618569
                                  Attr: [0]
     9428992          32          Sec GPT table
     9429024           1          Sec GPT header"""


class TestDisk(cros_test_lib.RunCommandTestCase):
  """Test Disk class."""

  def getMockDisk(self):
    """Returns new Disk based on CGPT_SHOW_OUTPUT."""
    self.rc.SetDefaultCmdResult(output=CGPT_SHOW_OUTPUT)
    return cgpt.Disk.FromImage('foo')

  def testDiskFromImageEmpty(self):
    """Test ReadGpt when cgpt doesn't return an expected list."""
    with self.assertRaises(cgpt.Error):
      cgpt.Disk.FromImage('foo')

  def testDiskFromImage(self):
    """Test ReadGpt with mock cgpt output."""
    disk = self.getMockDisk()

    self.assertCommandCalled(['cgpt', 'show', '-n', 'foo'], capture_output=True)

    self.assertEqual(len(disk.partitions), 12)

    self.assertEqual(disk.partitions[3],
                     cgpt.Partition(part_num=3,
                                    label='ROOT-A',
                                    start=319488,
                                    size=4915200,
                                    part_type='3CB8E202-3B7E-47DD-'
                                              '8A3C-7FF2A13CFCEC',
                                    uuid='3E167F70-198B-E947-8924-6812593E9BD9',
                                    attr='[0]'))

  def testGetPartitionByLabel(self):
    """Test that mocked disk has all expected partitions."""
    disk = self.getMockDisk()

    for label, part_num in (('STATE', 1),
                            ('KERN-A', 2),
                            ('ROOT-A', 3),
                            ('KERN-B', 4),
                            ('ROOT-B', 5),
                            ('KERN-C', 6),
                            ('ROOT-C', 7),
                            ('OEM', 8),
                            ('RWFW', 11),
                            ('EFI-SYSTEM', 12)):
      self.assertEqual(disk.GetPartitionByLabel(label).part_num, part_num)

  def testGetPartitionByLabelMulitpleLabels(self):
    """Test MultiplePartitionLabel is raised on duplicate label 'reserved'."""
    disk = self.getMockDisk()

    with self.assertRaises(cgpt.MultiplePartitionLabel):
      disk.GetPartitionByLabel('reserved')

  def testGetPartitionByLabelMissingKey(self):
    """Test KeyError is raised on a non-existent label."""
    disk = self.getMockDisk()

    with self.assertRaises(KeyError):
      disk.GetPartitionByLabel('bar')
