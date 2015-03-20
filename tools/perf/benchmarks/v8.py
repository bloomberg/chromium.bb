# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import page_sets
from measurements import v8_detached_context_age_in_gc
from measurements import v8_gc_times
from telemetry import benchmark


@benchmark.Disabled('win')  # crbug.com/416502
class V8GarbageCollectionCases(benchmark.Benchmark):
  """Measure V8 GC metrics on the garbage collection cases."""
  test = v8_gc_times.V8GCTimes
  page_set = page_sets.GarbageCollectionCasesPageSet

  @classmethod
  def Name(cls):
    return 'v8.garbage_collection_cases'

# Disabled on Win due to crbug.com/416502.
# TODO(rmcilroy): reenable on reference when crbug.com/456845 is fixed.
@benchmark.Disabled('win', 'reference')
class V8Top25(benchmark.Benchmark):
  """Measures V8 GC metrics on the while scrolling down the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = v8_gc_times.V8GCTimes
  page_set = page_sets.Top25SmoothPageSet

  @classmethod
  def Name(cls):
    return 'v8.top_25_smooth'

@benchmark.Enabled('android')
class V8KeyMobileSites(benchmark.Benchmark):
  """Measures V8 GC metrics on the while scrolling down key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = v8_gc_times.V8GCTimes
  page_set = page_sets.KeyMobileSitesSmoothPageSet

  @classmethod
  def Name(cls):
    return 'v8.key_mobile_sites_smooth'

@benchmark.Disabled  # http://crbug.com/469146
class V8DetachedContextAgeInGC(benchmark.Benchmark):
  """Measures the number of GCs needed to collect a detached context.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = v8_detached_context_age_in_gc.V8DetachedContextAgeInGC
  page_set = page_sets.PageReloadCasesPageSet

  @classmethod
  def Name(cls):
    return 'v8.detached_context_age_in_gc'
