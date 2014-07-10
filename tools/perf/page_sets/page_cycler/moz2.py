# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module

class Moz2Page(page_module.Page):

  def __init__(self, url, page_set):
    super(Moz2Page, self).__init__(url=url, page_set=page_set)


class Moz2PageSet(page_set_module.PageSet):

  """ Description: Moz2 page_cycler benchmark """

  def __init__(self):
    super(Moz2PageSet, self).__init__(
      # pylint: disable=C0301
      serving_dirs=set(['../../../../data/page_cycler/moz2']),
      bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
      'file://../../../../data/page_cycler/moz2/bugzilla.mozilla.org/',
      'file://../../../../data/page_cycler/moz2/espn.go.com/',
      'file://../../../../data/page_cycler/moz2/home.netscape.com/',
      'file://../../../../data/page_cycler/moz2/hotwired.lycos.com/',
      'file://../../../../data/page_cycler/moz2/lxr.mozilla.org/',
      'file://../../../../data/page_cycler/moz2/my.netscape.com/',
      'file://../../../../data/page_cycler/moz2/news.cnet.com/',
      'file://../../../../data/page_cycler/moz2/slashdot.org/',
      'file://../../../../data/page_cycler/moz2/vanilla-page/',
      'file://../../../../data/page_cycler/moz2/web.icq.com/',
      'file://../../../../data/page_cycler/moz2/www.altavista.com/',
      'file://../../../../data/page_cycler/moz2/www.amazon.com/',
      'file://../../../../data/page_cycler/moz2/www.aol.com/',
      'file://../../../../data/page_cycler/moz2/www.apple.com/',
      'file://../../../../data/page_cycler/moz2/www.cnn.com/',
      'file://../../../../data/page_cycler/moz2/www.compuserve.com/',
      'file://../../../../data/page_cycler/moz2/www.digitalcity.com/',
      'file://../../../../data/page_cycler/moz2/www.ebay.com/',
      'file://../../../../data/page_cycler/moz2/www.excite.com/',
      'file://../../../../data/page_cycler/moz2/www.expedia.com/',
      'file://../../../../data/page_cycler/moz2/www.google.com/',
      'file://../../../../data/page_cycler/moz2/www.iplanet.com/',
      'file://../../../../data/page_cycler/moz2/www.mapquest.com/',
      'file://../../../../data/page_cycler/moz2/www.microsoft.com/',
      'file://../../../../data/page_cycler/moz2/www.moviefone.com/',
      'file://../../../../data/page_cycler/moz2/www.msn.com/',
      'file://../../../../data/page_cycler/moz2/www.msnbc.com/',
      'file://../../../../data/page_cycler/moz2/www.nytimes.com/',
      'file://../../../../data/page_cycler/moz2/www.nytimes.com_Table/',
      'file://../../../../data/page_cycler/moz2/www.quicken.com/',
      'file://../../../../data/page_cycler/moz2/www.spinner.com/',
      'file://../../../../data/page_cycler/moz2/www.sun.com/',
      'file://../../../../data/page_cycler/moz2/www.time.com/',
      'file://../../../../data/page_cycler/moz2/www.tomshardware.com/',
      'file://../../../../data/page_cycler/moz2/www.travelocity.com/',
      'file://../../../../data/page_cycler/moz2/www.voodooextreme.com/',
      'file://../../../../data/page_cycler/moz2/www.w3.org_DOML2Core/',
      'file://../../../../data/page_cycler/moz2/www.wired.com/',
      'file://../../../../data/page_cycler/moz2/www.yahoo.com/',
      'file://../../../../data/page_cycler/moz2/www.zdnet.com/',
      'file://../../../../data/page_cycler/moz2/www.zdnet.com_Gamespot.com/'
    ]

    for url in urls_list:
      self.AddPage(Moz2Page(url, self))
