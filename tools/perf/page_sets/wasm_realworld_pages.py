# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry import story

from page_sets import webgl_supported_shared_state


class Tanks(page_module.Page):

  def __init__(self, page_set):
    url = 'http://webassembly.org/demo/Tanks/'
    super(Tanks, self).__init__(
        url=url,
        page_set=page_set,
        shared_page_state_class=(
            webgl_supported_shared_state.WebGLSupportedSharedState),
        name='WasmTanks')

  @property
  def skipped_gpus(self):
    # Unity WebGL is not supported on mobile
    return ['arm']

  def RunPageInteractions(self, action_runner):
    action_runner.WaitForJavaScriptCondition(
        """document.getElementsByClassName('progress Dark').length != 0""")
    action_runner.WaitForJavaScriptCondition(
        """document.getElementsByClassName('progress Dark')[0].style['display']
          == 'none'""")


class WasmRealWorldPagesStorySet(story.StorySet):
  """Top apps, used to monitor web assembly apps."""

  def __init__(self):
    super(WasmRealWorldPagesStorySet, self).__init__(
        archive_data_file='data/wasm_realworld_pages.json',
        cloud_storage_bucket=story.INTERNAL_BUCKET)

    self.AddStory(Tanks(self))
