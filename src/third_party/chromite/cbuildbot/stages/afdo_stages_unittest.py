# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for updating Chrome ebuild stages."""

from __future__ import print_function

from chromite.cbuildbot import afdo
from chromite.cbuildbot.stages import afdo_stages
from chromite.cbuildbot.stages import generic_stages_unittest

from chromite.lib import gs
from chromite.lib import portage_util


def _GenerateAFDOGenerateTest(chrome_version_str,
                              expected_to_generate_profile,
                              test_name):
  class Test(generic_stages_unittest.AbstractStageTestCase):
    """Test that we don't update non-r1 AFDO profiles"""

    def setUp(self):
      self.PatchObject(afdo, 'CanGenerateAFDOData', lambda _: True)
      chrome_version = portage_util.SplitCPV(chrome_version_str)
      self.PatchObject(portage_util, 'PortageqBestVisible',
                       lambda *_, **_2: chrome_version)
      self.wait_for_data = self.PatchObject(afdo, 'WaitForAFDOPerfData',
                                            autospec=True,
                                            wraps=lambda *_: True)

      afdo_path = '/does/not/exist/afdo.prof'
      self.generate_afdo_data = self.PatchObject(afdo, 'GenerateAFDOData',
                                                 autospec=True,
                                                 wraps=lambda *_: afdo_path)

      self.PatchObject(afdo_stages.AFDODataGenerateStage, '_GetCurrentArch',
                       lambda *_: 'some_arch')

      self._Prepare()

    def ConstructStage(self):
      return afdo_stages.AFDODataGenerateStage(self._run, 'some_board')

    def testAFDOUpdateChromeEbuildStage(self):
      self.RunStage()
      if expected_to_generate_profile:
        self.assertTrue(self.wait_for_data.called)
        self.assertTrue(self.generate_afdo_data.called)
      else:
        self.assertFalse(self.wait_for_data.called)
        self.assertFalse(self.generate_afdo_data.called)

  Test.__name__ = test_name
  return Test


def _MakeChromeVersionWithRev(rev):
  return 'chromeos-chrome/chromeos-chrome-68.0.3436.0_rc-r%d' % rev


R1Test = _GenerateAFDOGenerateTest(
    chrome_version_str=_MakeChromeVersionWithRev(1),
    expected_to_generate_profile=True,
    test_name='AFDODataGenerateStageUpdatesR1ProfilesTest',
)

R2Test = _GenerateAFDOGenerateTest(
    chrome_version_str=_MakeChromeVersionWithRev(2),
    expected_to_generate_profile=False,
    test_name='AFDODataGenerateStageDoesntUpdateNonR1ProfilesTest',
)


class UpdateChromeEbuildTest(generic_stages_unittest.AbstractStageTestCase):
  """Test updating Chrome ebuild files"""

  def setUp(self):
    # Intercept afdo.UpdateChromeEbuildAFDOFile so that we can check how it is
    # called.
    self.patch_mock = self.PatchObject(afdo, 'UpdateChromeEbuildAFDOFile',
                                       autospec=True)
    # Don't care the return value of portage_util.PortageqBestVisible
    self.PatchObject(portage_util, 'PortageqBestVisible')
    # Don't care the return value of gs.GSContext
    self.PatchObject(gs, 'GSContext')
    # Don't call the getters; Use mock responses instead.
    self.PatchDict(afdo.PROFILE_SOURCES,
                   {'benchmark': lambda *_: 'benchmark.afdo',
                    'silvermont': lambda *_: 'silvermont.afdo'},
                   clear=True)
    self._Prepare()

  def ConstructStage(self):
    return afdo_stages.AFDOUpdateChromeEbuildStage(self._run)

  def testAFDOUpdateChromeEbuildStage(self):
    self.RunStage()

    # afdo.UpdateChromeEbuildAFDOFile should be called with the mock responses
    # from profile source specific query.
    self.patch_mock.assert_called_with(
        'amd64-generic',
        {'benchmark': 'benchmark.afdo',
         'silvermont': 'silvermont.afdo'})
