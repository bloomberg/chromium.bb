# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class Result(object):

  def __init__(self, suspected_cl, revision_url, component_name, author,
               reason, review_url, reviewers, line_content):
    self.suspected_cl = suspected_cl
    self.revision_url = revision_url
    self.component_name = component_name
    self.author = author
    self.reason = reason
    self.review_url = review_url
    self.reviewers = reviewers
    self.line_content = line_content
