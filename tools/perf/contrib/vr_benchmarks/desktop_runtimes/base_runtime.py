# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class DesktopRuntimeBase(object):
  """Interface for all desktop VR runtimes."""

  def __init__(self, finder_options):
    self._finder_options = finder_options
    self._finder_options.browser_options.AppendExtraBrowserArgs(
        '--enable-features=%s' % self.GetFeatureName())

  def Setup(self):
    """Called once before any stories are run."""
    raise NotImplementedError(
        'No runtime setup defined for %s' % self.__class__.__name__)

  def WillRunStory(self):
    """Called before each story is run."""
    raise NotImplementedError(
        'No runtime pre-story defined for %s' % self.__class__.__name__)

  def TearDown(self):
    """Called once after all stories are run."""
    raise NotImplementedError(
        'No runtime tear down defined for %s' % self.__class__.__name__)

  def GetFeatureName(self):
    raise NotImplementedError(
        'No feature defined for %s' % self.__class__.__name__)
