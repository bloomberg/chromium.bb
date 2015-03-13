# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import test_expectations

class WebGLExpectations(test_expectations.TestExpectations):
  def SetExpectations(self):
    self.Skip('*', ['arm', 'broadcom', 'hisilicon', 'imagination', 'qualcomm',
                    'vivante'], bug=462729)
