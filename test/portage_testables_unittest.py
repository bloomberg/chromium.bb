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

@pytest.fixture
def bare_overlay(tmp_path_factory):
  """Create a single bare overlay."""
  p = tmp_path_factory.mktemp('bare-overlay')
  return cr.test.Overlay(p, 'bare')


@pytest.fixture
def minimal_sysroot(bare_overlay, tmp_path_factory):
  """Set up a barebones sysroot with a single associated overlay."""
  path = tmp_path_factory.mktemp('minimal-sysroot')
  base = bare_overlay.create_profile()
  return cr.test.Sysroot(path, base, [bare_overlay])


def test_emerge_against_fake_sysroot(bare_overlay, minimal_sysroot):
  """Test that a basic `emerge` operation works against a test sysroot."""
  pkg1 = cr.test.Package('foo', 'bar')
  bare_overlay.add_package(pkg1)

  pkg2 = cr.test.Package('foo', 'spam', depend='foo/bar')
  bare_overlay.add_package(pkg2)

  minimal_sysroot.run(['emerge', 'foo/spam'])

  res = minimal_sysroot.run(['equery', 'list', '*'], stdout=True)

  assert 'foo/bar' in res.stdout
