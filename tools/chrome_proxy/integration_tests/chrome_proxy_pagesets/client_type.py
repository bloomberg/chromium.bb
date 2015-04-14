# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ClientTypePage(page_module.Page):
  """A test page for the chrome proxy client type tests.

  Attributes:
      bypass_for_client_type: The client type Chrome-Proxy header directive that
          would get a bypass when this page is fetched through the data
          reduction proxy. For example, a value of "android" means that this
          page would cause a bypass when fetched from a client that sets
          "Chrome-Proxy: c=android".
  """

  def __init__(self, url, page_set, bypass_for_client_type):
    super(ClientTypePage, self).__init__(url=url, page_set=page_set)
    self.bypass_for_client_type = bypass_for_client_type


class ClientTypePageSet(page_set_module.PageSet):
  """Chrome proxy test sites"""

  def __init__(self):
    super(ClientTypePageSet, self).__init__()

    # Page that should not bypass for any client types. This page is here in
    # order to determine the Chrome-Proxy client type value before running any
    # of the following pages, since there's no way to get the client type value
    # from a request that was bypassed.
    self.AddUserStory(ClientTypePage(
        url='http://aws1.mdw.la/fw',
        page_set=self,
        bypass_for_client_type='none'))

    # Page that should cause a bypass for android chrome clients.
    self.AddUserStory(ClientTypePage(
        url='http://check.googlezip.net/chrome-proxy-header/c=ANDROID',
        page_set=self,
        bypass_for_client_type='android'))

    # Page that should cause a bypass for android webview clients.
    self.AddUserStory(ClientTypePage(
        url='http://check.googlezip.net/chrome-proxy-header/c=WEBVIEW',
        page_set=self,
        bypass_for_client_type='webview'))

    # Page that should cause a bypass for iOS clients.
    self.AddUserStory(ClientTypePage(
        url='http://check.googlezip.net/chrome-proxy-header/c=IOS',
        page_set=self,
        bypass_for_client_type='ios'))

    # Page that should cause a bypass for Linux clients.
    self.AddUserStory(ClientTypePage(
        url='http://check.googlezip.net/chrome-proxy-header/c=LINUX',
        page_set=self,
        bypass_for_client_type='linux'))

    # Page that should cause a bypass for Windows clients.
    self.AddUserStory(ClientTypePage(
        url='http://check.googlezip.net/chrome-proxy-header/c=WIN',
        page_set=self,
        bypass_for_client_type='win'))

    # Page that should cause a bypass for ChromeOS clients.
    self.AddUserStory(ClientTypePage(
        url='http://check.googlezip.net/chrome-proxy-header/c=CHROMEOS',
        page_set=self,
        bypass_for_client_type='chromeos'))
