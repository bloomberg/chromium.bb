# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
class ScrollResults(object):
  """Container for ScrollTest results."""

  def __init__(self, url, first_paint_seconds, results_list):
    self._url = url
    self._first_paint_time = first_paint_seconds
    self._results_list = results_list

  def DidScroll(self):
    for index in xrange(len(self._results_list)):
      if self.GetFrameCount(index) > 0:
        return True
    return False

  def GetFirstPaintTime(self):
    return self._first_paint_time

  def GetFps(self, index):
    return (self.GetFrameCount(index) /
            self.GetResult(index)['totalTimeInSeconds'])

  def GetFrameCount(self, index):
    results = self.GetResult(index)
    return results.get('numFramesSentToScreen', results['numAnimationFrames'])

  def GetMeanFrameTime(self, index):
    return (self.GetResult(index)['totalTimeInSeconds'] /
            self.GetFrameCount(index))

  def GetPercentBelow60Fps(self, index):
    return (float(self.GetResult(index)['droppedFrameCount']) /
            self.GetFrameCount(index))

  def GetResult(self, index):
    return self._results_list[index]

  def GetUrl(self):
    return self._url
