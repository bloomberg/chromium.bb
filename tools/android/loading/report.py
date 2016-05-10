# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a loading report.

When executed as a script, takes a trace filename and print the report.
"""

import loading_trace
from user_satisfied_lens import (
    FirstTextPaintLens, FirstContentfulPaintLens, FirstSignificantPaintLens)


class LoadingReport(object):
  """Generates a loading report from a loading trace."""
  def __init__(self, trace):
    """Constructor.

    Args:
      trace: (LoadingTrace) a loading trace.
    """
    self.trace = trace
    self._text_msec = FirstTextPaintLens(self.trace).SatisfiedMs()
    self._contentful_paint_msec = (
        FirstContentfulPaintLens(self.trace).SatisfiedMs())
    self._significant_paint_msec = (
        FirstSignificantPaintLens(self.trace).SatisfiedMs())
    self._base_msec = min(
        r.start_msec for r in self.trace.request_track.GetEvents())
    # TODO(lizeb): This is not PLT. Should correlate with
    # RenderFrameImpl::didStopLoading.
    self._max_msec = max(
        r.end_msec or -1 for r in self.trace.request_track.GetEvents())

  def GenerateReport(self):
    """Returns a report as a dict."""
    return {
        'first_text_ms': self._text_msec - self._base_msec,
        'contentful_paint_ms': self._contentful_paint_msec - self._base_msec,
        'significant_paint_ms': self._significant_paint_msec - self._base_msec,
        'plt_ms': self._max_msec - self._base_msec}

  @classmethod
  def FromTraceFilename(cls, filename):
    """Returns a LoadingReport from a trace filename."""
    trace = loading_trace.LoadingTrace.FromJsonFile(filename)
    return LoadingReport(trace)


if __name__ == '__main__':
  import sys
  import json

  trace_filename = sys.argv[1]
  print json.dumps(
      LoadingReport.FromTraceFilename(trace_filename).GenerateReport(),
      indent=2)
