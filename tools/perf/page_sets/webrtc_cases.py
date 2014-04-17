# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class WebrtcCasesPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, page_set):
    super(WebrtcCasesPage, self).__init__(url=url, page_set=page_set)


class Page1(WebrtcCasesPage):

  """ Why: Simple test page only showing a local video stream """

  def __init__(self, page_set):
    super(Page1, self).__init__(
      url='file://webrtc/local-video.html',
      page_set=page_set)

  def RunWebrtc(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'seconds': 10
      }))
    action_runner.RunAction(JavascriptAction(
      {
        'expression': 'checkForErrors();'
      }))


class Page2(WebrtcCasesPage):

  """ Why: Loopback video call using the PeerConnection API. """

  def __init__(self, page_set):
    super(Page2, self).__init__(
      url='file://third_party/webrtc/samples/js/demos/html/pc1.html',
      page_set=page_set)

  def RunEndure(self, action_runner):
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'button[id="btn1"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'button[id="btn2"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 10
      }))
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'button[id="btn3"]'
      }))

  def RunWebrtc(self, action_runner):
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'button[id="btn1"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 2
      }))
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'button[id="btn2"]'
      }))
    action_runner.RunAction(WaitAction(
      {
        'seconds': 10
      }))
    action_runner.RunAction(ClickElementAction(
      {
        'selector': 'button[id="btn3"]'
      }))


class WebrtcCasesPageSet(page_set_module.PageSet):

  """ WebRTC tests for Real-time audio and video communication. """

  def __init__(self):
    super(WebrtcCasesPageSet, self).__init__(
      serving_dirs=['third_party/webrtc/samples/js'])

    self.AddPage(Page1(self))
    self.AddPage(Page2(self))
