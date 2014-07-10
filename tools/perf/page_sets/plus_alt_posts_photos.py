# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class PlusAltPostsPhotosPage(page_module.Page):

  """ Why: Alternate between clicking posts and albums """

  def __init__(self, page_set):
    super(PlusAltPostsPhotosPage, self).__init__(
      url='https://plus.google.com/+BarackObama/posts',
      page_set=page_set,
      name='plus_alt_posts_photos')
    self.credentials_path = 'data/credentials.json'
    self.credentials = 'google'
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/plus_alt_posts_photos.json'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForElement(text='Barack Obama')
    action_runner.WaitForElement(
        'span[guidedhelpid="posts_tab_profile"][class*="s6U8x"]')

  def RunEndure(self, action_runner):
    action_runner.ClickElement('span[guidedhelpid="posts_tab_profile"]')
    action_runner.WaitForElement(
        'span[guidedhelpid="posts_tab_profile"][class*="s6U8x"]')
    action_runner.Wait(5)
    action_runner.ClickElement('span[guidedhelpid="photos_tab_profile"]')
    action_runner.WaitForElement(
        'span[guidedhelpid="photos_tab_profile"][class*="s6U8x"]')
    action_runner.Wait(5)


class PlusAltPostsPhotosPageSet(page_set_module.PageSet):

  """ Chrome Endure test for Google Plus. """

  def __init__(self):
    super(PlusAltPostsPhotosPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      archive_data_file='data/plus_alt_posts_photos.json',
      bucket=page_set_module.PUBLIC_BUCKET)

    self.AddPage(PlusAltPostsPhotosPage(self))
