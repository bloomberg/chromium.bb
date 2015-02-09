# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import imp
import os.path
import sys
import unittest


def _GetDirAbove(dirname):
  """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
  path = os.path.abspath(__file__)
  while True:
    path, tail = os.path.split(path)
    assert tail
    if tail == dirname:
      return path


try:
  imp.find_module("mojom")
except ImportError:
  sys.path.append(os.path.join(_GetDirAbove("pylib"), "pylib"))
from mojom.generate import pack
from mojom.generate import module as mojom


# TODO(yzshen): Move tests in pack_tests.py here.
class PackTest(unittest.TestCase):

  def testMinVersion(self):
    """Tests that |min_version| is properly set for packed fields."""
    struct = mojom.Struct('test')
    struct.AddField('field_2', mojom.BOOL, 2)
    struct.AddField('field_0', mojom.INT32, 0)
    struct.AddField('field_1', mojom.INT64, 1)
    ps = pack.PackedStruct(struct)

    self.assertEquals("field_0", ps.packed_fields[0].field.name)
    self.assertEquals("field_2", ps.packed_fields[1].field.name)
    self.assertEquals("field_1", ps.packed_fields[2].field.name)

    self.assertEquals(1, ps.packed_fields[0].min_version)
    self.assertEquals(3, ps.packed_fields[1].min_version)
    self.assertEquals(2, ps.packed_fields[2].min_version)

    struct.fields[0].attributes = {'MinVersion': 1}
    ps = pack.PackedStruct(struct)

    self.assertEquals(0, ps.packed_fields[0].min_version)
    self.assertEquals(1, ps.packed_fields[1].min_version)
    self.assertEquals(0, ps.packed_fields[2].min_version)
