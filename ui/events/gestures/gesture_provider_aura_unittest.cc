// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_provider_aura.h"

namespace ui {

class GestureProviderAuraTest : public testing::Test,
                                public GestureProviderAuraClient {
 public:
  GestureProviderAuraTest() {}

  virtual ~GestureProviderAuraTest() {}

  virtual void OnGestureEvent(GestureEvent* event) OVERRIDE {}

  virtual void SetUp() OVERRIDE {
    provider_.reset(new GestureProviderAura(this));
  }

  virtual void TearDown() OVERRIDE { provider_.reset(); }

  GestureProviderAura* provider() { return provider_.get(); }

 private:
  scoped_ptr<GestureProviderAura> provider_;
  base::MessageLoopForUI message_loop_;
};

TEST_F(GestureProviderAuraTest, IgnoresExtraPressEvents) {
  base::TimeDelta time = ui::EventTimeForNow();
  TouchEvent press1(ET_TOUCH_PRESSED, gfx::PointF(10, 10), 0, time);
  EXPECT_TRUE(provider()->OnTouchEvent(press1));

  time += base::TimeDelta::FromMilliseconds(10);
  TouchEvent press2(ET_TOUCH_PRESSED, gfx::PointF(30, 40), 0, time);
  // Redundant press with same id is ignored.
  EXPECT_FALSE(provider()->OnTouchEvent(press2));
}

TEST_F(GestureProviderAuraTest, IgnoresExtraMoveOrReleaseEvents) {
  base::TimeDelta time = ui::EventTimeForNow();
  TouchEvent press1(ET_TOUCH_PRESSED, gfx::PointF(10, 10), 0, time);
  EXPECT_TRUE(provider()->OnTouchEvent(press1));

  time += base::TimeDelta::FromMilliseconds(10);
  TouchEvent release1(ET_TOUCH_RELEASED, gfx::PointF(30, 40), 0, time);
  EXPECT_TRUE(provider()->OnTouchEvent(release1));

  time += base::TimeDelta::FromMilliseconds(10);
  TouchEvent release2(ET_TOUCH_RELEASED, gfx::PointF(30, 45), 0, time);
  EXPECT_FALSE(provider()->OnTouchEvent(release1));

  time += base::TimeDelta::FromMilliseconds(10);
  TouchEvent move1(ET_TOUCH_MOVED, gfx::PointF(70, 75), 0, time);
  EXPECT_FALSE(provider()->OnTouchEvent(move1));
}

TEST_F(GestureProviderAuraTest, IgnoresIdenticalMoveEvents) {
  const float kRadiusX = 20;
  const float kRadiusY = 30;
  const float kAngle = 120;
  const float kForce = 40;
  const int kTouchId0 = 5;
  const int kTouchId1 = 3;

  base::TimeDelta time = ui::EventTimeForNow();
  TouchEvent press0_1(ET_TOUCH_PRESSED, gfx::PointF(9, 10), kTouchId0, time);
  EXPECT_TRUE(provider()->OnTouchEvent(press0_1));

  TouchEvent press1_1(ET_TOUCH_PRESSED, gfx::PointF(40, 40), kTouchId1, time);
  EXPECT_TRUE(provider()->OnTouchEvent(press1_1));

  time += base::TimeDelta::FromMilliseconds(10);
  TouchEvent move0_1(ET_TOUCH_MOVED,
                   gfx::PointF(10, 10),
                   0,
                   kTouchId0,
                   time,
                   kRadiusX,
                   kRadiusY,
                   kAngle,
                   kForce);
  EXPECT_TRUE(provider()->OnTouchEvent(move0_1));

  TouchEvent move1_1(ET_TOUCH_MOVED,
                   gfx::PointF(100, 200),
                   0,
                   kTouchId1,
                   time,
                   kRadiusX,
                   kRadiusY,
                   kAngle,
                   kForce);
  EXPECT_TRUE(provider()->OnTouchEvent(move1_1));

  time += base::TimeDelta::FromMilliseconds(10);
  TouchEvent move0_2(ET_TOUCH_MOVED,
                     gfx::PointF(10, 10),
                     0,
                     kTouchId0,
                     time,
                     kRadiusX,
                     kRadiusY,
                     kAngle,
                     kForce);
  // Nothing has changed, so ignore the move.
  EXPECT_FALSE(provider()->OnTouchEvent(move0_2));

  TouchEvent move1_2(ET_TOUCH_MOVED,
                     gfx::PointF(100, 200),
                     0,
                     kTouchId1,
                     time,
                     kRadiusX,
                     kRadiusY,
                     kAngle,
                     kForce);
  // Nothing has changed, so ignore the move.
  EXPECT_FALSE(provider()->OnTouchEvent(move1_2));

  time += base::TimeDelta::FromMilliseconds(10);
  TouchEvent move0_3(ET_TOUCH_MOVED,
                     gfx::PointF(70, 75.1f),
                     0,
                     kTouchId0,
                     time,
                     kRadiusX,
                     kRadiusY,
                     kAngle,
                     kForce);
  // Position has changed, so don't ignore the move.
  EXPECT_TRUE(provider()->OnTouchEvent(move0_3));

  time += base::TimeDelta::FromMilliseconds(10);
  TouchEvent move0_4(ET_TOUCH_MOVED,
                     gfx::PointF(70, 75.1f),
                     0,
                     kTouchId0,
                     time,
                     kRadiusX,
                     kRadiusY + 1,
                     kAngle,
                     kForce);
}

}  // namespace ui
