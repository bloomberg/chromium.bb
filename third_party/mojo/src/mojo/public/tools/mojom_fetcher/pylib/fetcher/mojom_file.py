# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from fetcher.dependency import Dependency


class MojomFile(object):
  """Mojom represents an interface file at a given location in the
  repository."""
  def __init__(self, repository, name):
    self.name = name
    self._repository = repository
    self.deps = []

  def add_dependency(self, dependency):
    """Declare a new dependency of this mojom."""
    self.deps.append(Dependency(self._repository, self.name, dependency))
