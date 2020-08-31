# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the key_value_store module."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.utils import key_value_store


class TestKeyValueFiles(cros_test_lib.TempDirTestCase):
  """Tests handling of key/value files."""

  def setUp(self):
    self.contents = """# A comment !@
A = 1
AA= 2
AAA =3
AAAA\t=\t4
AAAAA\t   \t=\t   5
AAAAAA = 6     \t\t# Another comment
\t
\t# Aerith lives!
C = 'D'
CC= 'D'
CCC ='D'
\x20
 \t# monsters go boom #
E \t= "Fxxxxx" # Blargl
EE= "Faaa\taaaa"\x20
EEE ="Fk  \t  kkkk"\t
Q = "'q"
\tQQ ="q'"\x20
 QQQ='"q"'\t
R = "r
"
RR = "rr
rrr"
RRR = 'rrr
 RRRR
 rrr
'
SSS=" ss
'ssss'
ss"
T="
ttt"
"""
    self.expected = {
        'A': '1',
        'AA': '2',
        'AAA': '3',
        'AAAA': '4',
        'AAAAA': '5',
        'AAAAAA': '6',
        'C': 'D',
        'CC': 'D',
        'CCC': 'D',
        'E': 'Fxxxxx',
        'EE': 'Faaa\taaaa',
        'EEE': 'Fk  \t  kkkk',
        'Q': "'q",
        'QQ': "q'",
        'QQQ': '"q"',
        'R': 'r\n',
        'RR': 'rr\nrrr',
        'RRR': 'rrr\n RRRR\n rrr\n',
        'SSS': " ss\n'ssss'\nss",
        'T': '\nttt'
    }

    self.conf_file = os.path.join(self.tempdir, 'file.conf')
    osutils.WriteFile(self.conf_file, self.contents)

  def _RunAndCompare(self, test_input, multiline):
    result = key_value_store.LoadFile(test_input, multiline=multiline)
    self.assertEqual(self.expected, result)

  def testLoadFilePath(self):
    """Verify reading a simple file works"""
    self._RunAndCompare(self.conf_file, True)

  def testLoadData(self):
    """Verify passing in a string works."""
    result = key_value_store.LoadData(self.contents, multiline=True)
    self.assertEqual(self.expected, result)

  def testLoadFileObject(self):
    """Verify passing in open file object works."""
    with open(self.conf_file) as f:
      self._RunAndCompare(f, True)

  def testNoMultlineValues(self):
    """Verify exception is thrown when multiline is disabled."""
    self.assertRaises(ValueError, self._RunAndCompare, self.conf_file, False)
