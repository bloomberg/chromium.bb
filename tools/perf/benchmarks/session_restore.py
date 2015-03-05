# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile

from measurements import session_restore
import page_sets
from profile_creators import small_profile_creator
from telemetry import benchmark
from telemetry.page import profile_generator


class _SessionRestoreTypical25(benchmark.Benchmark):
  """Base Benchmark class for session restore benchmarks.

  A cold start means none of the Chromium files are in the disk cache.
  A warm start assumes the OS has already cached much of Chromium's content.
  For warm tests, you should repeat the page set to ensure it's cached.

  Use Typical25PageSet to match what the SmallProfileCreator uses.
  TODO(slamm): Make SmallProfileCreator and this use the same page_set ref.
  """
  page_set = page_sets.Typical25PageSet
  tag = None  # override with 'warm' or 'cold'

  @classmethod
  def Name(cls):
    return 'session_restore'

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    super(_SessionRestoreTypical25, cls).ProcessCommandLineArgs(parser, args)
    profile_type = 'small_profile'
    if not args.browser_options.profile_dir:
      output_dir = os.path.join(tempfile.gettempdir(), profile_type)
      profile_dir = os.path.join(output_dir, profile_type)
      if not os.path.exists(output_dir):
        os.makedirs(output_dir)

      # Generate new profiles if profile_dir does not exist. It only exists if
      # all profiles had been correctly generated in a previous run.
      if not os.path.exists(profile_dir):
        new_args = args.Copy()
        new_args.pageset_repeat = 1
        new_args.output_dir = output_dir
        profile_generator.GenerateProfiles(
            small_profile_creator.SmallProfileCreator, profile_type, new_args)
      args.browser_options.profile_dir = profile_dir

  @classmethod
  def ValueCanBeAddedPredicate(cls, _, is_first_result):
    return cls.tag == 'cold' or not is_first_result

  def CreateUserStorySet(self, _):
    """Return a user story set that only has the first user story.

    The session restore measurement skips the navigation step and
    only tests session restore by having the browser start-up.
    The first user story is used to get WPR set up and hold results.
    """
    user_story_set = self.page_set()
    for user_story in user_story_set.user_stories[1:]:
      user_story_set.RemoveUserStory(user_story)
    return user_story_set

  def CreatePageTest(self, options):
    is_cold = (self.tag == 'cold')
    return session_restore.SessionRestore(cold=is_cold)

  def CreatePageSet(self, options):
    return page_sets.Typical25PageSet(run_no_page_interactions=True)

# crbug.com/325479, crbug.com/381990
@benchmark.Disabled('android', 'linux', 'reference')
class SessionRestoreColdTypical25(_SessionRestoreTypical25):
  """Test by clearing system cache and profile before repeats."""
  tag = 'cold'
  options = {'pageset_repeat': 5}

  @classmethod
  def Name(cls):
    return 'session_restore.cold.typical_25'


# crbug.com/325479, crbug.com/381990
@benchmark.Disabled('android', 'linux', 'reference', 'xp')
class SessionRestoreWarmTypical25(_SessionRestoreTypical25):
  """Test without clearing system cache or profile before repeats.

  The first result is discarded.
  """
  tag = 'warm'
  options = {'pageset_repeat': 20}

  @classmethod
  def Name(cls):
    return 'session_restore.warm.typical_25'

