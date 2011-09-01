#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for rietveld.py."""

import logging
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT_DIR, '..'))

import patch
import rietveld

# Access to a protected member XX of a client class
# pylint: disable=W0212


class RietveldTest(unittest.TestCase):
  def test_get_patch_empty(self):
    r = rietveld.Rietveld('url', 'email', 'password')
    r._send = lambda *args, **kwargs: '{}'
    patches = r.get_patch(123, 456)
    self.assertTrue(isinstance(patches, patch.PatchSet))
    self.assertEquals([], patches.patches)

  def test_get_patch_no_status(self):
    r = rietveld.Rietveld('url', 'email', 'password')
    r._send = lambda *args, **kwargs: (
        '{'
        '  "files":'
        '    {'
        '      "file_a":'
        '        {'
        '        }'
        '    }'
        '}')
    try:
      r.get_patch(123, 456)
      self.fail()
    except patch.UnsupportedPatchFormat, e:
      self.assertEquals('file_a', e.filename)

  def test_get_patch_two_files(self):
    output = (
        '{'
        '  "files":'
        '    {'
        '      "file_a":'
        '        {'
        '          "status": "A",'
        '          "is_binary": false,'
        '          "num_chunks": 1,'
        '          "id": 789'
        '        }'
        '    }'
        '}')
    r = rietveld.Rietveld('url', 'email', 'password')
    r._send = lambda *args, **kwargs: output
    patches = r.get_patch(123, 456)
    self.assertTrue(isinstance(patches, patch.PatchSet))
    self.assertEquals(1, len(patches.patches))
    obj = patches.patches[0]
    self.assertEquals(patch.FilePatchDiff, obj.__class__)
    self.assertEquals('file_a', obj.filename)
    self.assertEquals([], obj.svn_properties)
    self.assertEquals(False, obj.is_git_diff)
    self.assertEquals(0, obj.patchlevel)
    # This is because Rietveld._send() always returns the same buffer.
    self.assertEquals(output, obj.get())

  def testSvnProperties(self):
    # Line too long (N/80)
    # pylint: disable=C0301

    # To test one of these, run something like
    # import json, pprint, urllib
    # url = 'http://codereview.chromium.org/api/202046/1'
    # pprint.pprint(json.load(urllib.urlopen(url))['files'])

    # svn:mergeinfo across branches:
    # http://codereview.chromium.org/202046/diff/1/third_party/libxml/xmlcatalog_dummy.cc
    self.assertEquals(
        [('svn:eol-style', 'LF')],
        rietveld.Rietveld.parse_svn_properties(
          u'\nAdded: svn:eol-style\n   + LF\n', 'foo'))

    # svn:eol-style property that is lost in the diff
    # http://codereview.chromium.org/202046/diff/1/third_party/libxml/xmllint_dummy.cc
    self.assertEquals(
        [],
        rietveld.Rietveld.parse_svn_properties(
          u'\nAdded: svn:mergeinfo\n'
          '   Merged /branches/chrome_webkit_merge_branch/third_party/'
          'libxml/xmldummy_mac.cc:r69-2775\n',
          'foo'))

    self.assertEquals(
        [],
        rietveld.Rietveld.parse_svn_properties(u'', 'foo'))

    try:
      rietveld.Rietveld.parse_svn_properties(u'\n', 'foo')
      self.fail()
    except rietveld.patch.UnsupportedPatchFormat, e:
      self.assertEquals('foo', e.filename)
    # TODO(maruel): Change with no diff, only svn property change:
    # http://codereview.chromium.org/6462019/


if __name__ == '__main__':
  logging.basicConfig(level=logging.ERROR)
  unittest.main()
