# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import unittest

from content_classification_lens import (ContentClassificationLens,
                                         _RulesMatcher)
from request_track import (Request, TimingFromDict)
import test_utils


class ContentClassificationLensTestCase(unittest.TestCase):
  _REQUEST = Request.FromJsonDict({'url': 'http://bla.com',
                                   'request_id': '1234.1',
                                   'frame_id': '123.1',
                                   'initiator': {'type': 'other'},
                                   'timestamp': 2,
                                   'timing': TimingFromDict({})})
  _MAIN_FRAME_ID = '123.1'
  _PAGE_EVENTS = [{'method': 'Page.frameStartedLoading',
                   'frame_id': _MAIN_FRAME_ID},
                  {'method': 'Page.frameAttached',
                   'frame_id': '123.13', 'parent_frame_id': _MAIN_FRAME_ID}]
  _RULES = ['bla.com']

  def testNoRules(self):
    trace = test_utils.LoadingTraceFromEvents(
        [self._REQUEST], self._PAGE_EVENTS)
    lens = ContentClassificationLens(trace, [], [])
    self.assertFalse(lens.IsAdRequest(self._REQUEST))
    self.assertFalse(lens.IsTrackingRequest(self._REQUEST))

  def testAdRequest(self):
    trace = test_utils.LoadingTraceFromEvents(
        [self._REQUEST], self._PAGE_EVENTS)
    lens = ContentClassificationLens(trace, self._RULES, [])
    self.assertTrue(lens.IsAdRequest(self._REQUEST))
    self.assertFalse(lens.IsTrackingRequest(self._REQUEST))

  def testTrackingRequest(self):
    trace = test_utils.LoadingTraceFromEvents(
        [self._REQUEST], self._PAGE_EVENTS)
    lens = ContentClassificationLens(trace, [], self._RULES)
    self.assertFalse(lens.IsAdRequest(self._REQUEST))
    self.assertTrue(lens.IsTrackingRequest(self._REQUEST))

  def testMainFrameIsNotAdFrame(self):
    trace = test_utils.LoadingTraceFromEvents(
        [self._REQUEST] * 10, self._PAGE_EVENTS)
    lens = ContentClassificationLens(trace, self._RULES, [])
    self.assertFalse(lens.IsAdFrame(self._MAIN_FRAME_ID, .5))

  def testAdFrame(self):
    request = self._REQUEST
    request.frame_id = '123.123'
    trace = test_utils.LoadingTraceFromEvents(
        [request] * 10 + [self._REQUEST] * 5, self._PAGE_EVENTS)
    lens = ContentClassificationLens(trace, self._RULES, [])
    self.assertTrue(lens.IsAdFrame(request.frame_id, .5))


class _MatcherTestCase(unittest.TestCase):
  _RULES_WITH_WHITELIST = ['/thisisanad.', '@@myadvertisingdomain.com/*',
                           '@@||www.mydomain.com/ads/$elemhide']
  _SCRIPT_RULE = 'domainwithscripts.com/*$script'
  _SCRIPT_REQUEST = Request.FromJsonDict(
      {'url': 'http://domainwithscripts.com/bla.js',
       'resource_type': 'Script',
       'request_id': '1234.1',
       'frame_id': '123.1',
       'initiator': {'type': 'other'},
       'timestamp': 2,
       'timing': TimingFromDict({})})

  def testRemovesWhitelistRules(self):
    matcher = _RulesMatcher(self._RULES_WITH_WHITELIST, False)
    self.assertEquals(3, len(matcher._rules))
    matcher = _RulesMatcher(self._RULES_WITH_WHITELIST, True)
    self.assertEquals(1, len(matcher._rules))

  def testScriptRule(self):
    matcher = _RulesMatcher([self._SCRIPT_RULE], False)
    request = copy.deepcopy(self._SCRIPT_REQUEST)
    request.resource_type = 'Stylesheet'
    self.assertFalse(matcher.Matches(request))
    self.assertTrue(matcher.Matches(self._SCRIPT_REQUEST))


if __name__ == '__main__':
  unittest.main()
