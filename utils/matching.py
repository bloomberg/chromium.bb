# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides matching utilities."""

from __future__ import print_function

import difflib


def GetMostLikelyMatchedObject(haystack, needle, name_func=lambda x: x,
                               matched_score_threshold=0.4):
  """Matches objects whose names are most likely matched with target.

  Args:
    haystack (list): Objects to search against.
    needle (str): The name to match.
    name_func (callable): Function to get object name to match. Default is the
      identity function.
    matched_score_threshold (float): The threshold of likelihood to match.
      Must be in the range [0,1]. Default is 0.4.

  Returns:
    A list of entities from |haystack| whose names are likely needle.
  """

  def _Score(obj):
    return difflib.SequenceMatcher(a=name_func(obj), b=needle).ratio()

  return sorted([o for o in haystack if _Score(o) > matched_score_threshold])
