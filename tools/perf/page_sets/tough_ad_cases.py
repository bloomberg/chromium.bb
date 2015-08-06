# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core import exceptions
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


class ScrollingPage(page_module.Page):

  def __init__(self, url, page_set, top_start_ratio=.5,
               make_javascript_deterministic=True):
    super(ScrollingPage, self).__init__(url=url, page_set=page_set,
        make_javascript_deterministic=make_javascript_deterministic)
    self._top_start_ratio = top_start_ratio

  def RunNavigateSteps(self, action_runner):
    # Rewrite file urls to point to the replay server instead.
    if self.is_file:
      url = self.file_path_url_with_scheme
      url = action_runner.tab.browser.http_server.UrlOf(url[len('file://'):])
    else:
      url = self._url
    action_runner.tab.Navigate(url)

    # Give the page one second to become interactive and start scrolling after
    # the timeout regardless of the document's ready state.
    try:
      action_runner.tab.WaitForDocumentReadyStateToBeInteractiveOrBetter(1)
    except exceptions.TimeoutException:
      pass
    # Make sure we have a body element to scroll.
    action_runner.WaitForJavaScriptCondition('document.body !== null')

  def RunPageInteractions(self, action_runner):
    for _ in range(10):
      with action_runner.CreateGestureInteraction('ScrollAction',
                                                  repeatable=True):
        action_runner.ScrollPage(distance=500,
                                 top_start_ratio=self._top_start_ratio)
      action_runner.Wait(.25)


class ScrollingForbesPage(ScrollingPage):

  def __init__(self, url, page_set):
    # forbes.com uses a strange dynamic transform on the body element,
    # which occasionally causes us to try scrolling from outside the
    # screen. Start at the very top of the viewport to avoid this.
    super(ScrollingForbesPage, self).__init__(
        url=url, page_set=page_set, top_start_ratio=0,
        make_javascript_deterministic=False)

  def RunNavigateSteps(self, action_runner):
    super(ScrollingForbesPage, self).RunNavigateSteps(action_runner)
    # Wait until the interstitial banner goes away.
    action_runner.WaitForJavaScriptCondition(
        'window.location.pathname.indexOf("welcome") == -1')
    # Make sure we have a body element to scroll.
    action_runner.WaitForJavaScriptCondition('document.body !== null')


class ToughAdCasesPageSet(story.StorySet):
  """Pages for measuring rendering performance with advertising content."""

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
      self.AddStory(SwiffyPage(url, self))


class ScrollingToughAdCasesPageSet(story.StorySet):
  """Pages for measuring scrolling performance with advertising content."""

  def __init__(self):
    super(ScrollingToughAdCasesPageSet, self).__init__(
        archive_data_file='data/tough_ad_cases.json',
        cloud_storage_bucket=story.INTERNAL_BUCKET)

    self.AddStory(ScrollingPage('file://tough_ad_cases/'
        'swiffy_collection.html', self, make_javascript_deterministic=False))
    self.AddStory(ScrollingPage('http://www.latimes.com', self))
    self.AddStory(ScrollingForbesPage('http://www.forbes.com/sites/parmyolson/'
        '2015/07/29/jana-mobile-data-facebook-internet-org/', self))
    self.AddStory(ScrollingPage('http://androidcentral.com', self))
    self.AddStory(ScrollingPage('http://mashable.com', self, top_start_ratio=0))
    self.AddStory(ScrollingPage('http://www.androidauthority.com/'
        'reduce-data-use-turn-on-data-compression-in-chrome-630064/', self))
    self.AddStory(ScrollingPage('http://www.cnn.com/2015/01/09/politics/'
        'nebraska-keystone-pipeline/index.html', self, top_start_ratio=0))
    self.AddStory(ScrollingPage('http://time.com/3977891/'
        'donald-trump-debate-republican/', self))
    self.AddStory(ScrollingPage('http://www.theguardian.com/uk', self))
    self.AddStory(ScrollingPage('http://m.tmz.com', self))
    self.AddStory(ScrollingPage('http://androidpolice.com', self,
        top_start_ratio=0))
