# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from tracing.metrics import metric_runner

def AddTBMv2RenderingMetrics(trace_value, results, import_experimental_metrics):
  mre_result = metric_runner.RunMetric(
      trace_value.filename, metrics=['renderingMetric'],
      extra_import_options={'trackDetailedModelStats': True},
      report_progress=False, canonical_url=results.telemetry_info.trace_url)

  for f in mre_result.failures:
    results.Fail(f.stack)

  histograms = []
  for histogram in mre_result.pairs.get('histograms', []):
    if (import_experimental_metrics or
        histogram.get('name', '').find('_tbmv2') < 0):
      histograms.append(histogram)
  results.ImportHistogramDicts(histograms, import_immediately=False)
