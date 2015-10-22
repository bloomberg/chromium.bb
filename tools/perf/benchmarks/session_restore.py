# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from measurements import session_restore
import page_sets
from telemetry import benchmark


class _SessionRestoreTypical25(perf_benchmark.PerfBenchmark):
  """Base Benchmark class for session restore benchmarks.

  A cold start means none of the Chromium files are in the disk cache.
  A warm start assumes the OS has already cached much of Chromium's content.
  For warm tests, you should repeat the page set to ensure it's cached.

  Use Typical25PageSet to match what the SmallProfileCreator uses.
  TODO(slamm): Make SmallProfileCreator and this use the same page_set ref.
  """
  page_set = page_sets.Typical25PageSetWithProfile
  tag = None  # override with 'warm' or 'cold'

  @classmethod
  def Name(cls):
    return 'session_restore'

  @classmethod
  def ValueCanBeAddedPredicate(cls, _, is_first_result):
    return cls.tag == 'cold' or not is_first_result

  def CreateStorySet(self, _):
    """Return a story set that only has the first story.

    The session restore measurement skips the navigation step and
    only tests session restore by having the browser start-up.
    The first story is used to get WPR set up and hold results.
    """
    story_set = self.page_set()
    for story in story_set.stories[1:]:
      story_set.RemoveStory(story)
    return story_set

  def CreatePageTest(self, options):
    is_cold = (self.tag == 'cold')
    return session_restore.SessionRestore(cold=is_cold)

@benchmark.Disabled('android',
                    'reference')  # crbug.com/539011
class SessionRestoreColdTypical25(_SessionRestoreTypical25):
  """Test by clearing system cache and profile before repeats."""
  tag = 'cold'
  options = {'pageset_repeat': 5}

  @classmethod
  def Name(cls):
    return 'session_restore.cold.typical_25'


@benchmark.Disabled('android',
                    'linux', 'xp')  # crbug.com/539056
class SessionRestoreWarmTypical25(_SessionRestoreTypical25):
  """Test without clearing system cache or profile before repeats.

  The first result is discarded.
  """
  tag = 'warm'
  options = {'pageset_repeat': 20}

  @classmethod
  def Name(cls):
    return 'session_restore.warm.typical_25'

