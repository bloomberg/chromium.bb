# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import media
import page_sets
from telemetry import benchmark
from telemetry.page import page_measurement
from telemetry.value import list_of_scalar_values
from telemetry.value import scalar


class _MSEMeasurement(page_measurement.PageMeasurement):
  def MeasurePage(self, page, tab, results):
    media_metric = tab.EvaluateJavaScript('window.__testMetrics')
    trace = media_metric['id'] if 'id' in media_metric else None
    metrics = media_metric['metrics'] if 'metrics' in media_metric else []
    for m in metrics:
      trace_name = '%s.%s' % (m, trace)
      if isinstance(metrics[m], list):
        results.AddValue(list_of_scalar_values.ListOfScalarValues(
                results.current_page, trace_name, units='ms',
                values=[float(v) for v in metrics[m]],
                important=True))

      else:
        results.AddValue(scalar.ScalarValue(
                results.current_page, trace_name, units='ms',
                value=float(metrics[m]), important=True))


@benchmark.Disabled('android')
class Media(benchmark.Benchmark):
  """Obtains media metrics for key user scenarios."""
  test = media.Media
  page_set = page_sets.ToughVideoCasesPageSet


@benchmark.Disabled
class MediaNetworkSimulation(benchmark.Benchmark):
  """Obtains media metrics under different network simulations."""
  test = media.Media
  page_set = page_sets.MediaCnsCasesPageSet


@benchmark.Enabled('android')
@benchmark.Disabled('l')
class MediaAndroid(benchmark.Benchmark):
  """Obtains media metrics for key user scenarios on Android."""
  test = media.Media
  tag = 'android'
  page_set = page_sets.ToughVideoCasesPageSet
  # Exclude is_4k and 50 fps media files (garden* & crowd*).
  options = {'page_label_filter_exclude': 'is_4k,is_50fps'}


@benchmark.Enabled('chromeos')
class MediaChromeOS4kOnly(benchmark.Benchmark):
  """Benchmark for media performance on ChromeOS using only is_4k test content.
  """
  test = media.Media
  tag = 'chromeOS4kOnly'
  page_set = page_sets.ToughVideoCasesPageSet
  options = {
      'page_label_filter': 'is_4k',
      # Exclude is_50fps test files: crbug/331816
      'page_label_filter_exclude': 'is_50fps'
  }


@benchmark.Enabled('chromeos')
class MediaChromeOS(benchmark.Benchmark):
  """Benchmark for media performance on all ChromeOS platforms.

  This benchmark does not run is_4k content, there's a separate benchmark for
  that.
  """
  test = media.Media
  tag = 'chromeOS'
  page_set = page_sets.ToughVideoCasesPageSet
  # Exclude is_50fps test files: crbug/331816
  options = {'page_label_filter_exclude': 'is_4k,is_50fps'}


class MediaSourceExtensions(benchmark.Benchmark):
  """Obtains media metrics for key media source extensions functions."""
  test = _MSEMeasurement
  page_set = page_sets.MseCasesPageSet

  def CustomizeBrowserOptions(self, options):
    # Needed to allow XHR requests to return stream objects.
    options.AppendExtraBrowserArgs(
        ['--enable-experimental-web-platform-features',
         '--disable-gesture-requirement-for-media-playback'])
