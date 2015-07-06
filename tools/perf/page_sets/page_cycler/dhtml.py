# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry import story


class DhtmlPage(page_module.Page):

  def __init__(self, url, page_set):
    super(DhtmlPage, self).__init__(url=url, page_set=page_set)


class DhtmlPageSet(story.StorySet):

  """ DHTML page_cycler benchmark """

  def __init__(self):
    super(DhtmlPageSet, self).__init__(
      # pylint: disable=C0301
      serving_dirs=set(['../../../../data/page_cycler/dhtml']),
      cloud_storage_bucket=story.PARTNER_BUCKET)

    urls_list = [
      'file://../../../../data/page_cycler/dhtml/colorfade/',
      'file://../../../../data/page_cycler/dhtml/diagball/',
      'file://../../../../data/page_cycler/dhtml/fadespacing/',
      'file://../../../../data/page_cycler/dhtml/imageslide/',
      'file://../../../../data/page_cycler/dhtml/layers1/',
      'file://../../../../data/page_cycler/dhtml/layers2/',
      'file://../../../../data/page_cycler/dhtml/layers4/',
      'file://../../../../data/page_cycler/dhtml/layers5/',
      'file://../../../../data/page_cycler/dhtml/layers6/',
      'file://../../../../data/page_cycler/dhtml/meter/',
      'file://../../../../data/page_cycler/dhtml/movingtext/',
      'file://../../../../data/page_cycler/dhtml/mozilla/',
      'file://../../../../data/page_cycler/dhtml/replaceimages/',
      'file://../../../../data/page_cycler/dhtml/scrolling/',
      'file://../../../../data/page_cycler/dhtml/slidein/',
      'file://../../../../data/page_cycler/dhtml/slidingballs/',
      'file://../../../../data/page_cycler/dhtml/zoom/'
    ]

    for url in urls_list:
      self.AddStory(DhtmlPage(url, self))
