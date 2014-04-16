# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from measurements import startup


@test.Disabled('snowleopard') # crbug.com/336913
class StartWithUrlCold(test.Test):
  """Measure time to start Chrome cold with startup URLs"""
  tag = 'cold'
  test = startup.StartWithUrl
  page_set = 'page_sets/startup_pages.py'
  options = {'cold': True,
             'pageset_repeat': 5}

class StartWithUrlWarm(test.Test):
  """Measure time to start Chrome warm with startup URLs"""
  tag = 'warm'
  test = startup.StartWithUrl
  page_set = 'page_sets/startup_pages.py'
  options = {'warm': True,
             'pageset_repeat': 10}

