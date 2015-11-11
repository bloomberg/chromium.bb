# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import logging

import page_sets

from telemetry import decorators
from telemetry import page
from telemetry.testing import story_set_smoke_test

from catapult_base import cloud_storage


class StorySetUnitTest(story_set_smoke_test.StorySetSmokeTest):

  def setUp(self):
    self.story_sets_dir = os.path.dirname(os.path.realpath(__file__))
    self.top_level_dir = os.path.dirname(self.story_sets_dir)

  # TODO(tbarzic): crbug.com/386416.
  @decorators.Disabled('chromeos')
  def testSmoke(self):
    try:
      self.RunSmokeTest(self.story_sets_dir, self.top_level_dir)
    # Swallow cloud_storage.CredentialsError due to http://crbug.com/513086.
    # TODO(nednguyen): remove the try except block once that bug is fixed.
    except cloud_storage.CredentialsError:
      logging.warning(
          'CredentialsError was ignored for StorySetUnitTest.testSmoke')

  # TODO(nednguyen): Remove this test once crbug.com/508538 is fixed.
  # http://crbug.com/513086
  @decorators.Disabled('all')
  def testNoPageDefinedSyntheticDelay(self):
    for story_set_class in self.GetAllStorySetClasses(self.story_sets_dir,
                                                      self.top_level_dir):
      if story_set_class is page_sets.ToughSchedulingCasesPageSet:
        continue
      try:
        story_set = story_set_class()
        for story in story_set:
          if isinstance(story, page.Page):
            self.assertFalse(
              story.synthetic_delays,
              'Page %s in page set %s has non empty synthetic delay. '
              'Synthetic delay is no longer supported. See crbug.com/508538.' %
              (story.display_name, story_set.Name()))
      # Swallow cloud_storage.CredentialsError due to http://crbug.com/513086.
      # TODO(nednguyen): remove the try except block once that bug is fixed.
      except cloud_storage.CredentialsError:
        logging.warning('CredentialsError was ignored for '
                        'StorySetUnitTest.testNoPageDefinedSyntheticDelay')
