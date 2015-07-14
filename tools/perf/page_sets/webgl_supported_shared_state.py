# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from telemetry.page import shared_page_state


class WebGLSupportedSharedState(shared_page_state.SharedPageState):
  def CanRunOnBrowser(self, browser_info):
    if not browser_info.HasWebGLSupport():
      logging.warning('Browser does not support webgl, skipping test')
      return False
    return True
