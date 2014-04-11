# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class DhtmlPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, page_set):
    super(DhtmlPage, self).__init__(url=url, page_set=page_set)


class DhtmlPageSet(page_set_module.PageSet):

  """ DHTML page_cycler benchmark """

  def __init__(self):
    super(DhtmlPageSet, self).__init__(
      # pylint: disable=C0301
      serving_dirs=set(['../../../../data/page_cycler/dhtml']))

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
      self.AddPage(DhtmlPage(url, self))
