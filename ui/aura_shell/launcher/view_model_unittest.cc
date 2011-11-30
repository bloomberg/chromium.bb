// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/view_model.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

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
}

TEST(ViewModel, Move) {
  views::View v1, v2, v3;
  ViewModel model;
  model.Add(&v1, 0);
  model.Add(&v2, 1);
  model.Add(&v3, 2);
  model.Move(0, 2);
  EXPECT_EQ(&v1, model.view_at(2));
  EXPECT_EQ(&v2, model.view_at(0));
  EXPECT_EQ(&v3, model.view_at(1));

  model.Move(2, 0);
  EXPECT_EQ(&v1, model.view_at(0));
  EXPECT_EQ(&v2, model.view_at(1));
  EXPECT_EQ(&v3, model.view_at(2));
}

}  // namespace aura_shell
