# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Media Metrics class injects and calls JS responsible for recording metrics.

Default media metrics are collected for every media element in the page, such as
decoded_frame_count, dropped_frame_count, decoded_video_bytes, and
decoded_audio_bytes.
"""

import logging
import os


class MediaMetrics(object):
  def __init__(self, tab):
    with open(
        os.path.join(os.path.dirname(__file__), 'media_metrics.js')) as f:
      js = f.read()
      tab.ExecuteJavaScript(js)
    self.tab = tab

  def Start(self):
    """Create the media metrics for all media elements in the document."""
    self.tab.ExecuteJavaScript('window.__createMediaMetricsForDocument()')

  def StopAndGetResults(self, results):
    """Reports all recorded metrics as Telemetry perf results."""
    media_metrics = self.tab.EvaluateJavaScript('window.__getAllMetrics()')
    for media_metric in media_metrics:
      self.AddResultForMediaElement(media_metric, results)

  def AddResultForMediaElement(self, media_metric, results):
    """Reports metrics for one media element.

    Media metrics contain an ID identifying the media element and values:
    media_metric = {
      'id': 'video_1',
      'metrics': {
          'time_to_play': 120,
          'decoded_bytes': 13233,
          ...
      }
    }
    """
    def AddResult(metric, unit):
      metrics = media_metric['metrics']
      if metric in metrics:
        results.Add(trace, unit, str(metrics[metric]), chart_name=metric,
                    data_type='default')

    trace = media_metric['id']
    if not trace:
      logging.error('Metrics ID is missing in results.')
      return

    AddResult('time_to_play', 'sec')
    AddResult('playback_time', 'sec')
    AddResult('decoded_audio_bytes', 'bytes')
    AddResult('decoded_video_bytes', 'bytes')
    AddResult('decoded_frame_count', 'frames')
    AddResult('dropped_frame_count', 'frames')
