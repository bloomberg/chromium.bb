# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from telemetry import page
from contrib.vr_benchmarks import (shared_android_vr_page_state as
                                   vr_state)

SAMPLE_DIR = os.path.join(
    os.path.dirname(__file__), '..', '..', '..', '..', 'chrome', 'test',
    'data', 'vr', 'webvr_info', 'samples')


class VrSamplePage(page.Page):
  """Superclass for all VR sample pages."""

  def __init__(self, sample_page, page_set, url_parameters=None,
      extra_browser_args=None):
    url = '%s.html' % sample_page
    if url_parameters is not None:
      url += '?' + '&'.join(url_parameters)
    name = url.replace('.html', '')
    url = 'file://' + os.path.join(SAMPLE_DIR, url)
    super(VrSamplePage, self).__init__(
        url=url,
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=vr_state.SharedAndroidVrPageState)
    self._shared_page_state = None

  def Run(self, shared_state):
    self._shared_page_state = shared_state
    super(VrSamplePage, self).Run(shared_state)

  @property
  def platform(self):
    return self._shared_page_state.platform
