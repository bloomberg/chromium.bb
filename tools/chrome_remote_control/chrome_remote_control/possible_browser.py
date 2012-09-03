# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
class PossibleBrowser(object):
  """A browser that can be controlled.

  Call Create() to launch the browser and begin manipulating it..
  """

  def __init__(self, type, options):
    self.type = type
    self._options = options

  def __repr__(self):
    return "PossibleBrowser(type=%s)" % self.type

  def Create(self):
    raise Exception("Must be implemented in subclass.")
