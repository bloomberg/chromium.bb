# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class RenderingStats(object):
  """A helper class for handling chrome.gpuBenchmarking.renderStats
  objects"""
  def __init__(self, dict):
    for k, v in dict.items():
      setattr(self, underscore_delimited_name, v)
    if 'num_frames_sent_to_screen' not in self.__dict__:
      self.num_frames_sent_to_screen = self.num_animation_frames

