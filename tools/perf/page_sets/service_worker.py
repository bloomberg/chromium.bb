# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page
from telemetry.page import page_set


archive_data_file_path = 'data/service_worker.json'


class ServiceWorkerPage(page.Page):
  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage()
    action_runner.WaitForJavaScriptCondition('window.done')


class ServiceWorkerPageSet(page_set.PageSet):
  """ServiceWorker performance tests"""

  def __init__(self):
    super(ServiceWorkerPageSet, self).__init__(
        archive_data_file=archive_data_file_path,
        make_javascript_deterministic=False,
        bucket=page_set.PUBLIC_BUCKET)

    self.AddPage(ServiceWorkerPage('http://localhost:8091/index.html', self))
