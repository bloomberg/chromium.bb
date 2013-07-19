# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from measurements import startup_warm


class StartupWarmBlankPage(test.Test):
  test = startup_warm.StartupWarm
  page_set = 'page_sets/blank_page.json'
