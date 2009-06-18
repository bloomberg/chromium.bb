#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for watchlists.py."""

import unittest
import super_mox
import watchlists


class WatchlistsTest(super_mox.SuperMoxTestBase):

  def setUp(self):
    super_mox.SuperMoxTestBase.setUp(self)
    self.mox.StubOutWithMock(watchlists.Watchlists, '_HasWatchlistsFile')
    self.mox.StubOutWithMock(watchlists.Watchlists, '_ContentsOfWatchlistsFile')
    self.mox.StubOutWithMock(watchlists.logging, 'error')

  def testMissingWatchlistsFileOK(self):
    """Test that we act gracefully if WATCHLISTS file is missing."""
    watchlists.Watchlists._HasWatchlistsFile().AndReturn(False)
    self.mox.ReplayAll()

    wl = watchlists.Watchlists('/some/random/path')
    self.assertEqual(wl.GetWatchersForPaths(['some_path']), [])

  def testGarbledWatchlistsFileOK(self):
    """Test that we act gracefully if WATCHLISTS file is garbled."""
    contents = 'some garbled and unwanted text'
    watchlists.Watchlists._HasWatchlistsFile().AndReturn(True)
    watchlists.Watchlists._ContentsOfWatchlistsFile().AndReturn(contents)
    watchlists.logging.error(super_mox.mox.IgnoreArg())
    self.mox.ReplayAll()

    wl = watchlists.Watchlists('/a/path')
    self.assertEqual(wl.GetWatchersForPaths(['some_path']), [])

  def testNoWatchers(self):
    contents = \
      """{
        'WATCHLIST_DEFINITIONS': {
          'a_module': {
            'filepath': 'a_module',
          },
        },

        'WATCHLISTS': {
          'a_module': [],
        },
      } """
    watchlists.Watchlists._HasWatchlistsFile().AndReturn(True)
    watchlists.Watchlists._ContentsOfWatchlistsFile().AndReturn(contents)
    self.mox.ReplayAll()

    wl = watchlists.Watchlists('/a/path')
    self.assertEqual(wl.GetWatchersForPaths(['a_module']), [])

  def testValidWatcher(self):
    watchers = ['abc@def.com', 'x1@xyz.org']
    contents = \
      """{
        'WATCHLIST_DEFINITIONS': {
          'a_module': {
            'filepath': 'a_module',
          },
        },
        'WATCHLISTS': {
          'a_module': %s,
        },
      } """ % watchers
    watchlists.Watchlists._HasWatchlistsFile().AndReturn(True)
    watchlists.Watchlists._ContentsOfWatchlistsFile().AndReturn(contents)
    self.mox.ReplayAll()

    wl = watchlists.Watchlists('/a/path')
    self.assertEqual(wl.GetWatchersForPaths(['a_module']), watchers)

  def testMultipleWatchlistsTrigger(self):
    """Test that multiple watchlists can get triggered for one filepath."""
    contents = \
      """{
        'WATCHLIST_DEFINITIONS': {
          'mac': {
            'filepath': 'mac',
          },
          'views': {
            'filepath': 'views',
          },
        },
        'WATCHLISTS': {
          'mac': ['x1@chromium.org'],
          'views': ['x2@chromium.org'],
        },
      } """
    watchlists.Watchlists._HasWatchlistsFile().AndReturn(True)
    watchlists.Watchlists._ContentsOfWatchlistsFile().AndReturn(contents)
    self.mox.ReplayAll()

    wl = watchlists.Watchlists('/a/path')
    self.assertEqual(wl.GetWatchersForPaths(['file_views_mac']),
        ['x1@chromium.org', 'x2@chromium.org'])

  def testDuplicateWatchers(self):
    """Test that multiple watchlists can get triggered for one filepath."""
    watchers = ['someone@chromium.org']
    contents = \
      """{
        'WATCHLIST_DEFINITIONS': {
          'mac': {
            'filepath': 'mac',
          },
          'views': {
            'filepath': 'views',
          },
        },
        'WATCHLISTS': {
          'mac': %s,
          'views': %s,
        },
      } """ % (watchers, watchers)
    watchlists.Watchlists._HasWatchlistsFile().AndReturn(True)
    watchlists.Watchlists._ContentsOfWatchlistsFile().AndReturn(contents)
    self.mox.ReplayAll()

    wl = watchlists.Watchlists('/a/path')
    self.assertEqual(wl.GetWatchersForPaths(['file_views_mac']), watchers)


if __name__ == '__main__':
  unittest.main()
