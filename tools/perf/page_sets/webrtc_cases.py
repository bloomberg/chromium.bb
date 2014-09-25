# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class WebrtcCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(WebrtcCasesPage, self).__init__(url=url, page_set=page_set)


class Page1(WebrtcCasesPage):

  """ Why: Simple test page only showing a local video stream """

  def __init__(self, page_set):
    super(Page1, self).__init__(
      url='file://webrtc/local-video.html',
      page_set=page_set)

  def RunWebrtc(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(10)
    action_runner.ExecuteJavaScript('checkForErrors();')


class Page2(WebrtcCasesPage):

  """ Why: Loopback video call using the PeerConnection API. """

  def __init__(self, page_set):
    super(Page2, self).__init__(
      url='file://third_party/webrtc/samples/js/demos/html/pc1.html',
      page_set=page_set)

  def RunWebrtc(self, action_runner):
    action_runner.ClickElement('button[id="btn1"]')
    action_runner.Wait(2)
    action_runner.ClickElement('button[id="btn2"]')
    action_runner.Wait(10)
    action_runner.ClickElement('button[id="btn3"]')

class Page3(WebrtcCasesPage):

  """ Why: Acquires a high definition local stream. """

  def __init__(self, page_set):
    super(Page3, self).__init__(
      url=('http://googlechrome.github.io/webrtc/samples/web/content/'
           'getusermedia-resolution/'),
      page_set=page_set)

  def RunWebrtc(self, action_runner):
    action_runner.ClickElement('button[id="hd"]')
    action_runner.Wait(10)


class WebrtcCasesPageSet(page_set_module.PageSet):

  """ WebRTC tests for Real-time audio and video communication. """

  def __init__(self):
    super(WebrtcCasesPageSet, self).__init__(
      archive_data_file='data/webrtc_cases.json',
      bucket=page_set_module.PUBLIC_BUCKET,
      serving_dirs=['third_party/webrtc/samples/js'])

    self.AddPage(Page1(self))
    self.AddPage(Page2(self))
    self.AddPage(Page3(self))
