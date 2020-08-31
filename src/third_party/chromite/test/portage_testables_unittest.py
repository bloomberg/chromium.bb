# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for test/portage module."""

from __future__ import print_function

import pytest  # pylint: disable=import-error

import chromite as cr

# Pytest's method of declaring fixtures causes Pylint to complain about
# redefined outer names.
# pylint: disable=redefined-outer-name

_OVERLAY_STACK_PARAMS = list(range(1, len(cr.test.Overlay.HIERARCHY_NAMES) + 1))


@pytest.mark.parametrize('height', _OVERLAY_STACK_PARAMS)
def test_overlay_stack_masters(height, overlay_stack):
  """Test that overlays have the correct masters set."""
  overlays = list(overlay_stack(height))

  assert overlays[0].masters is None

  for x in range(1, height):
    assert overlays[x].masters == tuple(overlays[:x])


@pytest.mark.parametrize('height', _OVERLAY_STACK_PARAMS)
def test_overlay_stack_names(height, overlay_stack):
  """Test that generated overlays have the expected names."""
  overlays = overlay_stack(height)

  for i, o in enumerate(overlays):
    assert o.name == cr.test.Overlay.HIERARCHY_NAMES[i]


@pytest.fixture
def minimal_sysroot(overlay_stack, tmp_path_factory):
  """Set up a barebones sysroot with a single associated overlay."""
  overlay, = overlay_stack(1)
  path = tmp_path_factory.mktemp('minimal-sysroot')
  base = overlay.create_profile()
  return overlay, cr.test.Sysroot(path, base, overlays=[overlay])


def test_emerge_against_fake_sysroot(minimal_sysroot):
  """Test that a basic `emerge` operation works against a test sysroot."""
  overlay, sysroot = minimal_sysroot
  pkg1 = cr.test.Package('foo', 'bar')
  overlay.add_package(pkg1)

  pkg2 = cr.test.Package('foo', 'spam', depend='foo/bar')
  overlay.add_package(pkg2)

  sysroot.run(['emerge', 'foo/spam'])

  res = sysroot.run(['equery', 'list', '*'], stdout=True)

  assert 'foo/bar' in res.stdout
