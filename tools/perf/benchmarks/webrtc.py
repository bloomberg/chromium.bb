# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import webrtc
import page_sets
from telemetry import benchmark


class WebRTC(benchmark.Benchmark):
  """Obtains WebRTC metrics for a real-time video tests."""
  test = webrtc.WebRTC
  page_set = page_sets.WebrtcCasesPageSet
