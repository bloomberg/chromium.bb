# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class BlockOncePage(page_module.Page):

  def __init__(self, url, page_set):
    super(BlockOncePage, self).__init__(url=url, page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    super(BlockOncePage, self).RunNavigateSteps(action_runner)
    # Test block-once on a POST request.
    # Ensure that a subsequent request uses the data reduction proxy.
    action_runner.ExecuteJavaScript('''
      (function() {
        var request = new XMLHttpRequest();
        request.open("POST",
            "http://chromeproxy-test.appspot.com/default?respBody=T0s=&respStatus=200&flywheelAction=block-once");
        request.onload = function() {
          var viaProxyRequest = new XMLHttpRequest();
          viaProxyRequest.open("GET",
              "http://check.googlezip.net/image.png");
          viaProxyRequest.send();
        };
        request.send();
      })();
    ''')
    action_runner.Wait(1)

class BlockOncePageSet(page_set_module.PageSet):

  """ Chrome proxy test sites """

  def __init__(self):
    super(BlockOncePageSet, self).__init__()

    # Test block-once for a GET request.
    urls_list = [
      'http://check.googlezip.net/blocksingle/',
    ]

    for url in urls_list:
      self.AddUserStory(BlockOncePage(url, self))
