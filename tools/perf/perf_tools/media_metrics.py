# Copyright (c) 2013 The Chromium Authors. All rights reserved.
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
      self.AddResultsForMediaElement(media_metric, results)

  def AddResultsForMediaElement(self, media_metric, results):
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
    def AddResults(metric, unit):
      metrics = media_metric['metrics']
      for m in metrics:
        if m.startswith(metric):
          special_label = m[len(metric):]
          results.Add(trace + special_label, unit, str(metrics[m]),
                      chart_name=metric, data_type='default')

    trace = media_metric['id']
    if not trace:
      logging.error('Metrics ID is missing in results.')
      return
    AddResults('decoded_audio_bytes', 'bytes')
    AddResults('decoded_video_bytes', 'bytes')
    AddResults('decoded_frame_count', 'frames')
    AddResults('dropped_frame_count', 'frames')
    AddResults('playback_time', 'sec')
    AddResults('seek', 'sec')
    AddResults('time_to_play', 'sec')
