// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/view_model.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "views/view.h"

namespace aura_shell {

TEST(ViewModel, BasicAssertions) {
  views::View v1;
  ViewModel model;
  model.Add(&v1, 0);
  EXPECT_EQ(1, model.view_size());
  EXPECT_EQ(&v1, model.view_at(0));
  gfx::Rect v1_bounds(1, 2, 3, 4);
  model.set_ideal_bounds(0, v1_bounds);
  EXPECT_EQ(v1_bounds, model.ideal_bounds(0));
  EXPECT_EQ(0, model.GetIndexOfView(&v1));
  model.SetViewBoundsToIdealBounds();
  EXPECT_EQ(v1_bounds, v1.bounds());
}

}  // namespace aura_shell
