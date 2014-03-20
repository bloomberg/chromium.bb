# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import session_restore


class SessionRestoreWithUrl(session_restore.SessionRestore):

  def __init__(self):
    super(SessionRestoreWithUrl, self).__init__(
        action_name_to_run='RunNavigateSteps')

  def CanRunForPage(self, page):
    # Run for every page in the page set that has a startup url.
    return bool(page.startup_url)
