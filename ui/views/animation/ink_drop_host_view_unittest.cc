// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_host_view.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"

namespace views {
namespace test {

class InkDropHostViewTest : public testing::Test {
 public:
  InkDropHostViewTest();
  ~InkDropHostViewTest() override;

 protected:
  // Test target.
  InkDropHostView host_view_;

  // Provides internal access to |host_view_| test target.
  InkDropHostViewTestApi test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropHostViewTest);
};

InkDropHostViewTest::InkDropHostViewTest() : test_api_(&host_view_) {}

InkDropHostViewTest::~InkDropHostViewTest() {}

// Verifies the return value of GetInkDropCenterBasedOnLastEvent() for a null
// Event.
TEST_F(InkDropHostViewTest, GetInkDropCenterBasedOnLastEventForNullEvent) {
  host_view_.SetSize(gfx::Size(20, 20));
  test_api_.AnimateInkDrop(InkDropState::ACTION_PENDING, nullptr);
  EXPECT_EQ(gfx::Point(10, 10), test_api_.GetInkDropCenterBasedOnLastEvent());
}

// Verifies the return value of GetInkDropCenterBasedOnLastEvent() for a located
// Event.
TEST_F(InkDropHostViewTest, GetInkDropCenterBasedOnLastEventForLocatedEvent) {
  host_view_.SetSize(gfx::Size(20, 20));

  ui::MouseEvent located_event(ui::ET_MOUSE_PRESSED, gfx::Point(5, 6),
                               gfx::Point(5, 6), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);

  test_api_.AnimateInkDrop(InkDropState::ACTION_PENDING, &located_event);
  EXPECT_EQ(gfx::Point(5, 6), test_api_.GetInkDropCenterBasedOnLastEvent());
}

// Verifies that SetHasInkDrop() sets up gesture handling properly.
TEST_F(InkDropHostViewTest, SetHasInkDropGestureHandler) {
  EXPECT_FALSE(test_api_.HasGestureHandler());

  test_api_.SetHasInkDrop(true);
  EXPECT_TRUE(test_api_.HasGestureHandler());

  test_api_.SetHasInkDrop(true);
  EXPECT_TRUE(test_api_.HasGestureHandler());

  test_api_.SetHasInkDrop(false);
  EXPECT_FALSE(test_api_.HasGestureHandler());
}

}  // namespace test
}  // namespace views
