# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from benchmarks import v8_browsing
from contrib.cluster_telemetry import loading_base_ct
from telemetry.web_perf import timeline_based_measurement

# pylint: disable=protected-access
class V8LoadingClusterTelemetry(loading_base_ct._LoadingBaseClusterTelemetry):

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    super(V8LoadingClusterTelemetry, cls).AddBenchmarkCommandLineArgs(parser)
    parser.add_option('--enable-rcs', action='store_true', default=False,
        help='Enable expensive V8 Runtime Call Stats metrics.')

  @classmethod
  def ShouldAddValue(cls, name, from_first_story_run):
    del from_first_story_run  # unused
    return v8_browsing.V8BrowsingShouldAddValue(name)

  @classmethod
  def Name(cls):
    return 'v8.loading.cluster_telemetry'

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    v8_browsing.AugmentOptionsForV8BrowsingMetrics(options,
        enable_runtime_call_stats=options.enable_rcs)
    return options
