# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.system_health import platforms
from page_sets.system_health import loading_stories

from telemetry import story


class _SystemHealthStorySet(story.StorySet):
  """User stories for the System Health Plan.

  See https://goo.gl/Jek2NL.
  """
  PLATFORM = NotImplemented

  def __init__(self, take_memory_measurement=True):
    super(_SystemHealthStorySet, self).__init__(
        archive_data_file=('../data/memory_system_health_%s.json' %
                           self.PLATFORM),
        cloud_storage_bucket=story.PARTNER_BUCKET)
    for story_class in loading_stories.IterAllStoryClasses():
      if self.PLATFORM not in story_class.SUPPORTED_PLATFORMS:
        continue
      self.AddStory(story_class(self, take_memory_measurement))


class DesktopSystemHealthStorySet(_SystemHealthStorySet):
  """Desktop user stories for Chrome System Health Plan."""
  PLATFORM = platforms.DESKTOP


class MobileSystemHealthStorySet(_SystemHealthStorySet):
  """Mobile user stories for Chrome System Health Plan."""
  PLATFORM = platforms.MOBILE
