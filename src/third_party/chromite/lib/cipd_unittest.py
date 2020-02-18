# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for cipd."""

from __future__ import print_function

import hashlib
import json
import mock

import httplib2

from chromite.lib import cipd
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import path_util


class CIPDTest(cros_test_lib.MockTestCase):
  """Tests for chromite.lib.cipd"""

  def testDownloadCIPD(self):
    MockHttp = self.PatchObject(httplib2, 'Http')
    first_body = b")]}'\n" + json.dumps({
        'clientBinary': {
            'signedUrl': 'http://example.com',
        },
        'clientRefAliases': [
            {
                'hashAlgo': 'SKIP',
                'hexDigest': 'aaaa',
            },
            {
                'hashAlgo': 'SHA256',
                'hexDigest': 'bogus-sha256',
            },
        ],
    }).encode('utf-8')
    response = mock.Mock()
    response.status = 200
    MockHttp.return_value.request.side_effect = [
        (response, first_body),
        (response, b'bogus binary file')]

    sha1 = self.PatchObject(hashlib, 'sha256')
    sha1.return_value.hexdigest.return_value = 'bogus-sha256'

    # pylint: disable=protected-access
    self.assertEqual(b'bogus binary file',
                     cipd._DownloadCIPD('bogus-instance-sha256'))


class CipdCacheTest(cros_test_lib.MockTempDirTestCase):
  """Tests for CipdCache helper."""

  def setUp(self):
    self.download_mock = self.PatchObject(cipd, '_DownloadCIPD',
                                          return_value=b'data')

  def testFetch(self):
    """Check CipdCache._Fetch behavior."""
    cache = cipd.CipdCache(self.tempdir)
    ref = cache.Lookup(('1234',))
    ref.SetDefault('cipd://1234')
    self.assertEqual('data', osutils.ReadFile(ref.path))

  def testGetCIPDFromCache(self):
    """Check GetCIPDFromCache behavior."""
    self.PatchObject(path_util, 'GetCacheDir', return_value=self.tempdir)
    path = cipd.GetCIPDFromCache()
    # This is more about making sure the func doesn't crash than inspecting the
    # internal caching logic (which is handled by lib.cache_unittest already).
    self.assertTrue(path.startswith(self.tempdir))
