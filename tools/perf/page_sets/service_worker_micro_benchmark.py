# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page
from telemetry.page import page_set as page_set


archive_data_file_path = 'data/service_worker_micro_benchmark.json'


class ServiceWorkerBenchmarkPage(page.Page):
  """Page for workload to measure some specific functions in JS"""

  def RunNavigateSteps(self, action_runner):
    super(ServiceWorkerBenchmarkPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition('window.done')


class ServiceWorkerMicroBenchmarkPageSet(page_set.PageSet):
  """Page set for micro benchmarking of each functions with ServiceWorker"""

  def __init__(self):
    super(ServiceWorkerMicroBenchmarkPageSet, self).__init__(
        archive_data_file=archive_data_file_path,
        bucket=page_set.PUBLIC_BUCKET)

    # pylint: disable=C0301
    # The code of localhost:8091 is placed in
    # https://github.com/coonsta/Service-Worker-Performance
    # but currently the following is used:
    # https://github.com/amiq11/Service-Worker-Performance/tree/follow_spec_and_many_registration
    # (rev: 3238098ea0225f53dab2f69f7406db8a2712dbf9)
    # This will be merged into the main repository.
    # pylint: enable=C0301
    # Why: to measure performance of many concurrent fetches
    self.AddUserStory(ServiceWorkerBenchmarkPage(
        'http://localhost:8091/index.html', self,
        make_javascript_deterministic=False))
