# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from page_sets.system_health import platforms

from telemetry.core import discover
from telemetry.page import page


_DUMP_WAIT_TIME = 3


class SystemHealthStory(page.Page):
  """Abstract base class for System Health user stories."""

  # The full name of a single page story has the form CASE:GROUP:PAGE (e.g.
  # 'load:search:google').
  NAME = NotImplemented
  URL = NotImplemented
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS

  def __init__(self, story_set, take_memory_measurement):
    case, group, _ = self.NAME.split(':')
    super(SystemHealthStory, self).__init__(
        page_set=story_set, name=self.NAME, url=self.URL,
        credentials_path='../data/credentials.json',
        grouping_keys={'case': case, 'group': group})
    self._take_memory_measurement = take_memory_measurement

  def _Measure(self, action_runner):
    if not self._take_memory_measurement:
      return
    # TODO(petrcermak): This method is essentially the same as
    # MemoryHealthPage._TakeMemoryMeasurement() in memory_health_story.py.
    # Consider sharing the common code.
    action_runner.Wait(_DUMP_WAIT_TIME)
    action_runner.ForceGarbageCollection()
    action_runner.Wait(_DUMP_WAIT_TIME)
    tracing_controller = action_runner.tab.browser.platform.tracing_controller
    if not tracing_controller.is_tracing_running:
      return  # Tracing is not running, e.g., when recording a WPR archive.
    if not action_runner.tab.browser.DumpMemory():
      logging.error('Unable to get a memory dump for %s.', self.name)

  def _Login(self, action_runner):
    pass

  def _DidLoadDocument(self, action_runner):
    pass

  def RunNavigateSteps(self, action_runner):
    self._Login(action_runner)
    super(SystemHealthStory, self).RunNavigateSteps(action_runner)

  def RunPageInteractions(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    self._DidLoadDocument(action_runner)
    self._Measure(action_runner)


def IterAllStoryClasses(module, base_class):
  # Sort the classes by their names so that their order is stable and
  # deterministic.
  for unused_cls_name, cls in sorted(discover.DiscoverClassesInModule(
      module=module,
      base_class=base_class,
      index_by_class_name=True).iteritems()):
    yield cls
