# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import test
from telemetry.page import page_measurement

from measurements import media


class MSEMeasurement(page_measurement.PageMeasurement):
  def MeasurePage(self, page, tab, results):
    media_metric = tab.EvaluateJavaScript('window.__testMetrics')
    trace = media_metric['id'] if 'id' in media_metric else None
    metrics = media_metric['metrics'] if 'metrics' in media_metric else []
    for m in metrics:
      if isinstance(metrics[m], list):
        values = [float(v) for v in metrics[m]]
      else:
        values = float(metrics[m])
      results.Add(trace, 'ms', values, chart_name=m)


class Media(test.Test):
  """Obtains media metrics for key user scenarios."""
  test = media.Media
  page_set = 'page_sets/tough_video_cases.json'

class MediaNetworkSimulation(test.Test):
  """Obtains media metrics under different network simulations."""
  test = media.Media
  page_set = 'page_sets/media_cns_cases.json'

class MediaAndroid(test.Test):
  """Obtains media metrics for key user scenarios on Android."""
  test = media.Media
  tag = 'android'
  page_set = 'page_sets/tough_video_cases.json'
  # Exclude crowd* media files (50fps 2160p).
  options = {
      'page_filter_exclude': '.*crowd.*'
  }

class MediaChromeOS(test.Test):
  """Obtains media metrics for key user scenarios on ChromeOS."""
  test = media.Media
  tag = 'chromeOS'
  page_set = 'page_sets/tough_video_cases.json'
  # Exclude crowd* media files (50fps 2160p): crbug/331816
  options = {
      'page_filter_exclude': '.*crowd.*'
  }

class MediaSourceExtensions(test.Test):
  """Obtains media metrics for key media source extensions functions."""
  test = MSEMeasurement
  page_set = 'page_sets/mse_cases.json'

  def CustomizeBrowserOptions(self, options):
    # Needed to allow XHR requests to return stream objects.
    options.AppendExtraBrowserArgs(
        ['--enable-experimental-web-platform-features',
         '--disable-gesture-requirement-for-media-playback'])
