#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_portage_upgrade.py."""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))
from chromite.lib import cros_test_lib as test_lib
from chromite.lib import gdata_lib
from chromite.lib import upgrade_table as utable
from chromite.scripts import sync_package_status as sps

# pylint: disable=W0212,R0904


class SyncerTest(test_lib.MoxTestCase):

  col_amd64 = utable.UpgradeTable.GetColumnName(utable.UpgradeTable.COL_STATE,
                                                'amd64')
  col_amd64 = gdata_lib.PrepColNameForSS(col_amd64)
  col_arm = utable.UpgradeTable.GetColumnName(utable.UpgradeTable.COL_STATE,
                                              'arm')
  col_arm = gdata_lib.PrepColNameForSS(col_arm)
  col_x86 = utable.UpgradeTable.GetColumnName(utable.UpgradeTable.COL_STATE,
                                              'x86')
  col_x86 = gdata_lib.PrepColNameForSS(col_x86)

  def testInit(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    tcomm, scomm = 'TComm', 'SComm'

    # Replay script
    self.mox.ReplayAll()

    # Verify
    sps.Syncer.__init__(mocked_syncer, tcomm, scomm)
    self.mox.VerifyAll()
    self.assertEquals(scomm, mocked_syncer.scomm)
    self.assertEquals(tcomm, mocked_syncer.tcomm)
    self.assertEquals(None, mocked_syncer.teams)
    self.assertEquals(None, mocked_syncer.owners)
    self.assertEquals(False, mocked_syncer.pretend)
    self.assertEquals(False, mocked_syncer.verbose)

  def testReduceTeamName(self):
    syncer = sps.Syncer('tcomm_obj', 'scomm_obj')

    tests = {
      'build/bdavirro': 'build',
      'build/rtc': 'build',
      'build': 'build',
      'UI/zel': 'ui',
      'UI': 'ui',
      'Build': 'build',
      None: None,
      }

    # Verify
    for key in tests:
      result = syncer._ReduceTeamName(key)
      self.assertEquals(tests[key], result)

  def testReduceOwnerName(self):
    syncer = sps.Syncer('tcomm_obj', 'scomm_obj')

    tests = {
      'joe': 'joe',
      'Joe': 'joe',
      'joe@chromium.org': 'joe',
      'Joe@chromium.org': 'joe',
      'Joe.Bob@chromium.org': 'joe.bob',
      None: None,
      }

    # Verify
    for key in tests:
      result = syncer._ReduceOwnerName(key)
      self.assertEquals(tests[key], result)

  def testSetTeamFilterOK(self):
    syncer = sps.Syncer('tcomm_obj', 'scomm_obj')

    tests = {
      'build:system:ui': set(['build', 'system', 'ui']),
      'Build:system:UI': set(['build', 'system', 'ui']),
      'kernel': set(['kernel']),
      'KERNEL': set(['kernel']),
      None: None,
      '': None,
      }

    # Verify
    for test in tests:
      syncer.SetTeamFilter(test)
      self.assertEquals(tests[test], syncer.teams)

  def testSetTeamFilterError(self):
    syncer = sps.Syncer('tcomm_obj', 'scomm_obj')

    # "systems" is not valid (should be "system")
    teamarg = 'build:systems'

    # Verify
    with self.OutputCapturer():
      self.assertRaises(SystemExit, sps.Syncer.SetTeamFilter,
                        syncer, teamarg)

  def testSetOwnerFilter(self):
    syncer = sps.Syncer('tcomm_obj', 'scomm_obj')

    tests = {
      'joe:bill:bob': set(['joe', 'bill', 'bob']),
      'Joe:Bill:BOB': set(['joe', 'bill', 'bob']),
      'joe@chromium.org:bill:bob': set(['joe', 'bill', 'bob']),
      'joe': set(['joe']),
      'joe@chromium.org': set(['joe']),
      '': None,
      None: None,
      }

    # Verify
    for test in tests:
      syncer.SetOwnerFilter(test)
      self.assertEquals(tests[test], syncer.owners)

  def testRowPassesFilters(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    row1 = { sps.COL_TEAM: 'build', sps.COL_OWNER: 'joe' }
    row2 = { sps.COL_TEAM: 'build', sps.COL_OWNER: 'bob' }
    row3 = { sps.COL_TEAM: 'build', sps.COL_OWNER: None }
    row4 = { sps.COL_TEAM: None, sps.COL_OWNER: None }

    teams1 = set(['build'])
    teams2 = set(['kernel'])
    teams3 = set(['build', 'ui'])

    owners1 = set(['joe'])
    owners2 = set(['bob'])
    owners3 = set(['joe', 'dan'])

    tests = [
      { 'row': row1, 'teams': None, 'owners': None, 'result': True },
      { 'row': row1, 'teams': teams1, 'owners': None, 'result': True },
      { 'row': row1, 'teams': teams2, 'owners': None, 'result': False },
      { 'row': row1, 'teams': teams3, 'owners': None, 'result': True },
      { 'row': row1, 'teams': teams1, 'owners': owners1, 'result': True },
      { 'row': row1, 'teams': None, 'owners': owners2, 'result': False },
      { 'row': row1, 'teams': None, 'owners': owners3, 'result': True },

      { 'row': row2, 'teams': None, 'owners': None, 'result': True },
      { 'row': row2, 'teams': teams1, 'owners': None, 'result': True },
      { 'row': row2, 'teams': teams2, 'owners': None, 'result': False },
      { 'row': row2, 'teams': teams3, 'owners': None, 'result': True },
      { 'row': row2, 'teams': teams1, 'owners': owners1, 'result': False },
      { 'row': row2, 'teams': None, 'owners': owners2, 'result': True },
      { 'row': row2, 'teams': None, 'owners': owners3, 'result': False },

      { 'row': row3, 'teams': None, 'owners': None, 'result': True },
      { 'row': row3, 'teams': teams1, 'owners': None, 'result': True },
      { 'row': row3, 'teams': teams2, 'owners': None, 'result': False },
      { 'row': row3, 'teams': teams3, 'owners': None, 'result': True },
      { 'row': row3, 'teams': teams1, 'owners': owners1, 'result': False },
      { 'row': row3, 'teams': None, 'owners': owners2, 'result': False },
      { 'row': row3, 'teams': None, 'owners': owners3, 'result': False },

      { 'row': row4, 'teams': None, 'owners': None, 'result': True },
      { 'row': row4, 'teams': teams1, 'owners': None, 'result': False },
      { 'row': row4, 'teams': teams1, 'owners': owners1, 'result': False },
      { 'row': row4, 'teams': None, 'owners': owners2, 'result': False },
      ]

    # Replay script
    for test in tests:
      done = False

      if test['teams']:
        row_team = test['row'][sps.COL_TEAM]
        mocked_syncer._ReduceTeamName(row_team).AndReturn(row_team)
        done = row_team not in test['teams']

      if not done and test['owners']:
        row_owner = test['row'][sps.COL_OWNER]
        mocked_syncer._ReduceOwnerName(row_owner).AndReturn(row_owner)
    self.mox.ReplayAll()

    # Verify
    for test in tests:
      mocked_syncer.teams = test['teams']
      mocked_syncer.owners = test['owners']
      result = sps.Syncer._RowPassesFilters(mocked_syncer, test['row'])

      msg = ('Expected following row to %s filter, but it did not:\n%r' %
             ('pass' if test['result'] else 'fail', test['row']))
      msg += '\n  Using teams filter : %r' % mocked_syncer.teams
      msg += '\n  Using owners filter: %r' % mocked_syncer.owners
      self.assertEquals(test['result'], result, msg)
    self.mox.VerifyAll()

  def testSyncNewIssues(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_scomm = self.mox.CreateMock(gdata_lib.SpreadsheetComm)
    mocked_tcomm = self.mox.CreateMock(gdata_lib.TrackerComm)
    mocked_syncer.scomm = mocked_scomm
    mocked_syncer.tcomm = mocked_tcomm

    rows = [
      { sps.COL_PACKAGE: 'd/f', sps.COL_TEAM: 'build', sps.COL_OWNER: None },
      { sps.COL_PACKAGE: 'd/f', sps.COL_TEAM: 'build', sps.COL_OWNER: 'joe' },
      ]

    # Replay script
    mocked_scomm.GetColumnIndex('Tracker')
    mocked_scomm.GetRows().AndReturn(rows)

    for ix in xrange(len(rows)):
      mocked_syncer._RowPassesFilters(rows[ix]).AndReturn(True)
      mocked_syncer._GenIssueForRow(rows[ix]).AndReturn('NewIssue%d' % ix)
      mocked_syncer._GetRowTrackerId(rows[ix]).AndReturn(None)
      mocked_syncer._CreateRowIssue(ix + 2, rows[ix], 'NewIssue%d' % ix)
    self.mox.ReplayAll()

    # Verify
    sps.Syncer.Sync(mocked_syncer)
    self.mox.VerifyAll()

  def testSyncClearIssues(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_scomm = self.mox.CreateMock(gdata_lib.SpreadsheetComm)
    mocked_tcomm = self.mox.CreateMock(gdata_lib.TrackerComm)
    mocked_syncer.scomm = mocked_scomm
    mocked_syncer.tcomm = mocked_tcomm

    rows = [
      { sps.COL_PACKAGE: 'd/f', sps.COL_TEAM: 'build', sps.COL_OWNER: None },
      { sps.COL_PACKAGE: 'd/f', sps.COL_TEAM: 'build', sps.COL_OWNER: 'joe' },
      ]

    # Replay script
    mocked_scomm.GetColumnIndex('Tracker')
    mocked_scomm.GetRows().AndReturn(rows)

    for ix in xrange(len(rows)):
      mocked_syncer._RowPassesFilters(rows[ix]).AndReturn(True)
      mocked_syncer._GenIssueForRow(rows[ix]).AndReturn(None)
      mocked_syncer._GetRowTrackerId(rows[ix]).AndReturn(123 + ix)
      mocked_syncer._ClearRowIssue(ix + 2, rows[ix])
    self.mox.ReplayAll()

    # Verify
    sps.Syncer.Sync(mocked_syncer)
    self.mox.VerifyAll()

  def testSyncFilteredOut(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_scomm = self.mox.CreateMock(gdata_lib.SpreadsheetComm)
    mocked_tcomm = self.mox.CreateMock(gdata_lib.TrackerComm)
    mocked_syncer.scomm = mocked_scomm
    mocked_syncer.tcomm = mocked_tcomm

    rows = [
      { sps.COL_PACKAGE: 'd/f', sps.COL_TEAM: 'build', sps.COL_OWNER: None },
      { sps.COL_PACKAGE: 'd/f', sps.COL_TEAM: 'build', sps.COL_OWNER: 'joe' },
      ]

    # Replay script
    mocked_scomm.GetColumnIndex('Tracker')
    mocked_scomm.GetRows().AndReturn(rows)

    for ix in xrange(len(rows)):
      mocked_syncer._RowPassesFilters(rows[ix]).AndReturn(False)
    self.mox.ReplayAll()

    # Verify
    sps.Syncer.Sync(mocked_syncer)
    self.mox.VerifyAll()

  def testGetRowValue(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    row = {
      self.col_amd64: 'ABC',
      self.col_arm: 'XYZ',
      self.col_x86: 'FooBar',
      sps.COL_TEAM: 'build',
      }

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = sps.Syncer._GetRowValue(mocked_syncer, row,
                                     'stateonamd64', 'amd64')
    self.assertEquals('ABC', result)
    result = sps.Syncer._GetRowValue(mocked_syncer, row,
                                     'stateonarm', 'arm')
    self.assertEquals('XYZ', result)
    result = sps.Syncer._GetRowValue(mocked_syncer, row,
                                     'stateonamd64', 'amd64')
    self.assertEquals('ABC', result)
    result = sps.Syncer._GetRowValue(mocked_syncer, row, sps.COL_TEAM)
    self.assertEquals('build', result)
    self.mox.VerifyAll()

  def _TestGenIssueForRowNeedsUpgrade(self, row):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_syncer.default_owner = None
    mocked_syncer.scomm = test_lib.EasyAttr(ss_key='SomeSSKey')

    # Replay script
    for arch in sps.ARCHES:
      state = sps.Syncer._GetRowValue(mocked_syncer, row,
                                      utable.UpgradeTable.COL_STATE, arch)
      mocked_syncer._GetRowValue(row, utable.UpgradeTable.COL_STATE,
                                 arch).AndReturn(state)
    red_team = sps.Syncer._ReduceTeamName(mocked_syncer, row[sps.COL_TEAM])
    mocked_syncer._ReduceTeamName(row[sps.COL_TEAM]).AndReturn(red_team)
    red_owner = sps.Syncer._ReduceOwnerName(mocked_syncer, row[sps.COL_OWNER])
    mocked_syncer._ReduceOwnerName(row[sps.COL_OWNER]).AndReturn(red_owner)
    for arch in sps.ARCHES:
      mocked_syncer._GetRowValue(row, utable.UpgradeTable.COL_CURRENT_VER,
                                 arch).AndReturn('1')
      mocked_syncer._GetRowValue(row,
                                 utable.UpgradeTable.COL_STABLE_UPSTREAM_VER,
                                 arch).AndReturn('2')
      mocked_syncer._GetRowValue(row,
                                 utable.UpgradeTable.COL_LATEST_UPSTREAM_VER,
                                 arch).AndReturn('3')
    self.mox.ReplayAll()

    # Verify
    result = sps.Syncer._GenIssueForRow(mocked_syncer, row)
    self.mox.VerifyAll()
    return result

  def testGenIssueForRowNeedsUpgrade1(self):
    row = {
      self.col_amd64: utable.UpgradeTable.STATE_NEEDS_UPGRADE,
      self.col_arm: 'Not important',
      self.col_x86: 'Not important',
      sps.COL_TEAM: 'build',
      sps.COL_OWNER: None,
      sps.COL_PACKAGE: 'dev/foo',
      }

    result = self._TestGenIssueForRowNeedsUpgrade(row)
    self.assertEquals(None, result.owner)
    self.assertEquals(0, result.id)
    self.assertEquals('Untriaged', result.status)

  def testGenIssueForRowNeedsUpgrade2(self):
    row = {
      self.col_amd64: utable.UpgradeTable.STATE_NEEDS_UPGRADE,
      self.col_arm: utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_PATCHED,
      self.col_x86: 'Not important',
      sps.COL_TEAM: 'build',
      sps.COL_OWNER: 'joe',
      sps.COL_PACKAGE: 'dev/foo',
      }

    result = self._TestGenIssueForRowNeedsUpgrade(row)
    self.assertEquals('joe@chromium.org', result.owner)
    self.assertEquals(0, result.id)
    self.assertEquals('Available', result.status)

  def testGenIssueForRowNeedsUpgrade3(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    row = {
      self.col_amd64: utable.UpgradeTable.STATE_NEEDS_UPGRADE,
      self.col_arm: utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_PATCHED,
      self.col_x86: 'Not important',
      sps.COL_TEAM: None,
      sps.COL_OWNER: 'joe',
      sps.COL_PACKAGE: 'dev/foo',
      }

    # Replay script
    for arch in sps.ARCHES:
      state = sps.Syncer._GetRowValue(mocked_syncer, row,
                                      utable.UpgradeTable.COL_STATE, arch)
      mocked_syncer._GetRowValue(row, utable.UpgradeTable.COL_STATE,
                                 arch).AndReturn(state)
    reduced = sps.Syncer._ReduceTeamName(mocked_syncer, row[sps.COL_TEAM])
    mocked_syncer._ReduceTeamName(row[sps.COL_TEAM]).AndReturn(reduced)
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      self.assertRaises(RuntimeError, sps.Syncer._GenIssueForRow,
                        mocked_syncer, row)
    self.mox.VerifyAll()

  def testGenIssueForRowNoUpgrade(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    row = {
      self.col_amd64: 'Not important',
      self.col_arm: 'Not important',
      self.col_x86: 'Not important',
      sps.COL_TEAM: None,
      sps.COL_OWNER: 'joe',
      sps.COL_PACKAGE: 'dev/foo',
      }

    # Replay script
    for arch in sps.ARCHES:
      state = sps.Syncer._GetRowValue(mocked_syncer, row,
                                      utable.UpgradeTable.COL_STATE, arch)
      mocked_syncer._GetRowValue(row, utable.UpgradeTable.COL_STATE,
                                 arch).AndReturn(state)
    self.mox.ReplayAll()

    # Verify
    result = sps.Syncer._GenIssueForRow(mocked_syncer, row)
    self.mox.VerifyAll()
    self.assertEquals(None, result)

  def testGetRowTrackerId(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    row = { sps.COL_TRACKER: '321' }

    # Replay script
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      result = sps.Syncer._GetRowTrackerId(mocked_syncer, row)
    self.mox.VerifyAll()
    self.assertEquals(321, result)

  def testCreateRowIssuePretend(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_syncer.pretend = True

    row = { sps.COL_PACKAGE: 'dev/foo' }

    # Replay script
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      sps.Syncer._CreateRowIssue(mocked_syncer, 5, row, 'some_issue')
    self.mox.VerifyAll()

  def testCreateRowIssue(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_scomm = self.mox.CreateMock(gdata_lib.SpreadsheetComm)
    mocked_tcomm = self.mox.CreateMock(gdata_lib.TrackerComm)
    mocked_syncer.scomm = mocked_scomm
    mocked_syncer.tcomm = mocked_tcomm
    mocked_syncer.tracker_col_ix = 8
    mocked_syncer.pretend = False

    row_ix = 5
    row = { sps.COL_PACKAGE: 'dev/foo' }
    issue = 'SomeIssue'
    issue_id = 234
    ss_issue_val = 'Hyperlink%d' % issue_id

    # Replay script
    mocked_tcomm.CreateTrackerIssue(issue).AndReturn(issue_id)
    mocked_syncer._GenSSLinkToIssue(issue_id).AndReturn(ss_issue_val)
    mocked_scomm.ReplaceCellValue(row_ix, mocked_syncer.tracker_col_ix,
                                  ss_issue_val)
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      sps.Syncer._CreateRowIssue(mocked_syncer, row_ix, row, issue)
    self.mox.VerifyAll()

  def testGenSSLinkToIssue(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)

    issue_id = 123

    # Replay script
    self.mox.ReplayAll()

    # Verify
    result = sps.Syncer._GenSSLinkToIssue(mocked_syncer, issue_id)
    self.mox.VerifyAll()
    self.assertEquals('=hyperlink("crosbug.com/123";"123")', result)

  def testClearRowIssue(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_scomm = self.mox.CreateMock(gdata_lib.SpreadsheetComm)
    mocked_syncer.scomm = mocked_scomm
    mocked_syncer.tracker_col_ix = 8
    mocked_syncer.pretend = False

    row_ix = 44
    row = { sps.COL_PACKAGE: 'dev/foo' }

    # Replay script
    mocked_scomm.ClearCellValue(row_ix, mocked_syncer.tracker_col_ix)
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      sps.Syncer._ClearRowIssue(mocked_syncer, row_ix, row)
    self.mox.VerifyAll()

  def testClearRowIssuePretend(self):
    mocked_syncer = self.mox.CreateMock(sps.Syncer)
    mocked_scomm = self.mox.CreateMock(gdata_lib.SpreadsheetComm)
    mocked_syncer.scomm = mocked_scomm
    mocked_syncer.tracker_col_ix = 8
    mocked_syncer.pretend = True

    row_ix = 44
    row = { sps.COL_PACKAGE: 'dev/foo' }

    # Replay script
    self.mox.ReplayAll()

    # Verify
    with self.OutputCapturer():
      sps.Syncer._ClearRowIssue(mocked_syncer, row_ix, row)
    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
