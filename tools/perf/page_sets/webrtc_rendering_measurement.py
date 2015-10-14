# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_test as page_test
from telemetry import story


class WebrtcRenderingPage(page_module.Page):

  def __init__(self, url, page_set):
    super(WebrtcRenderingPage, self).__init__(
      url=url,
      page_set=page_set,
      name='webrtc_rendering_page')

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('Action_Create_PeerConnection',
                                         repeatable=False):
      command = 'testCamera([%s, %s]);' % (1280, 720)
      action_runner.ExecuteJavaScript(command)
      action_runner.Wait(30)
      errors = action_runner.EvaluateJavaScript('errors')
      if errors:
        raise page_test.Failure('Errors on page: ' + ', '.join(errors))


class WebrtcRenderingPageSet(story.StorySet):
  """ A benchmark of webrtc rendering performance."""

  def __init__(self):
    super(WebrtcRenderingPageSet, self).__init__()

    url = ('file://webrtc_rendering/loopback_peerconnection.html')
    self.AddStory(WebrtcRenderingPage(url, self))
