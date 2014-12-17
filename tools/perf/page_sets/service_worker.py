# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page
from telemetry.page import page_set as page_set


archive_data_file_path = 'data/service_worker.json'


class ServiceWorkerPageSet(page_set.PageSet):
  """Page set of applications using ServiceWorker"""

  def __init__(self):
    super(ServiceWorkerPageSet, self).__init__(
        archive_data_file=archive_data_file_path,
        bucket=page_set.PARTNER_BUCKET)

    # Why: the first application using ServiceWorker
    # 1st time: registration
    self.AddUserStory(page.Page(
        'https://jakearchibald.github.io/trained-to-thrill/', self,
        make_javascript_deterministic=False))
    # 2st time: 1st onfetch with caching
    self.AddUserStory(page.Page(
        'https://jakearchibald.github.io/trained-to-thrill/', self,
        make_javascript_deterministic=False))
    # 3rd time: 2nd onfetch from cache
    self.AddUserStory(page.Page(
        'https://jakearchibald.github.io/trained-to-thrill/', self,
        make_javascript_deterministic=False))
