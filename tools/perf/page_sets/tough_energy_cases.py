# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughEnergyCasesPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, page_set):
    super(ToughEnergyCasesPage, self).__init__(url=url, page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.archive_data_file = 'data/tough_energy_cases.json'


class GmailPage(ToughEnergyCasesPage):

  """ Why: productivity, top google properties """

  def __init__(self, page_set):
    super(GmailPage, self).__init__(
      url='https://mail.google.com/mail/',
      page_set=page_set)

    self.credentials = 'google'

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'javascript': (
          'window.gmonkey !== undefined &&'
          'document.getElementById("gb") !== null')
      }))


class ToughEnergyCasesPageSet(page_set_module.PageSet):

  """ Pages for measuring Chrome power draw. """

  def __init__(self):
    super(ToughEnergyCasesPageSet, self).__init__(
      credentials_path='data/credentials.json',
      archive_data_file='data/tough_energy_cases.json')

    # Why: Above the fold animated gif running in the background
    self.AddPage(ToughEnergyCasesPage(
      'file://tough_energy_cases/above-fold-animated-gif.html',
      self))
    self.AddPage(GmailPage(self))
    # Why: Below the fold animated gif
    self.AddPage(ToughEnergyCasesPage(
      'file://tough_energy_cases/below-fold-animated-gif.html',
      self))
    # Why: Below the fold flash animation
    self.AddPage(ToughEnergyCasesPage(
      'file://tough_energy_cases/below-fold-flash.html',
      self))
