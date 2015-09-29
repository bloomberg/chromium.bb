# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets import repeatable_synthesize_scroll_gesture_shared_state

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

  def __init__(self, url, page_set, make_javascript_deterministic=True,
               y_scroll_distance_multiplier=0.5, bidirectional_scroll=False,
               wait_for_interactive_or_better=False):
    super(ScrollingPage, self).__init__(
        url=url,
        page_set=page_set,
        make_javascript_deterministic=make_javascript_deterministic,
        shared_page_state_class=(
            repeatable_synthesize_scroll_gesture_shared_state.\
                RepeatableSynthesizeScrollGestureSharedState))
    self._y_scroll_distance_multiplier = y_scroll_distance_multiplier
    self._bidirectional_scroll = bidirectional_scroll
    self._wait_for_interactive_or_better = wait_for_interactive_or_better

  def RunNavigateSteps(self, action_runner):
    # Rewrite file urls to point to the replay server instead.
    if self.is_file:
      url = self.file_path_url_with_scheme
      url = action_runner.tab.browser.platform.http_server.UrlOf(
          url[len('file://'):])
    else:
      url = self._url
    action_runner.tab.Navigate(url)

    # Wait for the page to be scrollable. Simultaneously (to reduce latency due
    # to main thread round trips),  insert a no-op touch handler on the body.
    # Most ads have touch handlers and we want to simulate the worst case of the
    # user trying to scroll the page by grabbing an ad.
    if self._wait_for_interactive_or_better:
        action_runner.WaitForJavaScriptCondition(
            '(document.readyState == "interactive" || '
            'document.readyState == "complete") &&'
            'document.body != null && '
            'document.body.scrollHeight > window.innerHeight && '
            '!document.body.addEventListener("touchstart", function() {})')
    else:
        action_runner.WaitForJavaScriptCondition(
            'document.body != null && '
            'document.body.scrollHeight > window.innerHeight && '
            '!document.body.addEventListener("touchstart", function() {})')

  def RunPageInteractions(self, action_runner):
    if self._bidirectional_scroll:
      action_runner.RepeatableBrowserDrivenScroll(
          y_scroll_distance_ratio=self._y_scroll_distance_multiplier,
          repeat_count=4)
      action_runner.RepeatableBrowserDrivenScroll(
          y_scroll_distance_ratio=-self._y_scroll_distance_multiplier * .5,
          repeat_count=4)
    else:
      action_runner.RepeatableBrowserDrivenScroll(
          y_scroll_distance_ratio=self._y_scroll_distance_multiplier,
          repeat_count=9)


class ScrollingForbesPage(ScrollingPage):

  def __init__(self, url, page_set, bidirectional_scroll=False):
    # forbes.com uses a strange dynamic transform on the body element,
    # which occasionally causes us to try scrolling from outside the
    # screen. Start at the very top of the viewport to avoid this.
    super(ScrollingForbesPage, self).__init__(
        url=url, page_set=page_set, make_javascript_deterministic=False,
        bidirectional_scroll=bidirectional_scroll,
        wait_for_interactive_or_better=True)

  def RunNavigateSteps(self, action_runner):
    super(ScrollingForbesPage, self).RunNavigateSteps(action_runner)
    # Wait until the interstitial banner goes away.
    action_runner.WaitForJavaScriptCondition(
        'window.location.pathname.indexOf("welcome") == -1')


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


class ToughWebglAdCasesPageSet(story.StorySet):
  """Pages for measuring rendering performance with WebGL ad content."""

  def __init__(self):
    super(ToughWebglAdCasesPageSet, self).__init__(
        archive_data_file='data/tough_ad_cases.json',
        cloud_storage_bucket=story.INTERNAL_BUCKET)

    base_url = 'http://localhost:8000'

    # See go/swiffy-chrome-samples for how to add new pages here or how to
    # update the existing ones.
    swiffy_pages = [
        'CICAgICQ15a9NxDIARjIASgBMghBC1XuTk8ezw.swf.webglbeta.html',
        'shapes-CK7ptO3F8bi2KxDQAhiYAigBMgij6QBQtD2gyA.swf.webglbeta.html',
        'CNP2xe_LmqPEKBCsAhj6ASgBMggnyMqth81h8Q.swf.webglbeta.html',
        'clip-paths-CICAgMDO7Ye9-gEQ2AUYWigBMgjZxDii6aoK9w.swf.webglbeta.html',
        'filters-CNLa0t2T47qJ_wEQoAEY2AQoATIIFaIdc7VMBr4.swf.webglbeta.html',
        'shapes-CICAgMDO7cfIzwEQ1AMYPCgBMghqY8tqyRCArQ.swf.webglbeta.html',
        'CICAgIDQ2Pb-MxCsAhj6ASgBMgi5DLoSO0gPbQ.swf.webglbeta.html',
        'CICAgKCN39CopQEQoAEY2AQoATIID59gK5hjjIg.swf.webglbeta.html',
        'CICAgKCNj4HgyAEQeBjYBCgBMgjQpPkOjyWNdw.1.swf.webglbeta.html',
        'clip-paths-CILZhLqO_-27bxB4GNgEKAEyCC46kMLBXnMT.swf.webglbeta.html',
        'CICAgMDOrcnRGRB4GNgEKAEyCP_ZBSfwUFsj.swf.webglbeta.html',
    ]
    for page_name in swiffy_pages:
      url = base_url + '/' + page_name
      self.AddStory(SwiffyPage(url, self))


class ScrollingToughAdCasesPageSet(story.StorySet):
  """Pages for measuring scrolling performance with advertising content."""

  def __init__(self, bidirectional_scroll=False):
    super(ScrollingToughAdCasesPageSet, self).__init__(
        archive_data_file='data/tough_ad_cases.json',
        cloud_storage_bucket=story.INTERNAL_BUCKET)

    self.AddStory(ScrollingPage('file://tough_ad_cases/'
        'swiffy_collection.html', self, make_javascript_deterministic=False,
        y_scroll_distance_multiplier=0.25,
        bidirectional_scroll=bidirectional_scroll))
    self.AddStory(ScrollingPage('file://tough_ad_cases/'
        'swiffy_webgl_collection.html',
        self, make_javascript_deterministic=False,
        bidirectional_scroll=bidirectional_scroll))
    self.AddStory(ScrollingPage('http://www.latimes.com', self,
        bidirectional_scroll=bidirectional_scroll,
        wait_for_interactive_or_better=True))
    self.AddStory(ScrollingForbesPage('http://www.forbes.com/sites/parmyolson/'
        '2015/07/29/jana-mobile-data-facebook-internet-org/', self,
        bidirectional_scroll=bidirectional_scroll))
    self.AddStory(ScrollingPage('http://androidcentral.com', self,
        bidirectional_scroll=bidirectional_scroll,
        wait_for_interactive_or_better=True))
    self.AddStory(ScrollingPage('http://mashable.com', self,
        y_scroll_distance_multiplier=0.25,
        bidirectional_scroll=bidirectional_scroll))
    self.AddStory(ScrollingPage('http://www.androidauthority.com/'
        'reduce-data-use-turn-on-data-compression-in-chrome-630064/', self,
        bidirectional_scroll=bidirectional_scroll))
    self.AddStory(ScrollingPage('http://www.cnn.com/2015/01/09/politics/'
        'nebraska-keystone-pipeline/index.html', self,
        bidirectional_scroll=bidirectional_scroll))
    # Disabled: crbug.com/520509
    #self.AddStory(ScrollingPage('http://time.com/3977891/'
    #    'donald-trump-debate-republican/', self,
    #    bidirectional_scroll=bidirectional_scroll))
    self.AddStory(ScrollingPage('http://www.theguardian.com/uk', self,
        bidirectional_scroll=bidirectional_scroll))
    self.AddStory(ScrollingPage('http://m.tmz.com', self,
        y_scroll_distance_multiplier=0.25,
        bidirectional_scroll=bidirectional_scroll))
    self.AddStory(ScrollingPage('http://androidpolice.com', self,
        bidirectional_scroll=bidirectional_scroll,
        wait_for_interactive_or_better=True))


class BidirectionallyScrollingToughAdCasesPageSet(ScrollingToughAdCasesPageSet):
  """Same as ScrollingAdCasesPageSet except we scroll in two directions."""

  def __init__(self):
    super(BidirectionallyScrollingToughAdCasesPageSet, self).__init__(
        bidirectional_scroll=True)
