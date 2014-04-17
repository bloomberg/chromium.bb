# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import webrtc
from telemetry import test


class WebRTC(test.Test):
  """Obtains WebRTC metrics for a real-time video tests."""
  test = webrtc.WebRTC
  page_set = 'page_sets/webrtc_cases.py'
