# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state


class PregeneratedLargeProfileSharedState(shared_page_state.SharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(PregeneratedLargeProfileSharedState, self).__init__(
        test, finder_options, story_set)
    self.SetPregeneratedProfileArchive('large_profile.zip')
