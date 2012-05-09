#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_portage_upgrade.py."""

import exceptions
import unittest

import mox

from chromite.lib import cros_test_lib as test_lib
from chromite.lib import gdata_lib
from chromite.lib import osutils
from chromite.lib import table as tablelib
from chromite.scripts import merge_package_status as mps
from chromite.scripts import upload_package_status as ups

# pylint: disable=W0212,R0904,E1120,E1101


class SSEntry(object):
  """Class to simulate one spreadsheet entry."""
  def __init__(self, text):
    self.text = text


class SSRow(object):
  """Class for simulating spreadsheet row."""
  def __init__(self, row, cols=None):
    self.custom = {}

    if not cols:
      # If columns not specified, then column order doesn't matter.
      cols = row.keys()
    for col in cols:
      ss_col = gdata_lib.PrepColNameForSS(col)
      val = row[col]
      ss_val = gdata_lib.PrepValForSS(val)
      self.custom[ss_col] = SSEntry(ss_val)


class SSFeed(object):
  """Class for simulating spreadsheet list feed."""
  def __init__(self, rows, cols=None):
    self.entry = []
    for row in rows:
      self.entry.append(SSRow(row, cols))


class UploaderTest(test_lib.MoxTestCase):
  """Test the functionality of upload_package_status.Uploader class."""

  COL_PKG = 'Package'
  COL_SLOT = 'Slot'
  COL_OVERLAY = 'Overlay'
  COL_STATUS = 'Status'
  COL_VER = 'Current Version'
  COL_STABLE_UP = 'Stable Upstream Version'
  COL_LATEST_UP = 'Latest Upstream Version'
  COL_TARGET = 'Chrome OS Root Target'

  SS_COL_PKG = gdata_lib.PrepColNameForSS(COL_PKG)
  SS_COL_SLOT = gdata_lib.PrepColNameForSS(COL_SLOT)
  SS_COL_OVERLAY = gdata_lib.PrepColNameForSS(COL_OVERLAY)
  SS_COL_STATUS = gdata_lib.PrepColNameForSS(COL_STATUS)
  SS_COL_VER = gdata_lib.PrepColNameForSS(COL_VER)
  SS_COL_STABLE_UP = gdata_lib.PrepColNameForSS(COL_STABLE_UP)
  SS_COL_LATEST_UP = gdata_lib.PrepColNameForSS(COL_LATEST_UP)
  SS_COL_TARGET = gdata_lib.PrepColNameForSS(COL_TARGET)

  COLS = [COL_PKG,
          COL_SLOT,
          COL_OVERLAY,
          COL_STATUS,
          COL_VER,
          COL_STABLE_UP,
          COL_LATEST_UP,
          COL_TARGET,
          ]

  ROW0 = {COL_PKG: 'lib/foo',
          COL_SLOT: '0',
          COL_OVERLAY: 'portage',
          COL_STATUS: 'needs upgrade',
          COL_VER: '3.0.2',
          COL_STABLE_UP: '3.0.9',
          COL_LATEST_UP: '3.0.11',
          COL_TARGET: 'chromeos',
          }
  ROW1 = {COL_PKG: 'sys-dev/bar',
          COL_SLOT: '0',
          COL_OVERLAY: 'chromiumos-overlay',
          COL_STATUS: 'needs upgrade',
          COL_VER: '1.2.3-r1',
          COL_STABLE_UP: '1.2.3-r2',
          COL_LATEST_UP: '1.2.4',
          COL_TARGET: 'chromeos-dev',
          }
  ROW2 = {COL_PKG: 'sys-dev/raster',
          COL_SLOT: '1',
          COL_OVERLAY: 'chromiumos-overlay',
          COL_STATUS: 'current',
          COL_VER: '1.2.3',
          COL_STABLE_UP: '1.2.3',
          COL_LATEST_UP: '1.2.4',
          COL_TARGET: 'chromeos-test',
          }

  SS_ROW0 = dict([(gdata_lib.PrepColNameForSS(c), v) for c, v in ROW0.items()])
  SS_ROW1 = dict([(gdata_lib.PrepColNameForSS(c), v) for c, v in ROW1.items()])
  SS_ROW2 = dict([(gdata_lib.PrepColNameForSS(c), v) for c, v in ROW2.items()])

  EMAIL = 'knights@ni.com'
  PASSWORD = 'the'

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def _MockUploader(self, table=None):
    """Set up a mocked Uploader object."""
    uploader = self.mox.CreateMock(ups.Uploader)

    if not table:
      # Use default table
      table = self._CreateDefaultTable()

    for slot in ups.Uploader.__slots__:
      uploader.__setattr__(slot, None)

    uploader._csv_table = table
    uploader._scomm = self.mox.CreateMock(gdata_lib.SpreadsheetComm)
    uploader._creds = test_lib.EasyAttr(user=self.EMAIL, password=self.PASSWORD)
    uploader._ss_row_cache = self._CreateRowCache(table)

    return uploader

  def _CreateRowCache(self, table):
    """Recreate the expected row cache (by pkg) from |table|."""
    if not table:
      return None

    row_cache = {}
    for rowIx, row in enumerate(table):
      pkg = row[self.COL_PKG]

      # Translate column names now.
      ss_row_dict = {}
      for col in row:
        ss_row_dict[gdata_lib.PrepColNameForSS(col)] = row[col]

      ss_row = gdata_lib.SpreadsheetRow('OrigRow%d' % (rowIx + 2),
                                        rowIx + 2, ss_row_dict)
      entry = row_cache.get(pkg)
      if not entry:
        row_cache[pkg] = ss_row
      elif type(entry) == list:
        row_cache[pkg] = entry + [ss_row]
      else:
        row_cache[pkg] = [entry, ss_row]
    return row_cache

  def _CreateDefaultTable(self):
    return self._CreateTableWithRows(self.COLS,
                                     [self.ROW0, self.ROW1])

  def _CreateTableWithRows(self, cols, rows):
    mytable = tablelib.Table(list(cols))
    if rows:
      for row in rows:
        mytable.AppendRow(dict(row))
    return mytable

  def testLoadTable(self):
    # Note that this test is not actually for method of Uploader class.

    self.mox.StubOutWithMock(tablelib.Table, 'LoadFromCSV')
    csv = 'any.csv'

    # Replay script
    tablelib.Table.LoadFromCSV(csv).AndReturn('loaded_table')
    self.mox.ReplayAll()

    # Verification steps.
    with self.OutputCapturer():
      loaded_table = ups.LoadTable(csv)
      self.assertEquals(loaded_table, 'loaded_table')

  def testGetSSRowForPackage(self):
    mocked_uploader = self._MockUploader()

    # No replay script.
    self.mox.ReplayAll()

    # Verification steps.
    result = ups.Uploader._GetSSRowForPackage(mocked_uploader,
                                              self.ROW0[self.COL_PKG])
    self.assertEquals(result, self.SS_ROW0)
    self.assertEquals(2, result.ss_row_num)
    result = ups.Uploader._GetSSRowForPackage(mocked_uploader,
                                              self.ROW1[self.COL_PKG])
    self.assertEquals(result, self.SS_ROW1)
    self.assertEquals(3, result.ss_row_num)
    result = ups.Uploader._GetSSRowForPackage(mocked_uploader,
                                              self.ROW2[self.COL_PKG])
    self.assertEquals(result, None)

    self.mox.VerifyAll()

  def testUploadFirstWorksheet(self):
    mocked_uploader = self._MockUploader()

    # Clear ._scomm attribute to simulate uploading first worksheet.
    mocked_scomm = mocked_uploader._scomm
    mocked_uploader._scomm = None

    self.mox.StubOutWithMock(gdata_lib.SpreadsheetComm, '__new__')

    ss_key = 'Some ss_key'
    ws_name = 'Some ws_name'

    # Replay script
    gdata_lib.SpreadsheetComm.__new__(gdata_lib.SpreadsheetComm
                                      ).AndReturn(mocked_scomm)
    mocked_scomm.Connect(mocked_uploader._creds, ss_key, ws_name,
                         source='Upload Package Status')
    mocked_scomm.GetRowCacheByCol(self.SS_COL_PKG).AndReturn('RowCache')
    mocked_uploader._UploadChangedRows().AndReturn(tuple([0, 1, 2]))
    mocked_uploader._DeleteOldRows().AndReturn(tuple([3, 4]))
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      ups.Uploader.Upload(mocked_uploader, ss_key, ws_name)
      self.mox.VerifyAll()

  def testUploadSecondWorksheet(self):
    mocked_uploader = self._MockUploader()

    ss_key = 'Some ss_key'
    ws_name = 'Some ws_name'

    # Replay script
    mocked_uploader._scomm.SetCurrentWorksheet(ws_name)
    mocked_uploader._scomm.GetRowCacheByCol(self.SS_COL_PKG).AndReturn('RCache')
    mocked_uploader._UploadChangedRows().AndReturn(tuple([0, 1, 2]))
    mocked_uploader._DeleteOldRows().AndReturn(tuple([3, 4]))
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      ups.Uploader.Upload(mocked_uploader, ss_key, ws_name)
      self.mox.VerifyAll()

  def testUploadChangedRows(self):
    table = self._CreateTableWithRows(self.COLS,
                                      [self.ROW0, self.ROW1, self.ROW2])
    mocked_uploader = self._MockUploader(table=table)

    def RowVerifier(row_delta, golden_col_set, golden_row):
      if golden_col_set != set(row_delta.keys()):
        return False

      for col in row_delta:
        val = row_delta[col]
        if val != golden_row[col]:
          return False

      return True

    # First Row.
    # Pretend first row does not exist already in online spreadsheet
    # by returning (None, None) from _GetSSRowForPackage.
    #
    row0_pkg = self.ROW0[self.COL_PKG]
    mocked_uploader._GetSSRowForPackage(row0_pkg).AndReturn(None)
    mocked_uploader._scomm.InsertRow(mox.IgnoreArg())

    # Second Row.
    # Pretend second row does already exist in online spreadsheet, and
    # pretend that it has a different value that needs to be changed
    # by an upload.
    row1_pkg = self.ROW1[self.COL_PKG]
    row1_reverse_delta = { self.SS_COL_VER: '1.2.3' }
    ss_row1 = dict(self.SS_ROW1)
    for col in row1_reverse_delta:
      ss_row1[col] = row1_reverse_delta[col]
    ss_row1 = gdata_lib.SpreadsheetRow('OrigRow1', 3, ss_row1)
    mocked_uploader._GetSSRowForPackage(row1_pkg).AndReturn(ss_row1)
    # Prepare verfication for row.
    g_col_set1 = set(row1_reverse_delta.keys())
    g_row1 = gdata_lib.PrepRowForSS(self.SS_ROW1)
    row1_verifier = lambda rdelta : RowVerifier(rdelta, g_col_set1, g_row1)
    mocked_uploader._scomm.UpdateRowCellByCell(3, mox.Func(row1_verifier))

    # Third Row.
    # Pretend third row does already exist in online spreadsheet, and
    # pretend that several values need to be changed by an upload.
    row2_pkg = self.ROW2[self.COL_PKG]
    row2_reverse_delta = { self.SS_COL_STATUS: 'needs upgrade',
                           self.SS_COL_VER: '0.5',
                           self.SS_COL_TARGET: 'chromeos-foo',
                           }
    ss_row2 = dict(self.SS_ROW2)
    for col in row2_reverse_delta:
      ss_row2[col] = row2_reverse_delta[col]
    ss_row2 = gdata_lib.SpreadsheetRow('OrigRow2', 4, ss_row2)
    mocked_uploader._GetSSRowForPackage(row2_pkg).AndReturn(ss_row2)
    # Prepare verification for row.
    g_col_set2 = set(row2_reverse_delta.keys())
    g_row2 = gdata_lib.PrepRowForSS(self.SS_ROW2)
    row2_verifier = lambda rdelta : RowVerifier(rdelta, g_col_set2, g_row2)
    mocked_uploader._scomm.UpdateRowCellByCell(4, mox.Func(row2_verifier))

    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      ups.Uploader._UploadChangedRows(mocked_uploader)
    self.mox.VerifyAll()

  def testDeleteOldRows(self):
    mocked_uploader = self._MockUploader()

    # Pretend spreadsheet has 2 rows, one in table and one not.
    ss_row1 = gdata_lib.SpreadsheetRow('OrigRow1', 2, self.SS_ROW1)
    ss_row2 = gdata_lib.SpreadsheetRow('OrigRow2', 3, self.SS_ROW2)
    ss_rows = (ss_row1, ss_row2)

    mocked_uploader._scomm.GetRows().AndReturn(ss_rows)
    # We expect ROW2 in spreadsheet to be deleted.
    mocked_uploader._scomm.DeleteRow('OrigRow2')
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      ups.Uploader._DeleteOldRows(mocked_uploader)
    self.mox.VerifyAll()


class MainTest(test_lib.MoxTestCase):
  """Test argument handling at the main method level."""

  def setUp(self):
    """Setup for all tests in this class."""
    mox.MoxTestBase.setUp(self)

  def testHelp(self):
    """Test that --help is functioning"""
    with self.OutputCapturer() as output:
      # Running with --help should exit with code==0
      try:
        ups.main(['--help'])
      except exceptions.SystemExit, e:
        self.assertEquals(e.args[0], 0)

    # Verify that a message beginning with "Usage: " was printed
    stdout = output.GetStdout()
    self.assertTrue(stdout.startswith('Usage: '))

  def testMissingCSV(self):
    """Test that running without a csv file argument exits with an error."""
    with self.OutputCapturer():
      # Running without a package should exit with code!=0
      try:
        ups.main([])
      except exceptions.SystemExit, e:
        self.assertNotEquals(e.args[0], 0)

    self.AssertOutputEndsInError(check_stdout=True)

  def testPrepareCredsEmailPassword(self):
    email = 'foo@g.com'
    password = 'shh'
    creds_file = 'bogus'
    token_file = 'boguser'

    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    self.mox.StubOutWithMock(gdata_lib.Creds, '__new__')

    gdata_lib.Creds.__new__(gdata_lib.Creds).AndReturn(mocked_creds)
    mocked_creds.SetCreds(email, password)
    self.mox.ReplayAll()

    ups.PrepareCreds(creds_file, token_file, email, password)
    self.mox.VerifyAll()

  def testMainEmailPassword(self):
    """Verify that running main with email/password follows flow."""
    csv = 'any.csv'
    email = 'foo@g.com'
    password = '123'

    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    creds_file = 'non-existing-file'

    self.mox.StubOutWithMock(ups, 'PrepareCreds')
    self.mox.StubOutWithMock(ups, 'LoadTable')
    self.mox.StubOutWithMock(mps, 'FinalizeTable')
    self.mox.StubOutWithMock(ups.Uploader, 'Upload')

    ups.PrepareCreds(creds_file, None, email, password).AndReturn(mocked_creds)
    ups.LoadTable(csv).AndReturn('csv_table')
    mps.FinalizeTable('csv_table')
    ups.Uploader.Upload(mox.IgnoreArg(), ws_name='Packages')
    ups.Uploader.Upload(mox.IgnoreArg(), ws_name='Dependencies')
    mocked_creds.StoreCredsIfNeeded(creds_file)
    self.mox.ReplayAll()

    ups.main(['--email=%s' % email,
              '--password=%s' % password,
              '--cred-file=%s' % creds_file,
               csv])

    self.mox.VerifyAll()

  @osutils.TempFileDecorator
  def testMainCredsFile(self):
    """Verify that running main with creds file follows flow."""
    csv = 'any.csv'
    creds_file = self.tempfile
    token_file = 'non-existing-file'

    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    mocked_creds.auth_token_loaded = False

    self.mox.StubOutWithMock(ups, 'PrepareCreds')
    self.mox.StubOutWithMock(ups, 'LoadTable')
    self.mox.StubOutWithMock(mps, 'FinalizeTable')
    self.mox.StubOutWithMock(ups.Uploader, 'Upload')

    ups.PrepareCreds(creds_file, token_file, None, None).AndReturn(mocked_creds)
    ups.LoadTable(csv).AndReturn('csv_table')
    mps.FinalizeTable('csv_table')
    ups.Uploader.Upload(mox.IgnoreArg(), ws_name=ups.PKGS_WS_NAME)
    ups.Uploader.Upload(mox.IgnoreArg(), ws_name=ups.DEPS_WS_NAME)
    mocked_creds.StoreCredsIfNeeded(creds_file)
    mocked_creds.StoreAuthTokenIfNeeded(token_file)
    self.mox.ReplayAll()

    ups.main(['--cred-file=%s' % creds_file,
              '--auth-token-file=%s' % token_file,
              csv])

    self.mox.VerifyAll()


  @osutils.TempFileDecorator
  def testMainTokenFile(self):
    """Verify that running main with token file follows flow."""
    csv = 'any.csv'
    token_file = self.tempfile
    creds_file = 'non-existing-file'

    mocked_creds = self.mox.CreateMock(gdata_lib.Creds)
    mocked_creds.auth_token_loaded = True

    self.mox.StubOutWithMock(ups, 'PrepareCreds')
    self.mox.StubOutWithMock(ups, 'LoadTable')
    self.mox.StubOutWithMock(mps, 'FinalizeTable')
    self.mox.StubOutWithMock(ups.Uploader, 'Upload')

    ups.PrepareCreds(creds_file, token_file, None, None).AndReturn(mocked_creds)
    ups.LoadTable(csv).AndReturn('csv_table')
    mps.FinalizeTable('csv_table')
    ups.Uploader.Upload(mox.IgnoreArg(), ws_name=ups.PKGS_WS_NAME)
    ups.Uploader.Upload(mox.IgnoreArg(), ws_name=ups.DEPS_WS_NAME)
    mocked_creds.StoreCredsIfNeeded(creds_file)
    mocked_creds.StoreAuthTokenIfNeeded(token_file)
    self.mox.ReplayAll()

    ups.main(['--cred-file=%s' % creds_file,
              '--auth-token-file=%s' % token_file,
              csv])

    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
