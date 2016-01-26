// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_hover.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/size.h"

namespace views {
namespace test {

class InkDropHoverTest : public testing::Test {
 public:
  InkDropHoverTest();
  ~InkDropHoverTest() override;

 protected:
  scoped_ptr<InkDropHover> CreateInkDropHover() const;

 private:
  // Enables zero duration animations during the tests.
  scoped_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  DISALLOW_COPY_AND_ASSIGN(InkDropHoverTest);
};

InkDropHoverTest::InkDropHoverTest() {
  zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
}

InkDropHoverTest::~InkDropHoverTest() {}

scoped_ptr<InkDropHover> InkDropHoverTest::CreateInkDropHover() const {
  return make_scoped_ptr(new InkDropHover(gfx::Size(10, 10), 3));
}

TEST_F(InkDropHoverTest, InitialStateAfterConstruction) {
  scoped_ptr<InkDropHover> ink_drop_hover = CreateInkDropHover();
  EXPECT_FALSE(ink_drop_hover->IsVisible());
}

TEST_F(InkDropHoverTest, IsHoveredStateTransitions) {
  scoped_ptr<InkDropHover> ink_drop_hover = CreateInkDropHover();

  ink_drop_hover->FadeIn(base::TimeDelta::FromMilliseconds(0));
  EXPECT_TRUE(ink_drop_hover->IsVisible());

  ink_drop_hover->FadeOut(base::TimeDelta::FromMilliseconds(0));
  EXPECT_FALSE(ink_drop_hover->IsVisible());
}

}  // namespace test
}  // namespace views
