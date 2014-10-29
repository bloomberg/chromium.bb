# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page
from telemetry.page import page_set as page_set


archive_data_file_path = 'data/service_worker.json'


class ServiceWorkerBenchmarkPage(page.Page):
  """Page for workload to measure some specific functions in JS"""

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition('window.done')


class ServiceWorkerPageSet(page_set.PageSet):
  """Page set which is using ServiceWorker"""

  def __init__(self):
    super(ServiceWorkerPageSet, self).__init__(
        archive_data_file=archive_data_file_path,
        make_javascript_deterministic=False,
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
    self.AddPage(ServiceWorkerBenchmarkPage(
        'http://localhost:8091/index.html', self))
    # Why: to measure performance of registrations
    self.AddPage(ServiceWorkerBenchmarkPage(
        'http://localhost:8091/many-registration.html', self))
    # Why: the first application using ServiceWorker
    # TODO(shimazu): Separate application tests from microbenchmark above two.
    # 1st time: registration
    self.AddPage(page.Page(
        'https://jakearchibald.github.io/trained-to-thrill/', self))
    # 2st time: 1st onfetch with caching
    self.AddPage(page.Page(
        'https://jakearchibald.github.io/trained-to-thrill/', self))
    # 3rd time: 2nd onfetch from cache
    self.AddPage(page.Page(
        'https://jakearchibald.github.io/trained-to-thrill/', self))
