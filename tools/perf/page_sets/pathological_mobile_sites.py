# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story


class PathologicalMobileSitesPage(page_module.Page):

  def __init__(self, name, url, page_set):
    super(PathologicalMobileSitesPage, self).__init__(
        url=url, page_set=page_set,
        shared_page_state_class=shared_page_state.SharedMobilePageState,
        name=name)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()


class PathologicalMobileSitesPageSet(story.StorySet):

  """Pathologically bad and janky sites on mobile."""

  def __init__(self):
    super(PathologicalMobileSitesPageSet, self).__init__(
        archive_data_file='data/pathological_mobile_sites.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    sites = [('cnn_pathological', 'http://edition.cnn.com'),
             ('espn_pathological', 'http://m.espn.go.com/nhl/rankings'),
             ('recode_pathological', 'http://recode.net'),
             ('yahoo_sports_pathological', 'http://sports.yahoo.com/'),
             ('latimes_pathological', 'http://www.latimes.com'),
             ('pbs_pathological', 'http://www.pbs.org/newshour/bb/'
              'much-really-cost-live-city-like-seattle/#the-rundown'),
             ('guardian_pathological', 'http://www.theguardian.com/politics/2015/mar/09/'
              'ed-balls-tory-spending-plans-nhs-charging'),
             ('zdnet_pathological', 'http://www.zdnet.com'),
             ('wow_wiki_pathological', 'http://www.wowwiki.com/'
              'World_of_Warcraft:_Mists_of_Pandaria'),
             ('linkedin_pathological', 'https://www.linkedin.com/in/linustorvalds')]

    for name, site in sites:
      self.AddStory(PathologicalMobileSitesPage(name=name,
                                                url=site,
                                                page_set=self))
