# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class VideoPage(page_module.Page):
  """A test page containing a video.

  Attributes:
      use_chrome_proxy: If true, fetches use the data reduction proxy.
          Otherwise, fetches are sent directly to the origin.
  """

  def __init__(self, url, page_set, use_chrome_proxy):
    super(VideoPage, self).__init__(url=url, page_set=page_set)
    self.use_chrome_proxy = use_chrome_proxy


class VideoPageSet(page_set_module.PageSet):
  """Base class for Chrome proxy video tests."""

  def __init__(self, mode):
    super(VideoPageSet, self).__init__()
    urls_list = [
        'http://check.googlezip.net/cacheable/video/buck_bunny_tiny.html',
    ]
    for url in urls_list:
      self._AddUserStoryForURL(url)

  def _AddUserStoryForURL(self, url):
    raise NotImplementedError


class VideoDirectPageSet(VideoPageSet):
  """Chrome proxy video tests: direct fetch."""
  def __init__(self):
    super(VideoDirectPageSet, self).__init__('direct')

  def _AddUserStoryForURL(self, url):
      self.AddUserStory(VideoPage(url, self, False))


class VideoProxiedPageSet(VideoPageSet):
  """Chrome proxy video tests: proxied fetch."""
  def __init__(self):
    super(VideoProxiedPageSet, self).__init__('proxied')

  def _AddUserStoryForURL(self, url):
      self.AddUserStory(VideoPage(url, self, True))


class VideoComparePageSet(VideoPageSet):
  """Chrome proxy video tests: compare direct and proxied fetches."""
  def __init__(self):
    super(VideoComparePageSet, self).__init__('compare')

  def _AddUserStoryForURL(self, url):
      self.AddUserStory(VideoPage(url, self, False))
      self.AddUserStory(VideoPage(url, self, True))
