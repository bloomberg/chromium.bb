# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import urlparse

class Page(object):
  def __init__(self, url, attributes=None):
    self.url = url
    parsed_url = urlparse.urlparse(url)
    if parsed_url.scheme == None: # pylint: disable=E1101
      raise Exception('urls must be fully qualified: %s' % url)
    self.interactions = 'scroll'
    self.credentials = None
    self.wait_time_after_navigate = 2
    self.scroll_is_infinite = False
    self.wait_for_javascript_expression = None

    if attributes:
      for k, v in attributes.iteritems():
        setattr(self, k, v)

  def __str__(self):
    return self.url
