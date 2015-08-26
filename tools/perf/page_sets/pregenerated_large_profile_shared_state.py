# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry.page import shared_page_state


class PregeneratedLargeProfileSharedState(shared_page_state.SharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(PregeneratedLargeProfileSharedState, self).__init__(
        test, finder_options, story_set)
    perf_dir = os.path.normpath(
        os.path.join(os.path.dirname(__file__), os.path.pardir))
    profile_archive_dir = os.path.join(
        perf_dir, 'generated_profiles', self._possible_browser.target_os,
        'large_profile.zip')

    self.SetPregeneratedProfileArchiveDir(profile_archive_dir)
