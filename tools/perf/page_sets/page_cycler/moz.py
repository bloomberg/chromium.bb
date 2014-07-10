# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module

class MozPage(page_module.Page):

  def __init__(self, url, page_set):
    super(MozPage, self).__init__(url=url, page_set=page_set)


class MozPageSet(page_set_module.PageSet):

  """ Moz page_cycler benchmark """

  def __init__(self):
    super(MozPageSet, self).__init__(
      # pylint: disable=C0301
      serving_dirs=set(['../../../../data/page_cycler/moz']),
      bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
      'file://../../../../data/page_cycler/moz/bugzilla.mozilla.org/',
      'file://../../../../data/page_cycler/moz/espn.go.com/',
      'file://../../../../data/page_cycler/moz/home.netscape.com/',
      'file://../../../../data/page_cycler/moz/hotwired.lycos.com/',
      'file://../../../../data/page_cycler/moz/lxr.mozilla.org/',
      'file://../../../../data/page_cycler/moz/my.netscape.com/',
      'file://../../../../data/page_cycler/moz/news.cnet.com/',
      'file://../../../../data/page_cycler/moz/slashdot.org/',
      'file://../../../../data/page_cycler/moz/vanilla-page/',
      'file://../../../../data/page_cycler/moz/web.icq.com/',
      'file://../../../../data/page_cycler/moz/www.altavista.com/',
      'file://../../../../data/page_cycler/moz/www.amazon.com/',
      'file://../../../../data/page_cycler/moz/www.aol.com/',
      'file://../../../../data/page_cycler/moz/www.apple.com/',
      'file://../../../../data/page_cycler/moz/www.cnn.com/',
      'file://../../../../data/page_cycler/moz/www.compuserve.com/',
      'file://../../../../data/page_cycler/moz/www.digitalcity.com/',
      'file://../../../../data/page_cycler/moz/www.ebay.com/',
      'file://../../../../data/page_cycler/moz/www.excite.com/',
      'file://../../../../data/page_cycler/moz/www.expedia.com/',
      'file://../../../../data/page_cycler/moz/www.google.com/',
      'file://../../../../data/page_cycler/moz/www.iplanet.com/',
      'file://../../../../data/page_cycler/moz/www.mapquest.com/',
      'file://../../../../data/page_cycler/moz/www.microsoft.com/',
      'file://../../../../data/page_cycler/moz/www.moviefone.com/',
      'file://../../../../data/page_cycler/moz/www.msn.com/',
      'file://../../../../data/page_cycler/moz/www.msnbc.com/',
      'file://../../../../data/page_cycler/moz/www.nytimes.com/',
      'file://../../../../data/page_cycler/moz/www.nytimes.com_Table/',
      'file://../../../../data/page_cycler/moz/www.quicken.com/',
      'file://../../../../data/page_cycler/moz/www.spinner.com/',
      'file://../../../../data/page_cycler/moz/www.sun.com/',
      'file://../../../../data/page_cycler/moz/www.time.com/',
      'file://../../../../data/page_cycler/moz/www.tomshardware.com/',
      'file://../../../../data/page_cycler/moz/www.travelocity.com/',
      'file://../../../../data/page_cycler/moz/www.voodooextreme.com/',
      'file://../../../../data/page_cycler/moz/www.w3.org_DOML2Core/',
      'file://../../../../data/page_cycler/moz/www.wired.com/',
      'file://../../../../data/page_cycler/moz/www.yahoo.com/',
      'file://../../../../data/page_cycler/moz/www.zdnet.com/',
      'file://../../../../data/page_cycler/moz/www.zdnet.com_Gamespot.com/'
    ]

    for url in urls_list:
      self.AddPage(MozPage(url, self))
