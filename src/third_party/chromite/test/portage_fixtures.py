# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration and fixtures for pytest.

See the following doc link for an explanation of conftest.py and how it is used
by pytest:
https://docs.pytest.org/en/latest/fixture.html#conftest-py-sharing-fixture-functions
"""

from __future__ import print_function

import pytest

import chromite as cr


@pytest.fixture
def overlay_stack(tmp_path_factory):
  """Factory for stacked Portage overlays.

  The factory function takes an integer argument and returns an iterator of
  that many overlays, each of which has all prior overlays as masters.
  """

  def make_overlay_stack(height):
    if not height <= len(cr.test.Overlay.HIERARCHY_NAMES):
      raise ValueError(
          'Overlay stacks taller than %s are not supported. Received: %s' %
          (len(cr.test.Overlay.HIERARCHY_NAMES), height))

    overlays = []

    for i in range(height):
      overlays.append(
          cr.test.Overlay(
              root_path=tmp_path_factory.mktemp(
                  'overlay-' + cr.test.Overlay.HIERARCHY_NAMES[i]),
              name=cr.test.Overlay.HIERARCHY_NAMES[i],
              masters=overlays))
      yield overlays[i]

  return make_overlay_stack
