#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for pushimage.py"""

from __future__ import print_function

import logging
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import gs_unittest
from chromite.lib import osutils
from chromite.lib import signing
from chromite.scripts import pushimage


class InputInsnsTest(cros_test_lib.MockTestCase):
  """Tests for InputInsns"""

  def testBasic(self):
    """Simple smoke test"""
    insns = pushimage.InputInsns('test.board')
    insns.GetInsnFile('recovery')
    self.assertEqual(insns.GetChannels(), ['dev', 'canary'])
    self.assertEqual(insns.GetKeysets(), ['stumpy-mp-v3'])

  def testGetInsnFile(self):
    """Verify various inputs result in right insns path"""
    testdata = (
        ('UPPER_CAPS', 'UPPER_CAPS'),
        ('recovery', 'test.board'),
        ('firmware', 'test.board.firmware'),
        ('factory', 'test.board.factory'),
    )
    insns = pushimage.InputInsns('test.board')
    for image_type, filename in testdata:
      ret = insns.GetInsnFile(image_type)
      self.assertEqual(os.path.basename(ret), '%s.instructions' % (filename))

  def testSplitCfgField(self):
    """Verify splitting behavior behaves"""
    testdata = (
        ('', []),
        ('a b c', ['a', 'b', 'c']),
        ('a, b', ['a', 'b']),
        ('a,b', ['a', 'b']),
        ('a,\tb', ['a', 'b']),
        ('a\tb', ['a', 'b']),
    )
    for val, exp in testdata:
      ret = pushimage.InputInsns.SplitCfgField(val)
      self.assertEqual(ret, exp)

  def testOutputInsnsBasic(self):
    """Verify output instructions are sane"""
    exp_content = """[insns]
keyset = stumpy-mp-v3
channel = dev canary
chromeos_shell = false
ensure_no_password = true
firmware_update = true
security_checks = true
create_nplusone = true

[general]
"""

    insns = pushimage.InputInsns('test.board')
    m = self.PatchObject(osutils, 'WriteFile')
    insns.OutputInsns('recovery', '/bogus', {}, {})
    self.assertTrue(m.called)
    content = m.call_args_list[0][0][1]
    self.assertEqual(content.rstrip(), exp_content.rstrip())

  def testOutputInsnsReplacements(self):
    """Verify output instructions can be updated"""
    exp_content = """[insns]
keyset = batman
channel = dev
chromeos_shell = false
ensure_no_password = true
firmware_update = true
security_checks = true
create_nplusone = true

[general]
board = board
config_board = test.board
"""
    sect_insns = {
        'channel': 'dev',
        'keyset': 'batman',
    }
    sect_general = {
        'config_board': 'test.board',
        'board': 'board',
    }

    insns = pushimage.InputInsns('test.board')
    m = self.PatchObject(osutils, 'WriteFile')
    insns.OutputInsns('recovery', '/a/file', sect_insns, sect_general)
    self.assertTrue(m.called)
    content = m.call_args_list[0][0][1]
    self.assertEqual(content.rstrip(), exp_content.rstrip())


class MarkImageToBeSignedTest(cros_test_lib.MockTestCase):
  """Tests for MarkImageToBeSigned()"""

  def setUp(self):
    self.gs_mock = self.StartPatcher(gs_unittest.GSContextMock())
    self.gs_mock.SetDefaultCmdResult()
    self.ctx = gs.GSContext()

    # Minor optimization -- we call this for logging purposes in the main
    # code, but don't really care about it for testing.  It just slows us.
    self.PatchObject(git, 'RunGit',
                     return_value=cros_build_lib.CommandResult(output='1234\n'))

  def testBasic(self):
    """Simple smoke test"""
    tbs_base = 'gs://some-bucket'
    insns_path = 'chan/board/ver/file.instructions'
    tbs_file = '%s/tobesigned/90,chan,board,ver,file.instructions' % tbs_base
    ret = pushimage.MarkImageToBeSigned(self.ctx, tbs_base, insns_path, 90)
    self.assertEqual(ret, tbs_file)

  def testPriority(self):
    """Verify diff priority values get used correctly"""
    for prio, sprio in ((0, '00'), (9, '09'), (35, '35'), (99, '99')):
      ret = pushimage.MarkImageToBeSigned(self.ctx, '', '', prio)
      self.assertEquals(ret, '/tobesigned/%s,' % sprio)

  def testBadPriority(self):
    """Verify we reject bad priority values"""
    for prio in (-10, -1, 100, 91239):
      self.assertRaises(ValueError, pushimage.MarkImageToBeSigned, self.ctx,
                        '', '', prio)

  def testTbsFile(self):
    """Make sure the tbs file we write has useful data"""
    WriteFile = osutils.WriteFile
    def _write_check(*args, **kwargs):
      # We can't mock every call, so do the actual write for most.
      WriteFile(*args, **kwargs)

    m = self.PatchObject(osutils, 'WriteFile')
    m.side_effect = _write_check
    pushimage.MarkImageToBeSigned(self.ctx, '', '', 50)
    # We assume the first call is the one we care about.
    self.assertTrue(m.called)
    content = m.call_args_list[0][0][1]
    self.assertIn('USER=', content)
    self.assertIn('HOSTNAME=', content)
    self.assertIn('GIT_REV=1234', content)
    self.assertIn('\n', content)

  def testTbsUpload(self):
    """Make sure we actually try to upload the file"""
    pushimage.MarkImageToBeSigned(self.ctx, '', '', 50)
    self.gs_mock.assertCommandContains(['cp', '--'])


class PushImageTests(cros_test_lib.MockTestCase):
  """Tests for PushImage()"""

  def setUp(self):
    self.gs_mock = self.StartPatcher(gs_unittest.GSContextMock())
    self.gs_mock.SetDefaultCmdResult()
    self.ctx = gs.GSContext()

    self.mark_mock = self.PatchObject(pushimage, 'MarkImageToBeSigned')

  def testBasic(self):
    """Simple smoke test"""
    EXPECTED = {
        'canary': [
            'gs://chromeos-releases/canary-channel/test.board-hi/5126.0.0/'
              'ChromeOS-recovery-R34-5126.0.0-test.board-hi.instructions'],
        'dev': [
            'gs://chromeos-releases/dev-channel/test.board-hi/5126.0.0/'
              'ChromeOS-recovery-R34-5126.0.0-test.board-hi.instructions'],
    }
    urls = pushimage.PushImage('/src', 'test.board', 'R34-5126.0.0',
                               profile='hi')

    self.assertEqual(urls, EXPECTED)

  def testBasicMock(self):
    """Simple smoke test in mock mode"""
    pushimage.PushImage('/src', 'test.board', 'R34-5126.0.0',
                        dry_run=True, mock=True)

  def testBadVersion(self):
    """Make sure we barf on bad version strings"""
    self.assertRaises(ValueError, pushimage.PushImage, '', '', 'asdf')

  def testNoInsns(self):
    """Boards w/out insn files should get skipped"""
    urls = pushimage.PushImage('/src', 'a bad bad board', 'R34-5126.0.0')
    self.assertEqual(self.gs_mock.call_count, 0)
    self.assertEqual(urls, None)

  def testSignTypesRecovery(self):
    """Only sign the requested recovery type"""
    EXPECTED = {
        'canary': [
            'gs://chromeos-releases/canary-channel/test.board/5126.0.0/'
              'ChromeOS-recovery-R34-5126.0.0-test.board.instructions'],
        'dev': [
            'gs://chromeos-releases/dev-channel/test.board/5126.0.0/'
              'ChromeOS-recovery-R34-5126.0.0-test.board.instructions'],
    }

    urls = pushimage.PushImage('/src', 'test.board', 'R34-5126.0.0',
                               sign_types=['recovery'])
    self.assertEqual(self.gs_mock.call_count, 18)
    self.assertTrue(self.mark_mock.called)
    self.assertEqual(urls, EXPECTED)

  def testSignTypesNone(self):
    """Verify nothing is signed when we request an unavailable type"""
    urls = pushimage.PushImage('/src', 'test.board', 'R34-5126.0.0',
                               sign_types=['nononononono'])
    self.assertEqual(self.gs_mock.call_count, 16)
    self.assertFalse(self.mark_mock.called)
    self.assertEqual(urls, {})


class MainTests(cros_test_lib.MockTestCase):
  """Tests for main()"""

  def setUp(self):
    self.PatchObject(pushimage, 'PushImage')

  def testBasic(self):
    """Simple smoke test"""
    pushimage.main(['--board', 'test.board', '/src', '--yes'])


if __name__ == '__main__':
  # Use our local copy of insns for testing as the main one is not
  # available in the public manifest.
  signing.INPUT_INSN_DIR = signing.TEST_INPUT_INSN_DIR

  # Run the tests.
  cros_test_lib.main(level=logging.INFO)
