# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class Typical25Page(page_module.Page):

  def __init__(self, url, page_set):
    super(Typical25Page, self).__init__(url=url, page_set=page_set)
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/typical_25.json'

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class Typical25PageSet(page_set_module.PageSet):

  """ Pages designed to represent the median, not highly optimized web """

  def __init__(self):
    super(Typical25PageSet, self).__init__(
      user_agent_type='desktop',
      archive_data_file='data/typical_25.json',
      bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
      # Why: Alexa games #48
      'http://www.nick.com/games',
      # Why: Alexa sports #45
      'http://www.rei.com/',
      # Why: Alexa sports #50
      'http://www.fifa.com/',
      # Why: Alexa shopping #41
      'http://www.gamestop.com/ps3',
      # Why: Alexa shopping #25
      'http://www.barnesandnoble.com/u/books-bestselling-books/379003057/',
      # Why: Alexa news #55
      ('http://www.economist.com/news/science-and-technology/21573529-small-'
       'models-cosmic-phenomena-are-shedding-light-real-thing-how-build'),
      # Why: Alexa news #67
      'http://www.theonion.com',
      'http://arstechnica.com/',
      # Why: Alexa home #10
      'http://allrecipes.com/Recipe/Pull-Apart-Hot-Cross-Buns/Detail.aspx',
      'http://www.html5rocks.com/en/',
      'http://www.mlb.com/',
      # pylint: disable=C0301
      'http://gawker.com/5939683/based-on-a-true-story-is-a-rotten-lie-i-hope-you-never-believe',
      'http://www.imdb.com/title/tt0910970/',
      'http://www.flickr.com/search/?q=monkeys&f=hp',
      'http://money.cnn.com/',
      'http://www.nationalgeographic.com/',
      'http://premierleague.com',
      'http://www.osubeavers.com/',
      'http://walgreens.com',
      'http://colorado.edu',
      ('http://www.ticketmaster.com/JAY-Z-and-Justin-Timberlake-tickets/artist/'
       '1837448?brand=none&tm_link=tm_homeA_rc_name2'),
      # pylint: disable=C0301
      'http://www.theverge.com/2013/3/5/4061684/inside-ted-the-smartest-bubble-in-the-world',
      'http://www.airbnb.com/',
      'http://www.ign.com/',
      # Why: Alexa health #25
      'http://www.fda.gov',
    ]

    for url in urls_list:
      self.AddPage(Typical25Page(url, self))
