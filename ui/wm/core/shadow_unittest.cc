// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow.h"

#include "base/macros.h"
#include "ui/aura/test/aura_test_base.h"

namespace wm {
namespace {

using ShadowTest = aura::test::AuraTestBase;

// Test if the proper content bounds is calculated based on the current style.
TEST_F(ShadowTest, SetContentBounds) {
  // Verify that layer bounds are outset from content bounds.
  Shadow shadow;
  {
    shadow.Init(Shadow::STYLE_ACTIVE);
    gfx::Rect content_bounds(100, 100, 300, 300);
    shadow.SetContentBounds(content_bounds);
    EXPECT_EQ(content_bounds, shadow.content_bounds());
    gfx::Rect shadow_bounds(content_bounds);
    int elevation = 24;
    shadow_bounds.Inset(-gfx::Insets(2 * elevation) +
                        gfx::Insets(elevation, 0, -elevation, 0));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }

  {
    shadow.SetStyle(Shadow::STYLE_SMALL);
    gfx::Rect content_bounds(100, 100, 300, 300);
    shadow.SetContentBounds(content_bounds);
    EXPECT_EQ(content_bounds, shadow.content_bounds());
    gfx::Rect shadow_bounds(content_bounds);
    int elevation = 6;
    shadow_bounds.Inset(-gfx::Insets(2 * elevation) +
                        gfx::Insets(elevation, 0, -elevation, 0));
    EXPECT_EQ(shadow_bounds, shadow.layer()->bounds());
  }
}

}  // namespace
}  // namespace wm
