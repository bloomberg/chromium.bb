# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughDomMemoryCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(ToughDomMemoryCasesPage, self).__init__(url=url, page_set=page_set)
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/tough_dom_memory_cases.json'

  def RunStressMemory(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()

class ToughDomMemoryCasesPageSet(page_set_module.PageSet):

  """
  Websites in top 25 where some DOM objects seem to remain even
  after closing the page.
  """

  def __init__(self):
    super(ToughDomMemoryCasesPageSet, self).__init__(
      user_agent_type='desktop',
      archive_data_file='data/tough_dom_memory_cases.json',
      bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
      # pylint: disable=C0301
      'http://en.blog.wordpress.com/2012/09/04/freshly-pressed-editors-picks-for-august-2012/',
      # pylint: disable=C0301
      'http://en.blog.wordpress.com/2012/09/06/new-themes-gridspace-and-ascetica/',
      # pylint: disable=C0301
      'http://en.blog.wordpress.com/2012/09/20/new-themes-gigawatt-and-pinboard/',
      # pylint: disable=C0301
      'https://twitter.com/katyperry',
      'https://twitter.com/justinbieber',
      'https://twitter.com/BarackObama',
      'https://twitter.com/ladygaga'
    ]

    for url in urls_list:
      self.AddPage(ToughDomMemoryCasesPage(url, self))
