# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class PathologicalMobileSitesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(PathologicalMobileSitesPage, self).__init__(
        url=url, page_set=page_set, credentials_path='data/credentials.json')
    self.user_agent_type = 'mobile'
    self.archive_data_file = 'data/pathological_mobile_sites.json'

  def RunPageInteractions(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class PathologicalMobileSitesPageSet(page_set_module.PageSet):

  """Pathologically bad and janky sites on mobile."""

  def __init__(self):
    super(PathologicalMobileSitesPageSet, self).__init__(
        user_agent_type='mobile',
        archive_data_file='data/pathological_mobile_sites.json',
        bucket=page_set_module.PARTNER_BUCKET)

    sites = ['http://edition.cnn.com',
             'http://m.espn.go.com/nhl/rankings',
             'http://recode.net',
             'http://www.latimes.com',
             ('http://www.pbs.org/newshour/bb/'
              'much-really-cost-live-city-like-seattle/#the-rundown'),
             'http://www.zdnet.com',
             'http://www.wowwiki.com/World_of_Warcraft:_Mists_of_Pandaria',
             'https://www.linkedin.com/in/linustorvalds']

    for site in sites:
      self.AddUserStory(PathologicalMobileSitesPage(site, self))
