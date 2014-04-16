# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class PlusAltPostsPhotosPage(page_module.PageWithDefaultRunNavigate):

  """ Why: Alternate between clicking posts and albums """

  def __init__(self, page_set):
    super(PlusAltPostsPhotosPage, self).__init__(
      url='https://plus.google.com/+BarackObama/posts',
      page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.credentials = 'google'
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/plus_alt_posts_photos.json'
    self.name = 'plus_alt_posts_photos'

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'text': 'Barack Obama',
        'condition': 'element'
      }))
    action_runner.RunAction(WaitAction(
      {
        'condition': 'element',
        'selector': 'span[guidedhelpid="posts_tab_profile"][class*="s6U8x"]'
      }))

  def RunEndure(self, action_runner):
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'span[guidedhelpid="posts_tab_profile"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'condition': 'element',
        'selector': 'span[guidedhelpid="posts_tab_profile"][class*="s6U8x"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 5
      }))
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'span[guidedhelpid="photos_tab_profile"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'condition': 'element',
        'selector': 'span[guidedhelpid="photos_tab_profile"][class*="s6U8x"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 5
      }))


class PlusAltPostsPhotosPageSet(page_set_module.PageSet):

  """ Chrome Endure test for Google Plus. """

  def __init__(self):
    super(PlusAltPostsPhotosPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      archive_data_file='data/plus_alt_posts_photos.json')

    self.AddPage(PlusAltPostsPhotosPage(self))
