# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module

class PassThroughPage(page_module.Page):
  """
  A test page for the chrome proxy pass-through tests.
  """

  def __init__(self, url, page_set):
    super(PassThroughPage, self).__init__(url=url, page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    super(PassThroughPage, self).RunNavigateSteps(action_runner)
    action_runner.ExecuteJavaScript('''
        (function() {
          var request = new XMLHttpRequest();
          request.open("GET", "%s");
          request.setRequestHeader("Chrome-Proxy", "pass-through");
          request.send(null);
        })();''' % (self.url))
    action_runner.Wait(1)


class PassThroughPageSet(page_set_module.PageSet):
  """ Chrome proxy test sites """

  def __init__(self):
    super(PassThroughPageSet, self).__init__()

    urls_list = [
      'http://check.googlezip.net/image.png',
    ]

    for url in urls_list:
      self.AddUserStory(PassThroughPage(url, self))