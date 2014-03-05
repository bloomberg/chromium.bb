// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/gesture_provider.h"
#include "ui/events/gesture_detection/mock_motion_event.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/geometry/point_f.h"

using base::TimeDelta;
using base::TimeTicks;

namespace ui {
namespace {

const float kFakeCoordX = 42.f;
const float kFakeCoordY = 24.f;
const TimeDelta kOneSecond = TimeDelta::FromSeconds(1);
const TimeDelta kFiveMilliseconds = TimeDelta::FromMilliseconds(5);

}  // namespace

class GestureProviderTest : public testing::Test, public GestureProviderClient {
 public:
  GestureProviderTest() {}
  virtual ~GestureProviderTest() {}

  static MockMotionEvent ObtainMotionEvent(base::TimeTicks event_time,
                                           MotionEvent::Action action,
                                           float x,
                                           float y) {
    return MockMotionEvent(action, event_time, x, y);
  }

  static MockMotionEvent ObtainMotionEvent(base::TimeTicks event_time,
                                           MotionEvent::Action action,
                                           float x0,
                                           float y0,
                                           float x1,
                                           float y1) {
    return MockMotionEvent(action, event_time, x0, y0, x1, y1);
  }

  static MockMotionEvent ObtainMotionEvent(base::TimeTicks event_time,
                                           MotionEvent::Action action) {
    return ObtainMotionEvent(event_time, action, kFakeCoordX, kFakeCoordY);
  }

  // Test
  virtual void SetUp() OVERRIDE {
    gesture_provider_.reset(new GestureProvider(GetDefaultConfig(), this));
    gesture_provider_->SetMultiTouchSupportEnabled(false);
  }

  virtual void TearDown() OVERRIDE {
    gestures_.clear();
    gesture_provider_.reset();
  }

  // GestureProviderClient
  virtual void OnGestureEvent(const GestureEventData& gesture) OVERRIDE {
    if (gesture.type == ET_GESTURE_SCROLL_BEGIN)
      active_scroll_begin_event_.reset(new GestureEventData(gesture));
    gestures_.push_back(gesture);
  }

  bool CancelActiveTouchSequence() {
    if (!gesture_provider_->current_down_event())
      return false;
    return gesture_provider_->OnTouchEvent(
        *gesture_provider_->current_down_event()->Cancel());
  }

  bool HasReceivedGesture(EventType type) const {
    for (size_t i = 0; i < gestures_.size(); ++i) {
      if (gestures_[i].type == type)
        return true;
    }
    return false;
  }

  const GestureEventData& GetMostRecentGestureEvent() const {
    EXPECT_FALSE(gestures_.empty());
    return gestures_.back();
  }

  const EventType GetMostRecentGestureEventType() const {
    EXPECT_FALSE(gestures_.empty());
    return gestures_.back().type;
  }

  size_t GetReceivedGestureCount() const { return gestures_.size(); }

  const GestureEventData& GetReceivedGesture(size_t index) const {
    EXPECT_LT(index, GetReceivedGestureCount());
    return gestures_[index];
  }

  const GestureEventData* GetActiveScrollBeginEvent() const {
    return active_scroll_begin_event_ ? active_scroll_begin_event_.get() : NULL;
  }

  const GestureProvider::Config& GetDefaultConfig() const {
    static GestureProvider::Config sConfig;
    return sConfig;
  }

  int GetTouchSlop() const {
    return GetDefaultConfig().gesture_detector_config.scaled_touch_slop;
  }

  base::TimeDelta GetLongPressTimeout() const {
    return GetDefaultConfig().gesture_detector_config.longpress_timeout;
  }

  base::TimeDelta GetShowPressTimeout() const {
    return GetDefaultConfig().gesture_detector_config.tap_timeout;
  }

 protected:
  void CheckScrollEventSequenceForEndActionType(
      MotionEvent::Action end_action_type) {
    base::TimeTicks event_time = base::TimeTicks::Now();
    const int scroll_to_x = kFakeCoordX + 100;
    const int scroll_to_y = kFakeCoordY + 100;

    MockMotionEvent event =
        ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);

    EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

    event = ObtainMotionEvent(event_time + kOneSecond,
                              MotionEvent::ACTION_MOVE,
                              scroll_to_x,
                              scroll_to_y);
    EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
    EXPECT_TRUE(gesture_provider_->IsScrollInProgress());
    EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_BEGIN));
    EXPECT_EQ(ET_GESTURE_SCROLL_UPDATE, GetMostRecentGestureEventType());
    ASSERT_EQ(4U, GetReceivedGestureCount()) << "Only TapDown, TapCancel, "
                                                "ScrollBegin and ScrollBy "
                                                "should have been sent";

    EXPECT_EQ(ET_GESTURE_TAP_CANCEL, GetReceivedGesture(1).type);
    EXPECT_EQ(ET_GESTURE_SCROLL_BEGIN, GetReceivedGesture(2).type);
    EXPECT_EQ(event_time + kOneSecond, GetReceivedGesture(2).time)
        << "ScrollBegin should have the time of the ACTION_MOVE";

    event = ObtainMotionEvent(
        event_time + kOneSecond, end_action_type, scroll_to_x, scroll_to_y);
    gesture_provider_->OnTouchEvent(event);
    EXPECT_FALSE(gesture_provider_->IsScrollInProgress());
    EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_END));
    EXPECT_EQ(ET_GESTURE_SCROLL_END, GetMostRecentGestureEventType());
  }

  static void Wait(base::TimeDelta delay) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::MessageLoop::QuitClosure(), delay);
    base::MessageLoop::current()->Run();
  }

  std::vector<GestureEventData> gestures_;
  scoped_ptr<GestureProvider> gesture_provider_;
  scoped_ptr<GestureEventData> active_scroll_begin_event_;
  base::MessageLoopForUI message_loop_;
};

// Verify that a DOWN followed shortly by an UP will trigger a single tap.
TEST_F(GestureProviderTest, GestureTapTap) {
  base::TimeTicks event_time = base::TimeTicks::Now();

  gesture_provider_->SetDoubleTapSupportForPlatformEnabled(false);

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_UP);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP, GetMostRecentGestureEventType());
}

// Verify that a DOWN followed shortly by an UP will trigger
// a ET_GESTURE_TAP_UNCONFIRMED event if double-tap is enabled.
TEST_F(GestureProviderTest, GestureTapTapWithDelay) {
  base::TimeTicks event_time = base::TimeTicks::Now();

  gesture_provider_->SetDoubleTapSupportForPlatformEnabled(true);

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_UP);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_UNCONFIRMED, GetMostRecentGestureEventType());
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_TAP));
}

// Verify that a DOWN followed by a MOVE will trigger fling (but not LONG).
TEST_F(GestureProviderTest, GestureFlingAndCancelLongPress) {
  base::TimeTicks event_time = TimeTicks::Now();

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);

  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(event_time + kFiveMilliseconds,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX * 10,
                            kFakeCoordY * 10);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX * 10,
                            kFakeCoordY * 10);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_SCROLL_FLING_START, GetMostRecentGestureEventType());
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_LONG_PRESS));
}

// Verify that for a normal scroll the following events are sent:
// - ET_GESTURE_SCROLL_BEGIN
// - ET_GESTURE_SCROLL_UPDATE
// - ET_GESTURE_SCROLL_END
TEST_F(GestureProviderTest, ScrollEventActionUpSequence) {
  CheckScrollEventSequenceForEndActionType(MotionEvent::ACTION_UP);
}

// Verify that for a cancelled scroll the following events are sent:
// - ET_GESTURE_SCROLL_BEGIN
// - ET_GESTURE_SCROLL_UPDATE
// - ET_GESTURE_SCROLL_END
TEST_F(GestureProviderTest, ScrollEventActionCancelSequence) {
  CheckScrollEventSequenceForEndActionType(MotionEvent::ACTION_CANCEL);
}

// Verify that for a normal fling (fling after scroll) the following events are
// sent:
// - ET_GESTURE_SCROLL_BEGIN
// - ET_SCROLL_FLING_START
TEST_F(GestureProviderTest, FlingEventSequence) {
  base::TimeTicks event_time = base::TimeTicks::Now();

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);

  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  event = ObtainMotionEvent(event_time + kFiveMilliseconds,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX * 5,
                            kFakeCoordY * 5);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(gesture_provider_->IsScrollInProgress());
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_BEGIN));
  EXPECT_EQ(ET_GESTURE_SCROLL_UPDATE, GetMostRecentGestureEventType());
  ASSERT_EQ(4U, GetReceivedGestureCount());
  ASSERT_EQ(ET_GESTURE_SCROLL_BEGIN, GetReceivedGesture(2).type);

  // We don't want to take a dependency here on exactly how hints are calculated
  // for a fling (eg. may depend on velocity), so just validate the direction.
  int hint_x = GetReceivedGesture(2).details.scroll_begin.delta_x_hint;
  int hint_y = GetReceivedGesture(2).details.scroll_begin.delta_y_hint;
  EXPECT_TRUE(hint_x > 0 && hint_y > 0 && hint_x > hint_y)
      << "ScrollBegin hint should be in positive X axis";

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 3,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX * 10,
                            kFakeCoordY * 10);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_FALSE(gesture_provider_->IsScrollInProgress());
  EXPECT_EQ(ET_SCROLL_FLING_START, GetMostRecentGestureEventType());
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_SCROLL_END));
  EXPECT_EQ(event_time + kFiveMilliseconds * 3,
            GetMostRecentGestureEvent().time)
      << "FlingStart should have the time of the ACTION_UP";
}

TEST_F(GestureProviderTest, TapCancelledWhenWindowFocusLost) {
  const base::TimeTicks event_time = TimeTicks::Now();

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());

  Wait(GetShowPressTimeout() + kFiveMilliseconds);
  EXPECT_EQ(ET_GESTURE_SHOW_PRESS, GetMostRecentGestureEventType());

  Wait(GetLongPressTimeout() + kFiveMilliseconds);
  EXPECT_EQ(ET_GESTURE_LONG_PRESS, GetMostRecentGestureEventType());

  // The long press triggers window focus loss by opening a context menu
  EXPECT_TRUE(CancelActiveTouchSequence());
  EXPECT_EQ(ET_GESTURE_TAP_CANCEL, GetMostRecentGestureEventType());
}

TEST_F(GestureProviderTest, TapCancelledWhenScrollBegins) {
  base::TimeTicks event_time = base::TimeTicks::Now();

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);

  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());
  event = ObtainMotionEvent(event_time + kFiveMilliseconds,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX + 50,
                            kFakeCoordY + 50);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  EXPECT_EQ(ET_GESTURE_SCROLL_UPDATE, GetMostRecentGestureEventType());
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_TAP_CANCEL));

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX + 100,
                            kFakeCoordY + 100);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_SCROLL_FLING_START, GetMostRecentGestureEventType());
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_SCROLL_END));
}

TEST_F(GestureProviderTest, DoubleTap) {
  base::TimeTicks event_time = base::TimeTicks::Now();

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(event_time + kFiveMilliseconds,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY);
  gesture_provider_->OnTouchEvent(event);
  EXPECT_EQ(ET_GESTURE_TAP_UNCONFIRMED, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_DOWN,
                            kFakeCoordX,
                            kFakeCoordY);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());

  // Moving a very small amount of distance should not trigger the double tap
  // drag zoom mode.
  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 3,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY + 1);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 3,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY + 1);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_DOUBLE_TAP, GetMostRecentGestureEventType());
}

TEST_F(GestureProviderTest, DoubleTapDragZoom) {
  const base::TimeTicks down_time_1 = TimeTicks::Now();
  const base::TimeTicks down_time_2 = down_time_1 + kFiveMilliseconds * 20;

  MockMotionEvent event =
      ObtainMotionEvent(down_time_1, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  event = ObtainMotionEvent(down_time_1 + kFiveMilliseconds,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY);
  gesture_provider_->OnTouchEvent(event);
  EXPECT_EQ(ET_GESTURE_TAP_UNCONFIRMED, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(
      down_time_2, MotionEvent::ACTION_DOWN, kFakeCoordX, kFakeCoordY);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY + 100);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_BEGIN));
  const GestureEventData* scroll_begin_gesture = GetActiveScrollBeginEvent();
  ASSERT_TRUE(!!scroll_begin_gesture);
  EXPECT_EQ(0, scroll_begin_gesture->details.scroll_begin.delta_x_hint);
  EXPECT_EQ(100, scroll_begin_gesture->details.scroll_begin.delta_y_hint);
  EXPECT_EQ(ET_GESTURE_PINCH_BEGIN, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY + 200);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_UPDATE));
  EXPECT_EQ(ET_GESTURE_PINCH_UPDATE, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds * 3,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY + 200);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_PINCH_END));
  EXPECT_EQ(ET_GESTURE_SCROLL_END, GetMostRecentGestureEventType());
}

TEST_F(GestureProviderTest, DoubleTapDragZoomCancelledOnSecondaryPointerDown) {
  const base::TimeTicks down_time_1 = TimeTicks::Now();
  const base::TimeTicks down_time_2 = down_time_1 + kFiveMilliseconds * 20;

  MockMotionEvent event =
      ObtainMotionEvent(down_time_1, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(down_time_1 + kFiveMilliseconds,
                            MotionEvent::ACTION_UP);
  gesture_provider_->OnTouchEvent(event);
  EXPECT_EQ(ET_GESTURE_TAP_UNCONFIRMED, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(down_time_2, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_TAP_CANCEL));

  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds * 10,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY - 30);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_BEGIN));
  EXPECT_EQ(ET_GESTURE_PINCH_BEGIN, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds * 20,
                            MotionEvent::ACTION_POINTER_DOWN,
                            kFakeCoordX,
                            kFakeCoordY - 30,
                            kFakeCoordX + 50,
                            kFakeCoordY + 50);
  gesture_provider_->OnTouchEvent(event);
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_PINCH_END));
  EXPECT_EQ(ET_GESTURE_SCROLL_END, GetMostRecentGestureEventType());
  const size_t gesture_count = GetReceivedGestureCount();

  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds * 30,
                            MotionEvent::ACTION_POINTER_UP,
                            kFakeCoordX,
                            kFakeCoordY - 30,
                            kFakeCoordX + 50,
                            kFakeCoordY + 50);
  gesture_provider_->OnTouchEvent(event);
  EXPECT_EQ(gesture_count, GetReceivedGestureCount());
}

// Generate a scroll gesture and verify that the resulting scroll motion event
// has both absolute and relative position information.
TEST_F(GestureProviderTest, ScrollUpdateValues) {
  const int delta_x = 16;
  const int delta_y = 84;

  const base::TimeTicks event_time = TimeTicks::Now();

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  // Move twice so that we get two ET_GESTURE_SCROLL_UPDATE events and can
  // compare the relative and absolute coordinates.
  event = ObtainMotionEvent(event_time + kFiveMilliseconds,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX - delta_x / 2,
                            kFakeCoordY - delta_y / 2);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX - delta_x,
                            kFakeCoordY - delta_y);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  // Make sure the reported gesture event has all the expected details.
  ASSERT_LT(0U, GetReceivedGestureCount());
  GestureEventData gesture = GetMostRecentGestureEvent();
  EXPECT_EQ(ET_GESTURE_SCROLL_UPDATE, gesture.type);
  EXPECT_EQ(event_time + kFiveMilliseconds * 2, gesture.time);
  EXPECT_EQ(kFakeCoordX - delta_x, gesture.x);
  EXPECT_EQ(kFakeCoordY - delta_y, gesture.y);

  // No horizontal delta because of snapping.
  EXPECT_EQ(0, gesture.details.scroll_update.delta_x);
  EXPECT_EQ(-delta_y / 2, gesture.details.scroll_update.delta_y);
}

// Generate a scroll gesture and verify that the resulting scroll begin event
// has the expected hint values.
TEST_F(GestureProviderTest, ScrollBeginValues) {
  const int delta_x = 13;
  const int delta_y = 89;

  const base::TimeTicks event_time = TimeTicks::Now();

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  // Move twice such that the first event isn't sufficient to start
  // scrolling on it's own.
  event = ObtainMotionEvent(event_time + kFiveMilliseconds,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX + 2,
                            kFakeCoordY + 1);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_FALSE(gesture_provider_->IsScrollInProgress());

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX + delta_x,
                            kFakeCoordY + delta_y);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(gesture_provider_->IsScrollInProgress());

  const GestureEventData* scroll_begin_gesture = GetActiveScrollBeginEvent();
  ASSERT_TRUE(!!scroll_begin_gesture);
  EXPECT_EQ(delta_x, scroll_begin_gesture->details.scroll_begin.delta_x_hint);
  EXPECT_EQ(delta_y, scroll_begin_gesture->details.scroll_begin.delta_y_hint);
}

TEST_F(GestureProviderTest, LongPressAndTapCancelledWhenScrollBegins) {
  base::TimeTicks event_time = base::TimeTicks::Now();

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  event = ObtainMotionEvent(event_time + kFiveMilliseconds,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX * 5,
                            kFakeCoordY * 5);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX * 10,
                            kFakeCoordY * 10);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  const base::TimeDelta long_press_timeout =
      GetLongPressTimeout() + GetShowPressTimeout() + kFiveMilliseconds * 2;
  Wait(long_press_timeout);

  // No LONG_TAP as the LONG_PRESS timer is cancelled.
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_LONG_PRESS));
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_LONG_TAP));
}

// Verify that LONG_TAP is triggered after LONG_PRESS followed by an UP.
TEST_F(GestureProviderTest, GestureLongTap) {
  base::TimeTicks event_time = base::TimeTicks::Now();

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  const base::TimeDelta long_press_timeout =
      GetLongPressTimeout() + GetShowPressTimeout() + kFiveMilliseconds * 2;
  Wait(long_press_timeout);

  EXPECT_EQ(ET_GESTURE_LONG_PRESS, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(event_time + kOneSecond, MotionEvent::ACTION_UP);
  gesture_provider_->OnTouchEvent(event);
  EXPECT_EQ(ET_GESTURE_LONG_TAP, GetMostRecentGestureEventType());
}

TEST_F(GestureProviderTest, GestureLongPressDoesNotPreventScrolling) {
  base::TimeTicks event_time = base::TimeTicks::Now();

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  const base::TimeDelta long_press_timeout =
      GetLongPressTimeout() + GetShowPressTimeout() + kFiveMilliseconds * 2;
  Wait(long_press_timeout);

  EXPECT_EQ(ET_GESTURE_LONG_PRESS, GetMostRecentGestureEventType());
  event = ObtainMotionEvent(event_time + long_press_timeout,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX + 100,
                            kFakeCoordY + 100);
  gesture_provider_->OnTouchEvent(event);

  EXPECT_EQ(ET_GESTURE_SCROLL_UPDATE, GetMostRecentGestureEventType());
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_BEGIN));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_TAP_CANCEL));

  event = ObtainMotionEvent(event_time + long_press_timeout,
                            MotionEvent::ACTION_UP);
  gesture_provider_->OnTouchEvent(event);
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_LONG_TAP));
}

TEST_F(GestureProviderTest, NoGestureLongPressDuringDoubleTap) {
  base::TimeTicks event_time = base::TimeTicks::Now();

  MockMotionEvent event = ObtainMotionEvent(
      event_time, MotionEvent::ACTION_DOWN, kFakeCoordX, kFakeCoordY);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  event = ObtainMotionEvent(event_time + kFiveMilliseconds,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY);
  gesture_provider_->OnTouchEvent(event);
  EXPECT_EQ(ET_GESTURE_TAP_UNCONFIRMED, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_DOWN,
                            kFakeCoordX,
                            kFakeCoordY);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());
  EXPECT_TRUE(gesture_provider_->IsDoubleTapInProgress());

  const base::TimeDelta long_press_timeout =
      GetLongPressTimeout() + GetShowPressTimeout() + kFiveMilliseconds * 2;
  Wait(long_press_timeout);
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_LONG_PRESS));

  event = ObtainMotionEvent(event_time + long_press_timeout,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX + 20,
                            kFakeCoordY + 20);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_PINCH_BEGIN, GetMostRecentGestureEventType());
  EXPECT_TRUE(gesture_provider_->IsDoubleTapInProgress());

  event = ObtainMotionEvent(event_time + long_press_timeout + kFiveMilliseconds,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY + 1);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_SCROLL_END, GetMostRecentGestureEventType());
  EXPECT_FALSE(gesture_provider_->IsDoubleTapInProgress());
}

// Verify that the touch slop region is removed from the first scroll delta to
// avoid a jump when starting to scroll.
TEST_F(GestureProviderTest, TouchSlopRemovedFromScroll) {
  const int scaled_touch_slop = GetTouchSlop();
  const int scroll_delta = 5;

  base::TimeTicks event_time = base::TimeTicks::Now();

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY + scaled_touch_slop + scroll_delta);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  EXPECT_EQ(ET_GESTURE_SCROLL_UPDATE, GetMostRecentGestureEventType());
  GestureEventData gesture = GetMostRecentGestureEvent();
  EXPECT_EQ(0, gesture.details.scroll_update.delta_x);
  EXPECT_EQ(scroll_delta, gesture.details.scroll_update.delta_y);
}

TEST_F(GestureProviderTest, NoDoubleTapWhenExplicitlyDisabled) {
  gesture_provider_->SetDoubleTapSupportForPlatformEnabled(false);

  base::TimeTicks event_time = base::TimeTicks::Now();
  MockMotionEvent event = ObtainMotionEvent(
      event_time, MotionEvent::ACTION_DOWN, kFakeCoordX, kFakeCoordY);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(1U, GetReceivedGestureCount());
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(event_time + kFiveMilliseconds,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_DOWN,
                            kFakeCoordX,
                            kFakeCoordY);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 3,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP, GetMostRecentGestureEventType());
}

TEST_F(GestureProviderTest, NoDoubleTapDragZoomWhenDisabledOnPlatform) {
  const base::TimeTicks down_time_1 = TimeTicks::Now();
  const base::TimeTicks down_time_2 = down_time_1 + kFiveMilliseconds * 20;

  gesture_provider_->SetDoubleTapSupportForPlatformEnabled(false);

  MockMotionEvent event =
      ObtainMotionEvent(down_time_1, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  event = ObtainMotionEvent(down_time_1 + kFiveMilliseconds,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY);
  gesture_provider_->OnTouchEvent(event);

  event = ObtainMotionEvent(
      down_time_2, MotionEvent::ACTION_DOWN, kFakeCoordX, kFakeCoordY);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY + 100);

  // The move should become a scroll, as doubletap drag zoom is disabled.
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_BEGIN));
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_PINCH_BEGIN));

  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY + 200);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_SCROLL_UPDATE, GetMostRecentGestureEventType());
  EXPECT_EQ(down_time_2 + kFiveMilliseconds * 2,
            GetMostRecentGestureEvent().time);
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_PINCH_UPDATE));

  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds * 3,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY + 200);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_PINCH_END));
}

// Verify that double tap drag zoom feature is not invoked when the gesture
// handler is told to disable double tap gesture detection.
// The second tap sequence should be treated just as the first would be.
TEST_F(GestureProviderTest, NoDoubleTapDragZoomWhenDisabledOnPage) {
  const base::TimeTicks down_time_1 = TimeTicks::Now();
  const base::TimeTicks down_time_2 = down_time_1 + kFiveMilliseconds * 20;

  gesture_provider_->SetDoubleTapSupportForPageEnabled(false);

  MockMotionEvent event =
      ObtainMotionEvent(down_time_1, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  event = ObtainMotionEvent(down_time_1 + kFiveMilliseconds,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY);
  gesture_provider_->OnTouchEvent(event);

  event = ObtainMotionEvent(
      down_time_2, MotionEvent::ACTION_DOWN, kFakeCoordX, kFakeCoordY);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY + 100);

  // The move should become a scroll, as double tap drag zoom is disabled.
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_BEGIN));
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_PINCH_BEGIN));

  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY + 200);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_SCROLL_UPDATE, GetMostRecentGestureEventType());
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_PINCH_UPDATE));

  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds * 3,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY + 200);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_PINCH_END));
}

// Verify that updating double tap support during a double tap drag zoom
// disables double tap detection after the gesture has ended.
TEST_F(GestureProviderTest, FixedPageScaleDuringDoubleTapDragZoom) {
  base::TimeTicks down_time_1 = TimeTicks::Now();
  base::TimeTicks down_time_2 = down_time_1 + kFiveMilliseconds * 20;

  // Start a double-tap drag gesture.
  MockMotionEvent event =
      ObtainMotionEvent(down_time_1, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  event = ObtainMotionEvent(down_time_1 + kFiveMilliseconds,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY);
  gesture_provider_->OnTouchEvent(event);
  event = ObtainMotionEvent(
      down_time_2, MotionEvent::ACTION_DOWN, kFakeCoordX, kFakeCoordY);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY + 100);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_BEGIN));
  EXPECT_EQ(ET_GESTURE_PINCH_BEGIN, GetMostRecentGestureEventType());

  // Simulate setting a fixed page scale (or a mobile viewport);
  // this should not disrupt the current double-tap gesture.
  gesture_provider_->SetDoubleTapSupportForPageEnabled(false);

  // Double tap zoom updates should continue.
  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY + 200);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_UPDATE));
  EXPECT_EQ(ET_GESTURE_PINCH_UPDATE, GetMostRecentGestureEventType());
  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds * 3,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY + 200);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_PINCH_END));
  EXPECT_EQ(ET_GESTURE_SCROLL_END, GetMostRecentGestureEventType());

  // The double-tap gesture has finished, but the page scale is fixed.
  // The same event sequence should not generate any double tap getsures.
  gestures_.clear();
  down_time_1 += kFiveMilliseconds * 40;
  down_time_2 += kFiveMilliseconds * 40;

  // Start a double-tap drag gesture.
  event = ObtainMotionEvent(down_time_1, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  event = ObtainMotionEvent(down_time_1 + kFiveMilliseconds,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY);
  gesture_provider_->OnTouchEvent(event);
  event = ObtainMotionEvent(
      down_time_2, MotionEvent::ACTION_DOWN, kFakeCoordX, kFakeCoordY);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY + 100);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_BEGIN));
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_PINCH_BEGIN));

  // Double tap zoom updates should not be sent.
  // Instead, the second tap drag becomes a scroll gesture sequence.
  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY + 200);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_UPDATE));
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_PINCH_UPDATE));
  event = ObtainMotionEvent(down_time_2 + kFiveMilliseconds * 3,
                            MotionEvent::ACTION_UP,
                            kFakeCoordX,
                            kFakeCoordY + 200);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_PINCH_END));
}

// Verify that pinch zoom sends the proper event sequence.
TEST_F(GestureProviderTest, PinchZoom) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  const int scaled_touch_slop = GetTouchSlop();

  gesture_provider_->SetMultiTouchSupportEnabled(true);

  int secondary_coord_x = kFakeCoordX + 20 * scaled_touch_slop;
  int secondary_coord_y = kFakeCoordY + 20 * scaled_touch_slop;

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(event_time,
                            MotionEvent::ACTION_POINTER_DOWN,
                            kFakeCoordX,
                            kFakeCoordY,
                            secondary_coord_x,
                            secondary_coord_y);
  gesture_provider_->OnTouchEvent(event);
  EXPECT_EQ(1U, GetReceivedGestureCount());

  secondary_coord_x += 5 * scaled_touch_slop;
  secondary_coord_y += 5 * scaled_touch_slop;

  event = ObtainMotionEvent(event_time,
                            MotionEvent::ACTION_MOVE,
                            kFakeCoordX,
                            kFakeCoordY,
                            secondary_coord_x,
                            secondary_coord_y);

  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_PINCH_BEGIN));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_BEGIN));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_PINCH_UPDATE));
  EXPECT_TRUE(HasReceivedGesture(ET_GESTURE_SCROLL_UPDATE));

  event = ObtainMotionEvent(event_time,
                            MotionEvent::ACTION_POINTER_UP,
                            kFakeCoordX,
                            kFakeCoordY,
                            secondary_coord_x,
                            secondary_coord_y);

  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_PINCH_END, GetMostRecentGestureEventType());
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_SCROLL_END));

  event = ObtainMotionEvent(event_time, MotionEvent::ACTION_UP);
  gesture_provider_->OnTouchEvent(event);
  EXPECT_EQ(ET_GESTURE_SCROLL_END, GetMostRecentGestureEventType());
}

// Verify that the timer of LONG_PRESS will be cancelled when scrolling begins
// so LONG_PRESS and LONG_TAP won't be triggered.
TEST_F(GestureProviderTest, GesturesCancelledAfterLongPressCausesLostFocus) {
  base::TimeTicks event_time = base::TimeTicks::Now();

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));

  const base::TimeDelta long_press_timeout =
      GetLongPressTimeout() + GetShowPressTimeout() + kFiveMilliseconds * 2;
  Wait(long_press_timeout);
  EXPECT_EQ(ET_GESTURE_LONG_PRESS, GetMostRecentGestureEventType());

  EXPECT_TRUE(CancelActiveTouchSequence());
  EXPECT_EQ(ET_GESTURE_TAP_CANCEL, GetMostRecentGestureEventType());

  event = ObtainMotionEvent(event_time + long_press_timeout,
                            MotionEvent::ACTION_UP);
  gesture_provider_->OnTouchEvent(event);
  EXPECT_FALSE(HasReceivedGesture(ET_GESTURE_LONG_TAP));
}

// Verify that inserting a touch cancel event will trigger proper touch and
// gesture sequence cancellation.
TEST_F(GestureProviderTest, CancelActiveTouchSequence) {
  base::TimeTicks event_time = base::TimeTicks::Now();

  EXPECT_FALSE(CancelActiveTouchSequence());
  EXPECT_EQ(0U, GetReceivedGestureCount());

  MockMotionEvent event =
      ObtainMotionEvent(event_time, MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());

  ASSERT_TRUE(CancelActiveTouchSequence());
  EXPECT_EQ(ET_GESTURE_TAP_CANCEL, GetMostRecentGestureEventType());

  // Subsequent MotionEvent's are dropped until ACTION_DOWN.
  event = ObtainMotionEvent(event_time + kFiveMilliseconds,
                            MotionEvent::ACTION_MOVE);
  EXPECT_FALSE(gesture_provider_->OnTouchEvent(event));

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 2,
                            MotionEvent::ACTION_UP);
  EXPECT_FALSE(gesture_provider_->OnTouchEvent(event));

  event = ObtainMotionEvent(event_time + kFiveMilliseconds * 3,
                            MotionEvent::ACTION_DOWN);
  EXPECT_TRUE(gesture_provider_->OnTouchEvent(event));
  EXPECT_EQ(ET_GESTURE_TAP_DOWN, GetMostRecentGestureEventType());
}

}  // namespace ui
