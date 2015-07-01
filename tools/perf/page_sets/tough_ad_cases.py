# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry import story


class SwiffyPage(page_module.Page):

  def __init__(self, url, page_set):
    super(SwiffyPage, self).__init__(url=url, page_set=page_set,
                                     make_javascript_deterministic=False)

  def RunNavigateSteps(self, action_runner):
    super(SwiffyPage, self).RunNavigateSteps(action_runner)
    # Swiffy overwrites toString() to return a constant string, so "undo" that
    # here so that we don't think it has stomped over console.time.
    action_runner.EvaluateJavaScript(
        'Function.prototype.toString = function() { return "[native code]"; }')
    # Make sure we have a reasonable viewport for mobile.
    viewport_js = (
        'var meta = document.createElement("meta");'
        'meta.name = "viewport";'
        'meta.content = "width=device-width";'
        'document.getElementsByTagName("head")[0].appendChild(meta);')
    action_runner.EvaluateJavaScript(viewport_js)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('ToughAd'):
      action_runner.Wait(10)


class ToughAdCasesPageSet(story.StorySet):
  """Pages for measuring performance with advertising content."""

  def __init__(self):
    super(ToughAdCasesPageSet, self).__init__(
        archive_data_file='data/tough_ad_cases.json',
        cloud_storage_bucket=story.INTERNAL_BUCKET)

    base_url = 'http://localhost:8000'

    # See go/swiffy-chrome-samples for how to add new pages here or how to
    # update the existing ones.
    swiffy_pages = [
        'CICAgICQ15a9NxDIARjIASgBMghBC1XuTk8ezw.swiffy72.html',
        'shapes-CK7ptO3F8bi2KxDQAhiYAigBMgij6QBQtD2gyA.swiffy72.html',
        'CNP2xe_LmqPEKBCsAhj6ASgBMggnyMqth81h8Q.swiffy72.html',
        'clip-paths-CICAgMDO7Ye9-gEQ2AUYWigBMgjZxDii6aoK9w.swiffy72.html',
        'filters-CNLa0t2T47qJ_wEQoAEY2AQoATIIFaIdc7VMBr4.swiffy72.html',
        'shapes-CICAgMDO7cfIzwEQ1AMYPCgBMghqY8tqyRCArQ.swiffy72.html',
        'CICAgIDQ2Pb-MxCsAhj6ASgBMgi5DLoSO0gPbQ.swiffy72.html',
        'CICAgKCN39CopQEQoAEY2AQoATIID59gK5hjjIg.swiffy72.html',
        'CICAgKCNj4HgyAEQeBjYBCgBMgjQpPkOjyWNdw.1.swiffy72.html',
        'clip-paths-CILZhLqO_-27bxB4GNgEKAEyCC46kMLBXnMT.swiffy72.html',
        'CICAgMDOrcnRGRB4GNgEKAEyCP_ZBSfwUFsj.swiffy72.html',
    ]
    for page_name in swiffy_pages:
      url = base_url + '/' + page_name
      self.AddUserStory(SwiffyPage(url, self))
