# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Labels requests according to the type of content they represent."""

import collections
import logging
import os

import loading_trace
import request_track


class ContentClassificationLens(object):
  """Associates requests and frames with the type of content they represent."""
  def __init__(self, trace, ad_rules, tracking_rules):
    """Initializes an instance of ContentClassificationLens.

    Args:
      trace: (LoadingTrace) loading trace.
      ad_rules: ([str]) List of Adblock+ compatible rules used to classify ads.
      tracking_rules: ([str]) List of Adblock+ compatible rules used to
                      classify tracking and analytics.
    """
    self._trace = trace
    self._requests = trace.request_track.GetEvents()
    self._main_frame_id = trace.page_track.GetEvents()[0]['frame_id']
    self._frame_to_requests = collections.defaultdict(list)
    self._ad_requests = set()
    self._tracking_requests = set()
    self._ad_matcher = _RulesMatcher(ad_rules, True)
    self._tracking_matcher = _RulesMatcher(tracking_rules, True)
    self._GroupRequestsByFrameId()
    self._LabelRequests()

  def IsAdRequest(self, request):
    """Returns True iff the request matches one of the ad_rules."""
    return request.request_id in self._ad_requests

  def IsTrackingRequest(self, request):
    """Returns True iff the request matches one of the tracking_rules."""
    return request.request_id in self._tracking_requests

  def IsAdFrame(self, frame_id, ratio):
    """A Frame is an Ad frame if more than |ratio| of its requests are
    ad-related, and is not the main frame."""
    if frame_id == self._main_frame_id:
      return False
    ad_requests_count = sum(r in self._ad_requests
                            for r in self._frame_to_requests[frame_id])
    frame_requests_count = len(self._frame_to_requests[frame_id])
    return (float(ad_requests_count) / frame_requests_count) > ratio

  @classmethod
  def WithRulesFiles(cls, trace, ad_rules_filename, tracking_rules_filename):
    """Returns an instance of ContentClassificationLens with the rules read
    from files.
    """
    ad_rules = []
    tracking_rules = []
    if os.path.exists(ad_rules_filename):
      ad_rules = open(ad_rules_filename, 'r').readlines()
    if os.path.exists(tracking_rules_filename):
      tracking_rules = open(tracking_rules_filename, 'r').readlines()
    return ContentClassificationLens(trace, ad_rules, tracking_rules)

  def _GroupRequestsByFrameId(self):
    for request in self._requests:
      frame_id = request.frame_id
      self._frame_to_requests[frame_id].append(request.request_id)

  def _LabelRequests(self):
    for request in self._requests:
      request_id = request.request_id
      if self._ad_matcher.Matches(request):
        self._ad_requests.add(request_id)
      if self._tracking_matcher.Matches(request):
        self._tracking_requests.add(request_id)


class _RulesMatcher(object):
  """Matches requests with rules in Adblock+ format."""
  _WHITELIST_PREFIX = '@@'
  _RESOURCE_TYPE_TO_OPTIONS_KEY = {
      'Script': 'script', 'Stylesheet': 'stylesheet', 'Image': 'image',
      'XHR': 'xmlhttprequest'}
  def __init__(self, rules, no_whitelist):
    """Initializes an instance of _RulesMatcher.

    Args:
      rules: ([str]) list of rules.
      no_whitelist: (bool) Whether the whitelisting rules should be ignored.
    """
    self._rules = self._FilterRules(rules, no_whitelist)
    if self._rules:
      try:
        import adblockparser
        self._matcher = adblockparser.AdblockRules(self._rules)
      except ImportError:
        logging.critical('Likely you need to install adblockparser. Try:\n'
                         ' pip install --user adblockparser\n'
                         'For 10-100x better performance, also try:\n'
                         " pip install --user 're2 >= 0.2.21'")
        raise
    else:
      self._matcher = None

  def Matches(self, request):
    """Returns whether a request matches one of the rules."""
    if self._matcher is None:
      return False
    url = request.url
    return self._matcher.should_block(url, self._GetOptions(request))

  @classmethod
  def _GetOptions(cls, request):
    options = {}
    resource_type = request.resource_type
    option = cls._RESOURCE_TYPE_TO_OPTIONS_KEY.get(resource_type)
    if option:
      options[option] = True
    return options

  @classmethod
  def _FilterRules(cls, rules, no_whitelist):
    if not no_whitelist:
      return rules
    else:
      return [rule for rule in rules
              if not rule.startswith(cls._WHITELIST_PREFIX)]
