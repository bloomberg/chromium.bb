// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/events/test/motion_event_test_utils.h"

namespace ui {

class FilteredGestureProviderTest : public GestureProviderClient,
                                    public testing::Test {
 public:
  FilteredGestureProviderTest() {}
  ~FilteredGestureProviderTest() override {}

  // GestureProviderClient implementation.
  void OnGestureEvent(const GestureEventData&) override {}

 private:
  base::MessageLoopForUI message_loop_;
};

TEST_F(FilteredGestureProviderTest, TouchDidGenerateScroll) {
  GestureProvider::Config config;
  FilteredGestureProvider provider(config, this);

  const float kSlopRegion = config.gesture_detector_config.touch_slop;

  test::MockMotionEvent event;
  event.set_event_time(base::TimeTicks::Now());
  event.PressPoint(0, 0);
  auto result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_FALSE(result.did_generate_scroll);

  event.MovePoint(0, kSlopRegion / 2.f, 0);
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_FALSE(result.did_generate_scroll);

  // Exceeding the slop should triggering scrolling and be reflected in the API.
  event.MovePoint(0, kSlopRegion * 2.f, 0);
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_TRUE(result.did_generate_scroll);

  // No movement should indicate no scrolling.
  event.MovePoint(0, kSlopRegion * 2.f, 0);
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_FALSE(result.did_generate_scroll);

  // Nonzero movement should reflect scrolling after exceeding the slop region.
  event.MovePoint(0, 0, 0);
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_TRUE(result.did_generate_scroll);

  // Ending a touch with no fling should not indicate scrolling.
  event.ReleasePoint();
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_FALSE(result.did_generate_scroll);

  // Ending a touch with a fling *should* indicate scrolling.
  base::TimeTicks time = base::TimeTicks::Now();
  event.PressPoint(0, 0);
  event.set_event_time(time);
  ASSERT_TRUE(provider.OnTouchEvent(event).succeeded);

  time += base::TimeDelta::FromMilliseconds(10);
  event.MovePoint(0, kSlopRegion * 50, 0);
  event.set_event_time(time);
  ASSERT_TRUE(provider.OnTouchEvent(event).succeeded);

  event.ReleasePoint();
  result = provider.OnTouchEvent(event);
  EXPECT_TRUE(result.succeeded);
  EXPECT_TRUE(result.did_generate_scroll);
}

}  // namespace ui
