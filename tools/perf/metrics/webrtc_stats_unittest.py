# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from metrics import webrtc_stats
from telemetry.unittest_util import simple_mock


SAMPLE_JSON = '''
[[
   [
      {
         "googFrameHeightInput":"480",
         "googFrameWidthInput":"640",
         "googFrameRateSent": "23",
         "packetsLost":"-1",
         "googRtt":"-1",
         "packetsSent":"1",
         "bytesSent":"0"
      },
      {
         "audioInputLevel":"2048",
         "googRtt":"-1",
         "googCodecName":"opus",
         "packetsSent":"4",
         "bytesSent":"0"
      }
   ],
   [
      {
         "googFrameHeightInput":"480",
         "googFrameWidthInput":"640",
         "googFrameRateSent": "21",
         "packetsLost":"-1",
         "googRtt":"-1",
         "packetsSent":"8",
         "bytesSent":"6291"
      },
      {
         "audioInputLevel":"1878",
         "googRtt":"-1",
         "googCodecName":"opus",
         "packetsSent":"16",
         "bytesSent":"634"
      }
   ]
],
[
   [
      {
         "googFrameRateReceived": "23",
         "googDecodeMs":"0",
         "packetsReceived":"8",
         "googRenderDelayMs":"10",
         "googMaxDecodeMs":"0"
      }
   ],
   [
      {
         "googFrameRateReceived": "23",
         "googDecodeMs":"14",
         "packetsReceived":"1234",
         "googRenderDelayMs":"102",
         "googMaxDecodeMs":"150"
      }
   ]
]]
'''


class FakeResults:
  def __init__(self, current_page):
    self._received_values = []
    self._current_page = current_page

  @property
  def received_values(self):
    return self._received_values

  @property
  def current_page(self):
    return self._current_page

  def AddValue(self, value):
    self._received_values.append(value)


class WebRtcStatsUnittest(unittest.TestCase):

  def _RunMetricOnJson(self, json_to_return):
    stats_metric = webrtc_stats.WebRtcStatisticsMetric()

    tab = simple_mock.MockObject()
    page = simple_mock.MockObject()

    stats_metric.Start(page, tab)

    tab.ExpectCall('EvaluateJavaScript',
                   simple_mock.DONT_CARE).WillReturn(json_to_return)
    stats_metric.Stop(page, tab)

    page.url = simple_mock.MockObject()
    results = FakeResults(page)
    stats_metric.AddResults(tab, results)
    return results

  def testExtractsValuesAsTimeSeries(self):
    results = self._RunMetricOnJson(SAMPLE_JSON)

    self.assertTrue(results.received_values,
                    'Expected values for googDecodeMs and others, got none.')

    # This also ensures we're clever enough to tell video packetsSent from audio
    # packetsSent.
    self.assertEqual(results.received_values[0].values,
                     [4.0, 16.0])
    self.assertEqual(results.received_values[1].values,
                     [1.0, 8.0])

  def testExtractsInterestingMetricsOnly(self):
    results = self._RunMetricOnJson(SAMPLE_JSON)

    self.assertEqual(len(results.received_values), 5)
    self.assertEqual(results.received_values[0].name,
                     'peer_connection_0_audio_packets_sent',
                     'The result should be a ListOfScalarValues instance with '
                     'a name <peer connection id>_<statistic>.')
    self.assertEqual(results.received_values[1].name,
                     'peer_connection_0_video_packets_sent')
    self.assertEqual(results.received_values[2].name,
                     'peer_connection_1_video_goog_max_decode_ms')
    self.assertEqual(results.received_values[3].name,
                     'peer_connection_1_video_packets_received')
    self.assertEqual(results.received_values[4].name,
                     'peer_connection_1_video_goog_decode_ms')

  def testReturnsIfJsonIsEmpty(self):
    results = self._RunMetricOnJson('[]')
    self.assertFalse(results.received_values)

