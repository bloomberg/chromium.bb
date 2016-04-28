// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/test_ink_drop_host.h"

#include "base/memory/ptr_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_hover.h"
#include "ui/views/animation/square_ink_drop_animation.h"
#include "ui/views/animation/test/ink_drop_hover_test_api.h"
#include "ui/views/animation/test/square_ink_drop_animation_test_api.h"

namespace views {

namespace {

// Test specific subclass of InkDropAnimation that returns a test api from
// GetTestApi().
class TestInkDropAnimation : public SquareInkDropAnimation {
 public:
  TestInkDropAnimation(const gfx::Size& large_size,
                       int large_corner_radius,
                       const gfx::Size& small_size,
                       int small_corner_radius,
                       const gfx::Point& center_point,
                       SkColor color)
      : SquareInkDropAnimation(large_size,
                               large_corner_radius,
                               small_size,
                               small_corner_radius,
                               center_point,
                               color) {}

  ~TestInkDropAnimation() override {}

  test::InkDropAnimationTestApi* GetTestApi() override {
    if (!test_api_)
      test_api_.reset(new test::SquareInkDropAnimationTestApi(this));
    return test_api_.get();
  }

 private:
  std::unique_ptr<test::InkDropAnimationTestApi> test_api_;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropAnimation);
};

// Test specific subclass of InkDropHover that returns a test api from
// GetTestApi().
class TestInkDropHover : public InkDropHover {
 public:
  TestInkDropHover(const gfx::Size& size,
                   int corner_radius,
                   const gfx::Point& center_point,
                   SkColor color)
      : InkDropHover(size, corner_radius, center_point, color) {}

  ~TestInkDropHover() override {}

  test::InkDropHoverTestApi* GetTestApi() override {
    if (!test_api_)
      test_api_.reset(new test::InkDropHoverTestApi(this));
    return test_api_.get();
  }

 private:
  std::unique_ptr<test::InkDropHoverTestApi> test_api_;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropHover);
};

}  // namespace

TestInkDropHost::TestInkDropHost()
    : num_ink_drop_layers_(0),
      should_show_hover_(false),
      disable_timers_for_test_(false) {}

TestInkDropHost::~TestInkDropHost() {}

void TestInkDropHost::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  ++num_ink_drop_layers_;
}

void TestInkDropHost::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  --num_ink_drop_layers_;
}

std::unique_ptr<InkDropAnimation> TestInkDropHost::CreateInkDropAnimation()
    const {
  gfx::Size size(10, 10);
  std::unique_ptr<InkDropAnimation> animation(
      new TestInkDropAnimation(size, 5, size, 5, gfx::Point(), SK_ColorBLACK));
  if (disable_timers_for_test_)
    animation->GetTestApi()->SetDisableAnimationTimers(true);
  return animation;
}

std::unique_ptr<InkDropHover> TestInkDropHost::CreateInkDropHover() const {
  std::unique_ptr<InkDropHover> hover;
  if (should_show_hover_) {
    hover.reset(new TestInkDropHover(gfx::Size(10, 10), 4, gfx::Point(),
                                     SK_ColorBLACK));
    if (disable_timers_for_test_)
      hover->GetTestApi()->SetDisableAnimationTimers(true);
  }
  return hover;
}

}  // namespace views
