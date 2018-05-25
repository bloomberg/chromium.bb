# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms

from telemetry import story
from telemetry.page import shared_page_state

from py_utils import discover


class RenderingStorySet(story.StorySet):
  """Stories related to rendering."""
  def __init__(self, platform, scroll_forever=False):
    super(RenderingStorySet, self).__init__(
        archive_data_file=('../data/rendering_%s.json' % platform),
        cloud_storage_bucket=story.PARTNER_BUCKET)

    assert platform in platforms.ALL_PLATFORMS

    if platform == platforms.MOBILE:
      shared_page_state_class = shared_page_state.SharedMobilePageState
    elif platform == platforms.DESKTOP:
      shared_page_state_class = shared_page_state.SharedDesktopPageState
    else:
      shared_page_state_class = shared_page_state.SharedPageState

    self.scroll_forever = scroll_forever

    for story_class in _IterAllRenderingStoryClasses():
      if (story_class.ABSTRACT_STORY or
          platform not in story_class.SUPPORTED_PLATFORMS):
        continue

      self.AddStory(story_class(
          page_set=self,
          shared_page_state_class=shared_page_state_class))

      if (platform == platforms.MOBILE and
          story_class.TAGS and
          story_tags.GPU_RASTERIZATION in story_class.TAGS):
        self.AddStory(story_class(
            page_set=self,
            shared_page_state_class=shared_page_state_class,
            name_suffix='_desktop_gpu_raster',
            extra_browser_args=['--force-gpu-rasterization']))

      if (platform == platforms.MOBILE and
          story_class.TAGS and
          story_tags.SYNC_SCROLL in story_class.TAGS):
        self.AddStory(story_class(
            page_set=self,
            shared_page_state_class=shared_page_state_class,
            name_suffix='_sync_scroll',
            extra_browser_args=['--disable-threaded-scrolling']))


class DesktopRenderingStorySet(RenderingStorySet):
  """Desktop stories related to rendering.

  Note: This story set is only intended to be used for recording stories via
  tools/perf/record_wpr. If you would like to use it in a benchmark, please use
  the generic RenderingStorySet class instead (you'll need to override the
  CreateStorySet method of your benchmark).
  """
  def __init__(self):
    super(DesktopRenderingStorySet, self).__init__(platform='desktop')


def _IterAllRenderingStoryClasses():
  start_dir = os.path.dirname(os.path.abspath(__file__))
  # Sort the classes by their names so that their order is stable and
  # deterministic.
  for _, cls in sorted(discover.DiscoverClasses(
      start_dir=start_dir,
      top_level_dir=os.path.dirname(start_dir),
      base_class=rendering_story.RenderingStory).iteritems()):
    yield cls
