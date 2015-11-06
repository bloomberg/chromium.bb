# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets import webgl_supported_shared_state
from telemetry import page as page_module
from telemetry import story

STARTUP_SCRIPT = '''
    window.benchmarkStarted =false;
    window.benchmarkFinished = false;
    window.benchmarkBeforeRun = function() {
      window.benchmarkStarted = true;
    };
    window.benchmarkAfterRun = function(score) {
      window.benchmarkFinished = true;
      window.benchmarkScore = score;
      window.benchmarkAfterRun = null;
    };'''

class OortOnlinePage(page_module.Page):
  def __init__(self, page_set):
    super(OortOnlinePage, self).__init__(
        url='http://oortonline.gl/#run', page_set=page_set,
        shared_page_state_class=(
            webgl_supported_shared_state.WebGLSupportedSharedState),
        make_javascript_deterministic=False)
    self.archive_data_file = 'data/oortonline.json'
    self.script_to_evaluate_on_commit = STARTUP_SCRIPT

  @property
  def skipped_gpus(self):
    # crbug.com/462729
    return ['arm', 'broadcom', 'hisilicon', 'imagination', 'qualcomm',
            'vivante', 'vmware']

class OortOnlinePageSet(story.StorySet):
  """Oort Online WebGL benchmark.
  URL: http://oortonline.gl/#run
  Info: http://v8project.blogspot.de/2015/10/jank-busters-part-one.html
  """

  def __init__(self):
    super(OortOnlinePageSet, self).__init__(
      archive_data_file='data/oortonline.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)
    self.AddStory(OortOnlinePage(self))
