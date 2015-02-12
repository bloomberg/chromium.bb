# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import task_execution_time
import page_sets
from telemetry import benchmark


@benchmark.Enabled('android')
class TaskExecutionTimeKeyMobileSites(benchmark.Benchmark):

  """Measures task execution statistics while scrolling down key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks
  """

  test = task_execution_time.TaskExecutionTime
  page_set = page_sets.KeyMobileSitesSmoothPageSet

  @classmethod
  def Name(cls):
    return 'task_execution_time.key_mobile_sites_smooth'


@benchmark.Enabled('android')
class TaskExecutionTimeToughSchedulingCases(benchmark.Benchmark):

  """Measures task execution statistics while scrolling tough scheduling sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks.
  """

  test = task_execution_time.TaskExecutionTime
  page_set = page_sets.ToughSchedulingCasesPageSet

  @classmethod
  def Name(cls):
    return 'task_execution_time.tough_scheduling_cases'


@benchmark.Enabled('android')
class TaskExecutionTimePathologicalMobileSites(benchmark.Benchmark):

  """Measures task execution statistics while scrolling pathological sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks.
  """

  test = task_execution_time.TaskExecutionTime
  page_set = page_sets.PathologicalMobileSitesPageSet

  @classmethod
  def Name(cls):
    return 'task_execution_time.pathological_mobile_sites'
