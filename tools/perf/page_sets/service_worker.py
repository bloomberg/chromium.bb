# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page
from telemetry.page import page_set
from telemetry.page.actions import all_page_actions as actions


archive_data_file_path = 'data/service_worker.json'


class ServiceWorkerPage(page.Page):
  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(actions.NavigateAction())
    action_runner.RunAction(actions.WaitAction({'javascript': 'window.done'}))


class ServiceWorkerPageSet(page_set.PageSet):
  def __init__(self):
    super(ServiceWorkerPageSet, self).__init__(
        description='ServiceWorker performance tests',
        archive_data_file=archive_data_file_path,
        make_javascript_deterministic=False)

    self.AddPage(ServiceWorkerPage('http://localhost:8091/index.html', self))
