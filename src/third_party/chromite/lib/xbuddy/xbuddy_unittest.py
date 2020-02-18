# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for xbuddy.py."""

from __future__ import print_function

import os
import shutil
import tempfile
import time

import mock
from six.moves import configparser

from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib.xbuddy import xbuddy


# pylint: disable=protected-access
# pylint: disable=no-value-for-parameter

GS_ALTERNATE_DIR = 'gs://chromeos-alternate-archive/'

class xBuddyTest(cros_test_lib.TestCase):
  """Regression tests for xbuddy."""
  def setUp(self):

    self.static_image_dir = tempfile.mkdtemp('xbuddy_unittest_static')

    self.mock_xb = xbuddy.XBuddy(
        True,
        static_dir=self.static_image_dir
    )
    self.images_dir = tempfile.mkdtemp('xbuddy_unittest_images')
    self.mock_xb.images_dir = self.images_dir

  def tearDown(self):
    """Removes testing files."""
    shutil.rmtree(self.static_image_dir)
    shutil.rmtree(self.images_dir)

  def testParseBoolean(self):
    """Check that some common True/False strings are handled."""
    self.assertEqual(xbuddy.XBuddy.ParseBoolean(None), False)
    self.assertEqual(xbuddy.XBuddy.ParseBoolean('false'), False)
    self.assertEqual(xbuddy.XBuddy.ParseBoolean('bs'), False)
    self.assertEqual(xbuddy.XBuddy.ParseBoolean('true'), True)
    self.assertEqual(xbuddy.XBuddy.ParseBoolean('y'), True)

  def testGetLatestVersionFromGsDir(self):
    """Test that we can get the most recent version from gsutil calls."""
    mock_data1 = """gs://chromeos-releases/stable-channel/parrot/3701.96.0/
    gs://chromeos-releases/stable-channel/parrot/3701.98.0/
    gs://chromeos-releases/stable-channel/parrot/3912.100.0/
    gs://chromeos-releases/stable-channel/parrot/3912.101.0/
    gs://chromeos-releases/stable-channel/parrot/3912.79.0/
    gs://chromeos-releases/stable-channel/parrot/3912.79.1/"""

    mock_data2 = """gs://chromeos-image-archive/parrot-release/R26-3912.101.0
    gs://chromeos-image-archive/parrot-release/R27-3912.101.0
    gs://chromeos-image-archive/parrot-release/R28-3912.101.0"""

    with mock.patch.object(
        self.mock_xb, '_LS',
        side_effect=[mock_data1.splitlines(),
                     mock_data2.splitlines()]) as ls_mock:
      self.assertEqual(
          self.mock_xb._GetLatestVersionFromGsDir('url1', with_release=False),
          '3912.101.0')
      ls_mock.assert_called_with('url1', list_subdirectory=False)
      self.assertEqual(
          self.mock_xb._GetLatestVersionFromGsDir(
              'url2', list_subdirectory=True, with_release=True),
          'R28-3912.101.0')
      ls_mock.assert_called_with('url2', list_subdirectory=True)

  def testLookupOfficial(self):
    """Basic test of _LookupOfficial. Checks that a given suffix is handled."""
    with mock.patch.object(gs.GSContext, 'Cat', return_value='v') as cat_mock:
      self.assertEqual(self.mock_xb._LookupOfficial('b', suffix='-s'), 'b-s/v')
      cat_mock.assert_called_with(
          'gs://chromeos-image-archive/b-s/LATEST-master')

  @mock.patch.object(xbuddy.XBuddy, '_GetLatestVersionFromGsDir',
                     side_effect=['4100.68.0', 'R28-4100.68.0'])
  def testLookupChannel(self, version_mock):
    """Basic test of _LookupChannel. Checks that a given suffix is handled."""
    self.assertEqual(self.mock_xb._LookupChannel('b', '-release'),
                     'b-release/R28-4100.68.0')
    version_mock.assert_called_with(
        'gs://chromeos-image-archive/b-release/R*4100.68.0',
        list_subdirectory=True)

  def testLookupAliasPathRewrite(self):
    """Tests LookupAlias of path rewrite, including keyword substitution."""
    path = 'remote/BOARD/VERSION/test'
    with mock.patch.object(
        self.mock_xb.config, 'get',
        side_effect=[configparser.Error, path]) as get_mock:
      self.assertEqual(('remote/parrot/1.2.3/test', '-release'),
                       self.mock_xb.LookupAlias('foobar', board='parrot',
                                                version='1.2.3'))
      get_mock.assert_called_with('PATH_REWRITES', 'foobar')

  def testLookupAliasSuffix(self):
    """Tests LookupAlias of location suffix."""
    with mock.patch.object(
        self.mock_xb.config, 'get',
        side_effect=['-random', configparser.Error]) as get_mock:
      self.assertEqual(('foobar', '-random'),
                       self.mock_xb.LookupAlias('foobar', board='parrot',
                                                version='1.2.3'))
      get_mock.assert_called_with('PATH_REWRITES', 'foobar')

  def testLookupAliasPathRewriteAndSuffix(self):
    """Tests LookupAlias with both path rewrite and suffix."""
    path = 'remote/BOARD/VERSION/test'
    with mock.patch.object(
        self.mock_xb.config, 'get',
        side_effect=['-random', path]) as get_mock:
      self.assertEqual(('remote/parrot/1.2.3/test', '-random'),
                       self.mock_xb.LookupAlias('foobar', board='parrot',
                                                version='1.2.3'))
      get_mock.assert_called_with('PATH_REWRITES', 'foobar')

  @mock.patch.object(xbuddy.XBuddy, '_LookupOfficial')
  def testResolveVersionToBuildIdAndChannel_Official(self, lookup_mock):
    """Check _ResolveVersionToBuildIdAndChannel support for official build."""
    board = 'chell'
    suffix = '-s'

    version = 'latest-official'
    self.mock_xb._ResolveVersionToBuildIdAndChannel(board, suffix, version)
    lookup_mock.assert_called_with('chell', '-s', image_dir=None)

    self.mock_xb._ResolveVersionToBuildIdAndChannel(board, suffix, version,
                                                    image_dir=GS_ALTERNATE_DIR)
    lookup_mock.assert_called_with('chell', '-s',
                                   image_dir='gs://chromeos-alternate-archive/')

    version = 'latest-official-paladin'
    self.mock_xb._ResolveVersionToBuildIdAndChannel(board, suffix, version)
    lookup_mock.assert_called_with('chell', 'paladin', image_dir=None)

    self.mock_xb._ResolveVersionToBuildIdAndChannel(board, suffix, version,
                                                    image_dir=GS_ALTERNATE_DIR)
    lookup_mock.assert_called_with('chell', 'paladin',
                                   image_dir='gs://chromeos-alternate-archive/')

  @mock.patch.object(xbuddy.XBuddy, '_LookupChannel')
  def testResolveVersionToBuildIdAndChannel_Channel(self, lookup_mock):
    """Check _ResolveVersionToBuildIdAndChannel support for channels."""
    board = 'chell'
    suffix = '-s'

    version = 'latest'
    self.mock_xb._ResolveVersionToBuildIdAndChannel(board, suffix, version)
    lookup_mock.assert_called_with('chell', '-s', image_dir=None)

    self.mock_xb._ResolveVersionToBuildIdAndChannel(board, suffix, version,
                                                    image_dir=GS_ALTERNATE_DIR)
    lookup_mock.assert_called_with('chell', '-s',
                                   image_dir='gs://chromeos-alternate-archive/')

    version = 'latest-dev'
    self.mock_xb._ResolveVersionToBuildIdAndChannel(board, suffix, version)
    lookup_mock.assert_called_with('chell', '-s', channel='dev', image_dir=None)

    self.mock_xb._ResolveVersionToBuildIdAndChannel(board, suffix, version,
                                                    image_dir=GS_ALTERNATE_DIR)
    lookup_mock.assert_called_with('chell', '-s', channel='dev',
                                   image_dir='gs://chromeos-alternate-archive/')

  # TODO(dgarrett): Re-enable when crbug.com/585914 is fixed.
  # def testResolveVersionToBuildId_BaseVersion(self):
  #   """Check _ResolveVersionToBuildId handles a base version."""
  #   board = 'b'
  #   suffix = '-s'

  #   self.mox.StubOutWithMock(self.mock_xb, '_ResolveBuildVersion')
  #   self.mock_xb._ResolveBuildVersion(board, suffix, '1.2.3').AndReturn(
  #       'R12-1.2.3')
  #   self.mox.StubOutWithMock(self.mock_xb, '_RemoteBuildId')
  #   self.mock_xb._RemoteBuildId(board, suffix, 'R12-1.2.3')
  #   self.mox.ReplayAll()

  #   self.mock_xb._ResolveVersionToBuildId(board, suffix, '1.2.3')
  #   self.mox.VerifyAll()

  def testBasicInterpretPath(self):
    """Basic checks for splitting a path"""
    path = 'parrot/R27-2455.0.0/test'
    expected = ('test', 'parrot', 'R27-2455.0.0', True)
    self.assertEqual(self.mock_xb._InterpretPath(path=path), expected)

    path = 'parrot/R27-2455.0.0/full_payload'
    expected = ('full_payload', 'parrot', 'R27-2455.0.0', True)
    self.assertEqual(self.mock_xb._InterpretPath(path=path), expected)

    path = 'parrot/R27-2455.0.0'
    expected = ('ANY', 'parrot', 'R27-2455.0.0', True)
    self.assertEqual(self.mock_xb._InterpretPath(path=path), expected)

    path = 'remote/parrot/R27-2455.0.0'
    expected = ('test', 'parrot', 'R27-2455.0.0', False)
    self.assertEqual(self.mock_xb._InterpretPath(path=path), expected)

    path = 'local/parrot/R27-2455.0.0'
    expected = ('ANY', 'parrot', 'R27-2455.0.0', True)
    self.assertEqual(self.mock_xb._InterpretPath(path=path), expected)

    path = ''
    expected = ('ANY', None, 'latest', True)
    self.assertEqual(self.mock_xb._InterpretPath(path=path), expected)

    path = 'local'
    expected = ('ANY', None, 'latest', True)
    self.assertEqual(self.mock_xb._InterpretPath(path=path), expected)

    path = 'local/parrot/latest/ANY'
    expected = ('ANY', 'parrot', 'latest', True)
    self.assertEqual(self.mock_xb._InterpretPath(path=path), expected)

  def testInterpretPathWithDefaults(self):
    """Test path splitting with default board/version."""
    path = ''
    expected = ('ANY', 'parrot', 'latest', True)
    self.assertEqual(expected, self.mock_xb._InterpretPath(
        path=path, default_board='parrot'))

    path = ''
    expected = ('ANY', None, '1.2.3', True)
    self.assertEqual(expected, self.mock_xb._InterpretPath(
        path=path, default_version='1.2.3'))

    path = ''
    expected = ('ANY', 'parrot', '1.2.3', True)
    self.assertEqual(expected, self.mock_xb._InterpretPath(
        path=path, default_board='parrot', default_version='1.2.3'))

    path = '1.2.3'
    expected = ('ANY', None, '1.2.3', True)
    self.assertEqual(expected, self.mock_xb._InterpretPath(
        path=path, default_version='1.2.3'))

    path = 'latest'
    expected = ('ANY', None, 'latest', True)
    self.assertEqual(expected, self.mock_xb._InterpretPath(
        path=path, default_version='1.2.3'))

    path = '1.2.3'
    expected = ('ANY', 'parrot', '1.2.3', True)
    self.assertEqual(expected, self.mock_xb._InterpretPath(
        path=path, default_board='parrot', default_version='1.2.3'))

    path = 'parrot'
    expected = ('ANY', 'parrot', '1.2.3', True)
    self.assertEqual(expected, self.mock_xb._InterpretPath(
        path=path, default_version='1.2.3'))

  def testTimestampsAndList(self):
    """Creation and listing of builds according to their timestamps."""
    # make 3 different timestamp files
    b_id11 = 'b1/v1'
    b_id12 = 'b1/v2'
    b_id23 = 'b2/v3'
    xbuddy.Timestamp.UpdateTimestamp(self.mock_xb._timestamp_folder, b_id11)
    time.sleep(0.05)
    xbuddy.Timestamp.UpdateTimestamp(self.mock_xb._timestamp_folder, b_id12)
    time.sleep(0.05)
    xbuddy.Timestamp.UpdateTimestamp(self.mock_xb._timestamp_folder, b_id23)

    # reference second one again
    time.sleep(0.05)
    xbuddy.Timestamp.UpdateTimestamp(self.mock_xb._timestamp_folder, b_id12)

    # check that list returns the same 3 things, in last referenced order
    result = self.mock_xb._ListBuildTimes()
    self.assertEqual(result[0][0], b_id12)
    self.assertEqual(result[1][0], b_id23)
    self.assertEqual(result[2][0], b_id11)

  def testSyncRegistry(self):
    # check that there are no builds initially
    result = self.mock_xb._ListBuildTimes()
    self.assertEqual(len(result), 0)

    # set up the dummy build/images directory with images
    boards = ['a', 'b']
    versions = ['v1', 'v2']
    for b in boards:
      os.makedirs(os.path.join(self.mock_xb.images_dir, b))
      for v in versions:
        os.makedirs(os.path.join(self.mock_xb.images_dir, b, v))

    # Sync and check that they've been added to xBuddy's registry
    self.mock_xb._SyncRegistryWithBuildImages()
    result = self.mock_xb._ListBuildTimes()
    self.assertEqual(len(result), 4)

  def testXBuddyCaching(self):
    """Caching & replacement of timestamp files."""

    def _ReleaseOnly(name):
      # All non-release URLs are invalid so we can meet expectations.
      if name.find('-release') == -1:
        raise gs.GSContextException('bad URL')
      return name

    with mock.patch.object(
        gs.GSContext, 'LS', side_effect=_ReleaseOnly) as ls_mock:
      with mock.patch.object(
          self.mock_xb, '_Download') as download_mock:
        version = '%s-release/R0'

        def _Download(image):
          gs_image = 'gs://chromeos-image-archive/' + version
          self.mock_xb.Get(('remote', image, 'R0', 'test'))
          ls_mock.assert_called_with(gs_image % image)
          download_mock.assert_called_with(gs_image % image, ['test_image'],
                                           version % image)
          time.sleep(0.05)

        # Requires default capacity.
        self.assertEqual(self.mock_xb.Capacity(), '5')

        # Get 6 different images: a,b,c,d,e,f.
        images = ['a', 'b', 'c', 'd', 'e', 'f']
        for image in images:
          _Download(image)

        # Check that b,c,d,e,f are still stored.
        result = self.mock_xb._ListBuildTimes()
        self.assertEqual(len(result), 5)

        # Flip the list to get reverse chronological order
        images.reverse()
        for i in range(5):
          self.assertEqual(result[i][0], version % images[i])

        # Get b,a.
        images = ['b', 'a']
        for image in images:
          _Download(image)

        # Check that d,e,f,b,a are still stored.
        result = self.mock_xb._ListBuildTimes()
        self.assertEqual(len(result), 5)
        images_expected = ['a', 'b', 'f', 'e', 'd']
        for i in range(5):
          self.assertEqual(result[i][0], '%s-release/R0' % images_expected[i])
