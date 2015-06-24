# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module

class VideoFramePageSet(page_set_module.PageSet):
  """Chrome proxy video tests: verify frames of transcoded videos"""
  def __init__(self):
    super(VideoFramePageSet, self).__init__()
    for url in [
        'http://check.googlezip.net/cacheable/video/buck_bunny_640x360_24fps_video.html',
        'http://check.googlezip.net/cacheable/video/buck_bunny_60fps_video.html',
        ]:
      self.AddUserStory(page_module.Page(url, self))

class VideoAudioPageSet(page_set_module.PageSet):
  """Chrome proxy video tests: verify audio of transcoded videos"""
  def __init__(self):
    super(VideoAudioPageSet, self).__init__()
    for url in [
        'http://check.googlezip.net/cacheable/video/buck_bunny_640x360_24fps_audio.html',
        ]:
      self.AddUserStory(page_module.Page(url, self))
