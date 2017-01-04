// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow.h"

#include "base/macros.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/wm/core/shadow_types.h"

namespace wm {
namespace {

gfx::Insets InsetsForElevation(int elevation) {
  return -gfx::Insets(2 * elevation) + gfx::Insets(elevation, 0, -elevation, 0);
}

using ShadowTest = aura::test::AuraTestBase;

// Test if the proper content bounds is calculated based on the current style.
TEST_F(ShadowTest, SetContentBounds) {
  // Verify that layer bounds are outset from content bounds.
  Shadow shadow;
  {
    shadow.Init(ShadowElevation::LARGE);
    gfx::Rect content_bounds(100, 100, 300, 300);
    shadow.SetContentBounds(content_bounds);
    EXPECT_EQ(content_bounds, shadow.content_bounds());
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(24));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }

  {
    shadow.SetElevation(ShadowElevation::SMALL);
    gfx::Rect content_bounds(100, 100, 300, 300);
    shadow.SetContentBounds(content_bounds);
    EXPECT_EQ(content_bounds, shadow.content_bounds());
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(6));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }
}

// Test that the elevation is reduced when the contents are too small to handle
// the full elevation.
TEST_F(ShadowTest, AdjustElevationForSmallContents) {
  Shadow shadow;
  shadow.Init(ShadowElevation::LARGE);
  {
    gfx::Rect content_bounds(100, 100, 300, 300);
    shadow.SetContentBounds(content_bounds);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation(24));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }

  {
    constexpr int kWidth = 80;
    gfx::Rect content_bounds(100, 100, kWidth, 300);
    shadow.SetContentBounds(content_bounds);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation((kWidth - 4) / 4));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }

  {
    constexpr int kHeight = 80;
    gfx::Rect content_bounds(100, 100, 300, kHeight);
    shadow.SetContentBounds(content_bounds);
    gfx::Rect shadow_bounds(content_bounds);
    shadow_bounds.Inset(InsetsForElevation((kHeight - 4) / 4));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }
}

}  // namespace
}  // namespace wm
