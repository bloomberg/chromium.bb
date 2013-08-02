# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from metrics import smoothness
from telemetry.page import page
from telemetry.page.page_measurement_results import PageMeasurementResults

class SmoothnessMetricsUnitTest(unittest.TestCase):

  def testCalcResultsFromRAFRenderStats(self):
    rendering_stats = {'droppedFrameCount': 5,
                       'totalTimeInSeconds': 1,
                       'numAnimationFrames': 10,
                       'numFramesSentToScreen': 10}
    res = PageMeasurementResults()
    res.WillMeasurePage(page.Page('http://foo.com/', None))
    smoothness.CalcScrollResults(rendering_stats, res)
    res.DidMeasurePage()
    self.assertEquals(50, res.page_results[0]['dropped_percent'].value)
    self.assertAlmostEquals(
      100,
      res.page_results[0]['mean_frame_time'].value, 2)

  def testCalcResultsRealRenderStats(self):
    rendering_stats = {'numFramesSentToScreen': 60,
                       'globalTotalTextureUploadTimeInSeconds': 0,
                       'totalProcessingCommandsTimeInSeconds': 0,
                       'globalTextureUploadCount': 0,
                       'droppedFrameCount': 0,
                       'textureUploadCount': 0,
                       'numAnimationFrames': 10,
                       'totalPaintTimeInSeconds': 0.35374299999999986,
                       'globalTotalProcessingCommandsTimeInSeconds': 0,
                       'totalTextureUploadTimeInSeconds': 0,
                       'totalRasterizeTimeInSeconds': 0,
                       'totalTimeInSeconds': 1.0}
    res = PageMeasurementResults()
    res.WillMeasurePage(page.Page('http://foo.com/', None))
    smoothness.CalcScrollResults(rendering_stats, res)
    res.DidMeasurePage()
    self.assertEquals(0, res.page_results[0]['dropped_percent'].value)
    self.assertAlmostEquals(
      1000 / 60.,
      res.page_results[0]['mean_frame_time'].value, 2)
