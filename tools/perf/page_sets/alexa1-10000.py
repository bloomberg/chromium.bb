# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import os

from telemetry.page.page_set import PageSet
from telemetry.page.page import Page


__location__ = os.path.realpath(
      os.path.join(os.getcwd(), os.path.dirname(__file__)))

# Generated on 2013-09-03 13:59:53.459117 by rmistry using
# create_page_set.py.
_TOP_10000_ALEXA_FILE = os.path.join(__location__, 'alexa1-10000-urls.json')


class Alexa1To10000Page(Page):

  def __init__(self, url, page_set):
    super(Alexa1To10000Page, self).__init__(url=url, page_set=page_set)

  def RunPageInteractions(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class Alexa1To10000PageSet(PageSet):
  """ Top 1-10000 Alexa global.
      Generated on 2013-09-03 13:59:53.459117 by rmistry using
      create_page_set.py.
  """

  def __init__(self):
    super(Alexa1To10000PageSet, self).__init__(user_agent_type='desktop')

    with open(_TOP_10000_ALEXA_FILE) as f:
      urls_list = json.load(f)
    for url in urls_list:
      self.AddUserStory(Alexa1To10000Page(url, self))
