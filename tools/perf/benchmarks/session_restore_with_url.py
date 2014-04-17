# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from measurements import session_restore_with_url


class SessionRestoreWithUrlCold(test.Test):
  """Measure Chrome cold session restore with startup URLs."""
  tag = 'cold'
  test = session_restore_with_url.SessionRestoreWithUrl
  page_set = 'page_sets/startup_pages.py'
  options = {'cold': True,
             'pageset_repeat': 5}

class SessionRestoreWithUrlWarm(test.Test):
  """Measure Chrome warm session restore with startup URLs."""
  tag = 'warm'
  test = session_restore_with_url.SessionRestoreWithUrl
  page_set = 'page_sets/startup_pages.py'
  options = {'warm': True,
             'pageset_repeat': 10}

