# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from chrome_ent_test.ent_tests import ChromeEnterpriseTestCase
from chrome_ent_test.infra.core import environment, before_all, test


@environment(file="../policy_test.asset.textpb")
class TranslateEnabledTest(ChromeEnterpriseTestCase):
  """Test the TranslateEnabled policy.

  See https://www.chromium.org/administrators/policy-list-3#TranslateEnabled"""

  @before_all
  def setup(self):
    self.InstallChrome('client2012')
    self.InstallWebDriver('client2012')

  def isChromeTranslateEnabled(self, incognito=False):
    dir = os.path.dirname(os.path.abspath(__file__))
    output = self.RunWebDriverTest(
        'client2012', os.path.join(dir, 'translate_enabled_webdriver_test.py'),
        ['--incognito'] if incognito else [])
    return "TRUE" in output

  @test
  def test_TranslatedDisabled(self, incognito=False):
    self.SetPolicy('win2012-dc', 'TranslateEnabled', 0, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    enabled = self.isChromeTranslateEnabled()
    self.assertFalse(enabled)

  @test
  def test_TranslatedEnabled(self, incognito=False):
    self.SetPolicy('win2012-dc', 'TranslateEnabled', 1, 'DWORD')
    self.RunCommand('client2012', 'gpupdate /force')

    enabled = self.isChromeTranslateEnabled()
    self.assertTrue(enabled)

  @test
  def test_TranslatedDisabledIncognito(self):
    self.test_TranslatedDisabled(incognito=True)

  @test
  def test_TranslatedEnabledIncognito(self):
    self.test_TranslatedEnabled(incognito=True)
