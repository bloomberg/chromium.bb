#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_portage_upgrade.py."""

import exceptions
import mox
import os
import tempfile

from chromite.lib import cros_test_lib
from chromite.lib import table
from chromite.scripts import merge_package_status as mps

# pylint: disable=W0212,R0904

class MergeTest(cros_test_lib.OutputTestCase, cros_test_lib.TempDirTestCase):
  """Test the functionality of merge_package_status."""

  # These taken from cros_portage_upgrade column names.
  COL_VER_x86 = 'Current x86 Version'
  COL_VER_arm = 'Current arm Version'

  COL_CROS_TARGET = 'ChromeOS Root Target'
  COL_HOST_TARGET = 'Host Root Target'
  COL_CMP_ARCH = 'Comparing arm vs x86 Versions'

  COLUMNS = [mps.COL_PACKAGE,
             mps.COL_SLOT,
             mps.COL_OVERLAY,
             COL_VER_x86,
             COL_VER_arm,
             mps.COL_TARGET,
             ]

  ROW0 = {mps.COL_PACKAGE: 'lib/foo',
          mps.COL_SLOT: '0',
          mps.COL_OVERLAY: 'portage',
          COL_VER_x86: '1.2.3',
          COL_VER_arm: '1.2.3',
          mps.COL_TARGET: 'chromeos-dev virtual/target-sdk'}
  ROW0_FINAL = dict(ROW0)
  ROW0_FINAL[mps.COL_PACKAGE] = ROW0[mps.COL_PACKAGE] + ':' + ROW0[mps.COL_SLOT]
  ROW0_FINAL[COL_CROS_TARGET] = 'chromeos-dev'
  ROW0_FINAL[COL_HOST_TARGET] = 'virtual/target-sdk'
  ROW0_FINAL[COL_CMP_ARCH] = 'same'

  ROW1 = {mps.COL_PACKAGE: 'dev/bar',
          mps.COL_SLOT: '0',
          mps.COL_OVERLAY: 'chromiumos-overlay',
          COL_VER_x86: '1.2.3',
          COL_VER_arm: '1.2.3-r1',
          mps.COL_TARGET: 'virtual/target-os'}
  ROW1_FINAL = dict(ROW1)
  ROW1_FINAL[COL_CROS_TARGET] = 'virtual/target-os'
  ROW1_FINAL[COL_HOST_TARGET] = ''
  ROW1_FINAL[COL_CMP_ARCH] = 'different'

  ROW2 = {mps.COL_PACKAGE: 'lib/foo',
          mps.COL_SLOT: '1',
          mps.COL_OVERLAY: 'portage',
          COL_VER_x86: '1.2.3',
          COL_VER_arm: '',
          mps.COL_TARGET: 'chromeos-dev world'}
  ROW2_FINAL = dict(ROW2)
  ROW2_FINAL[mps.COL_PACKAGE] = ROW2[mps.COL_PACKAGE] + ':' + ROW2[mps.COL_SLOT]
  ROW2_FINAL[COL_CROS_TARGET] = 'chromeos-dev'
  ROW2_FINAL[COL_HOST_TARGET] = 'world'
  ROW2_FINAL[COL_CMP_ARCH] = ''

  def setUp(self):
    self._table = self._CreateTableWithRows(self.COLUMNS,
                                            [self.ROW0, self.ROW1, self.ROW2])

  def _CreateTableWithRows(self, cols, rows):
    mytable = table.Table(list(cols))
    if rows:
      for row in rows:
        mytable.AppendRow(dict(row))
    return mytable

  def _CreateTmpCsvFile(self, table_obj):
    _fd, path = tempfile.mkstemp(text=True)
    tmpfile = open(path, 'w')
    table_obj.WriteCSV(tmpfile)
    tmpfile.close()
    return path

  def _GetFullRowFor(self, row, cols):
    return dict((col, row.get(col, '')) for col in cols)

  def assertRowsEqual(self, row1, row2):
    # Determine column superset
    cols = set(row1.keys() + row2.keys())
    self.assertEquals(self._GetFullRowFor(row1, cols),
                      self._GetFullRowFor(row2, cols))

  def testGetCrosTargetRank(self):
    cros_rank = mps._GetCrosTargetRank('virtual/target-os')
    crosdev_rank = mps._GetCrosTargetRank('chromeos-dev')
    crostest_rank = mps._GetCrosTargetRank('chromeos-test')
    other_rank = mps._GetCrosTargetRank('foobar')

    self.assertTrue(cros_rank)
    self.assertTrue(crosdev_rank)
    self.assertTrue(crostest_rank)
    self.assertFalse(other_rank)
    self.assertTrue(cros_rank < crosdev_rank)
    self.assertTrue(crosdev_rank < crostest_rank)

  def testProcessTargets(self):
    test_in = [
        ['virtual/target-os', 'chromeos-dev'],
        ['world', 'virtual/target-os', 'chromeos-dev', 'chromeos-test'],
        ['world', 'virtual/target-sdk', 'chromeos-dev', 'chromeos-test'],
        ]
    test_out = [
        ['chromeos-dev'],
        ['chromeos-test', 'world'],
        ['chromeos-test', 'virtual/target-sdk', 'world'],
        ]
    test_rev_out = [
        ['virtual/target-os'],
        ['virtual/target-os', 'world'],
        ['chromeos-dev', 'virtual/target-sdk', 'world'],
        ]

    for targets, good_out, rev_out in zip(test_in, test_out, test_rev_out):
      output = mps.ProcessTargets(targets)
      self.assertEquals(output, good_out)
      output = mps.ProcessTargets(targets, reverse_cros=True)
      self.assertEquals(output, rev_out)

  def testLoadTable(self):
    path = self._CreateTmpCsvFile(self._table)
    csv_table = mps.LoadTable(path)
    self.assertEquals(self._table, csv_table)
    os.unlink(path)

  def testLoadAndMergeTables(self):
    # Create a second table to merge with standard table.
    row0_2 = {mps.COL_PACKAGE: 'lib/foo',
              mps.COL_SLOT: '1',
              mps.COL_OVERLAY: 'portage',
              self.COL_VER_arm: '1.2.4',
              mps.COL_TARGET: 'chromeos-dev world'}
    row1_2 = {mps.COL_PACKAGE: 'dev/bar',
              mps.COL_SLOT: '0',
              mps.COL_OVERLAY: 'chromiumos-overlay',
              self.COL_VER_arm: '1.2.3-r1',
              mps.COL_TARGET: 'chromeos-test'}
    row2_2 = {mps.COL_PACKAGE: 'dev/newby',
              mps.COL_SLOT: '2',
              mps.COL_OVERLAY: 'chromiumos-overlay',
              self.COL_VER_arm: '3.2.1',
              mps.COL_TARGET: 'virtual/target-os virtual/target-sdk'}
    cols = [col for col in self.COLUMNS if col != self.COL_VER_x86]
    table_2 = self._CreateTableWithRows(cols,
                                        [row0_2, row1_2, row2_2])

    # Minor patch to main table for this test.
    self._table.GetRowByIndex(2)[self.COL_VER_arm] = '1.2.4'

    with self.OutputCapturer():
      path1 = self._CreateTmpCsvFile(self._table)
      path2 = self._CreateTmpCsvFile(table_2)

      combined_table1 = mps.MergeTables([self._table, table_2])
      combined_table2 = mps.LoadAndMergeTables([path1, path2])

    final_row0 = {mps.COL_PACKAGE: 'dev/bar',
                  mps.COL_SLOT: '0',
                  mps.COL_OVERLAY: 'chromiumos-overlay',
                  self.COL_VER_x86: '1.2.3',
                  self.COL_VER_arm: '1.2.3-r1',
                  mps.COL_TARGET: 'virtual/target-os'}
    final_row1 = {mps.COL_PACKAGE: 'dev/newby',
                  mps.COL_SLOT: '2',
                  mps.COL_OVERLAY: 'chromiumos-overlay',
                  self.COL_VER_x86: '',
                  self.COL_VER_arm: '3.2.1',
                  mps.COL_TARGET: 'virtual/target-os virtual/target-sdk'}
    final_row2 = {mps.COL_PACKAGE: 'lib/foo',
                  mps.COL_SLOT: '0',
                  mps.COL_OVERLAY: 'portage',
                  self.COL_VER_x86: '1.2.3',
                  self.COL_VER_arm: '1.2.3',
                  mps.COL_TARGET: 'chromeos-dev virtual/target-sdk'}
    final_row3 = {mps.COL_PACKAGE: 'lib/foo',
                  mps.COL_SLOT: '1',
                  mps.COL_OVERLAY: 'portage',
                  self.COL_VER_x86: '1.2.3',
                  self.COL_VER_arm: '1.2.4',
                  mps.COL_TARGET: 'chromeos-dev world'}

    final_rows = (final_row0, final_row1, final_row2, final_row3)
    for ix, row_out in enumerate(final_rows):
      self.assertRowsEqual(row_out, combined_table1[ix])
      self.assertRowsEqual(row_out, combined_table2[ix])

    os.unlink(path1)
    os.unlink(path2)

  def testFinalizeTable(self):
    self.assertEquals(3, self._table.GetNumRows())
    self.assertEquals(len(self.COLUMNS), self._table.GetNumColumns())

    with self.OutputCapturer():
      mps.FinalizeTable(self._table)

    self.assertEquals(3, self._table.GetNumRows())
    self.assertEquals(len(self.COLUMNS) + 3, self._table.GetNumColumns())

    final_rows = (self.ROW0_FINAL, self.ROW1_FINAL, self.ROW2_FINAL)
    for ix, row_out in enumerate(final_rows):
      self.assertRowsEqual(row_out, self._table[ix])

class MainTest(cros_test_lib.MoxOutputTestCase):
  """Test argument handling at the main method level."""

  def testHelp(self):
    """Test that --help is functioning"""
    with self.OutputCapturer() as output:
      # Running with --help should exit with code==0
      try:
        mps.main(['--help'])
      except exceptions.SystemExit as e:
        self.assertEquals(e.args[0], 0)

    # Verify that a message beginning with "Usage: " was printed
    stdout = output.GetStdout()
    self.assertTrue(stdout.startswith('Usage: '),
                    msg='Expected output starting with "Usage: " but got:\n%s' %
                    stdout)

  def testMissingOut(self):
    """Test that running without --out exits with an error."""
    with self.OutputCapturer():
      # Running without --out should exit with code!=0
      try:
        mps.main([])
      except exceptions.SystemExit as e:
        self.assertNotEquals(e.args[0], 0)

    # Verify that output ends in error.
    self.AssertOutputEndsInError()

  def testMissingPackage(self):
    """Test that running without a package argument exits with an error."""
    with self.OutputCapturer():
      # Running without a package should exit with code!=0
      try:
        mps.main(['--out=any-out'])
      except exceptions.SystemExit as e:
        self.assertNotEquals(e.args[0], 0)

    # Verify that output ends in error.
    self.AssertOutputEndsInError()

  def testMain(self):
    """Verify that running main method runs expected functons.

    Expected: LoadAndMergeTables, WriteTable.
    """
    self.mox.StubOutWithMock(mps, 'LoadAndMergeTables')
    self.mox.StubOutWithMock(mps, 'WriteTable')
    mps.LoadAndMergeTables(mox.IgnoreArg()).AndReturn('csv_table')
    mps.WriteTable(mox.Regex(r'csv_table'), 'any-out')
    self.mox.ReplayAll()

    mps.main(['--out=any-out', 'any-package'])
    self.mox.VerifyAll()

if __name__ == '__main__':
  cros_test_lib.main()
