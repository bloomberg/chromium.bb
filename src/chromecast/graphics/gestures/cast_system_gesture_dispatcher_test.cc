// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/gestures/cast_system_gesture_dispatcher.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

using testing::_;
using testing::Return;

namespace chromecast {

class MockCastGestureHandler : public CastGestureHandler {
 public:
  ~MockCastGestureHandler() override = default;

  MOCK_METHOD0(GetPriority, Priority());
  MOCK_METHOD1(CanHandleSwipe, bool(CastSideSwipeOrigin origin));
  MOCK_METHOD3(HandleSideSwipe,
               void(CastSideSwipeEvent event,
                    CastSideSwipeOrigin swipe_origin,
                    const gfx::Point& touch_location));
  MOCK_METHOD1(HandleTapDownGesture, void(const gfx::Point& touch_location));
  MOCK_METHOD1(HandleTapGesture, void(const gfx::Point& touch_location));
};

class CastSystemGestureDispatcherTest : public testing::Test {
 public:
  ~CastSystemGestureDispatcherTest() override = default;

  void SetUp() override {
    gesture_dispatcher_ = std::make_unique<CastSystemGestureDispatcher>();
  }

  void TearDown() override { gesture_dispatcher_.reset(); }

 protected:
  std::unique_ptr<CastSystemGestureDispatcher> gesture_dispatcher_;
};

TEST_F(CastSystemGestureDispatcherTest, SingleHandler) {
  MockCastGestureHandler handler;
  gesture_dispatcher_->AddGestureHandler(&handler);
  ON_CALL(handler, GetPriority())
      .WillByDefault(Return(CastGestureHandler::Priority::MAIN_ACTIVITY));

  // Test when a handler is able to handle a swipe event.
  ON_CALL(handler, CanHandleSwipe(_)).WillByDefault(Return(true));
  CastSideSwipeEvent event = CastSideSwipeEvent::BEGIN;
  CastSideSwipeOrigin origin = CastSideSwipeOrigin::TOP;
  gfx::Point point(0, 0);
  EXPECT_CALL(handler, HandleSideSwipe(event, origin, point));
  gesture_dispatcher_->HandleSideSwipe(event, origin, point);

  // Test when a handler cannot handle a swipe event.
  ON_CALL(handler, CanHandleSwipe(CastSideSwipeOrigin::BOTTOM))
      .WillByDefault(Return(false));
  EXPECT_CALL(handler, HandleSideSwipe(_, _, _)).Times(0);
  gesture_dispatcher_->HandleSideSwipe(event, CastSideSwipeOrigin::BOTTOM,
                                       point);

  gesture_dispatcher_->RemoveGestureHandler(&handler);
}

TEST_F(CastSystemGestureDispatcherTest, SingleHandlerPriorityNone) {
  MockCastGestureHandler handler;
  gesture_dispatcher_->AddGestureHandler(&handler);
  ON_CALL(handler, GetPriority())
      .WillByDefault(Return(CastGestureHandler::Priority::NONE));

  // Test when a handler is able to handle a swipe event.
  ON_CALL(handler, CanHandleSwipe(_)).WillByDefault(Return(true));
  CastSideSwipeEvent event = CastSideSwipeEvent::BEGIN;
  CastSideSwipeOrigin origin = CastSideSwipeOrigin::TOP;
  gfx::Point point(0, 0);
  // The gesture event will not be sent since the handler's priority is NONE.
  EXPECT_CALL(handler, HandleSideSwipe(_, _, _)).Times(0);
  gesture_dispatcher_->HandleSideSwipe(event, origin, point);

  gesture_dispatcher_->RemoveGestureHandler(&handler);
}

TEST_F(CastSystemGestureDispatcherTest, MultipleHandlersByPriority) {
  MockCastGestureHandler handler_1;
  MockCastGestureHandler handler_2;
  ON_CALL(handler_1, GetPriority())
      .WillByDefault(Return(CastGestureHandler::Priority::ROOT_UI));
  ON_CALL(handler_2, GetPriority())
      .WillByDefault(Return(CastGestureHandler::Priority::MAIN_ACTIVITY));

  gesture_dispatcher_->AddGestureHandler(&handler_1);
  gesture_dispatcher_->AddGestureHandler(&handler_2);

  // Higher priority handler should get the event.
  ON_CALL(handler_1, CanHandleSwipe(_)).WillByDefault(Return(true));
  ON_CALL(handler_2, CanHandleSwipe(_)).WillByDefault(Return(true));
  CastSideSwipeEvent event = CastSideSwipeEvent::BEGIN;
  CastSideSwipeOrigin origin = CastSideSwipeOrigin::TOP;
  gfx::Point point(0, 0);
  EXPECT_CALL(handler_1, HandleSideSwipe(_, _, _)).Times(0);
  EXPECT_CALL(handler_2, HandleSideSwipe(event, origin, point));
  gesture_dispatcher_->HandleSideSwipe(event, origin, point);

  // Test when the higher priority handler can't handle the event. Lower
  // priority handler should get it instead.
  origin = CastSideSwipeOrigin::BOTTOM;
  ON_CALL(handler_2, CanHandleSwipe(origin)).WillByDefault(Return(false));
  EXPECT_CALL(handler_1, HandleSideSwipe(event, origin, point));
  EXPECT_CALL(handler_2, HandleSideSwipe(_, _, _)).Times(0);
  gesture_dispatcher_->HandleSideSwipe(event, origin, point);

  // Test when no handlers can handle the event.
  ON_CALL(handler_1, CanHandleSwipe(origin)).WillByDefault(Return(false));
  EXPECT_CALL(handler_1, HandleSideSwipe(_, _, _)).Times(0);
  EXPECT_CALL(handler_2, HandleSideSwipe(_, _, _)).Times(0);
  gesture_dispatcher_->HandleSideSwipe(event, origin, point);

  gesture_dispatcher_->RemoveGestureHandler(&handler_2);
  gesture_dispatcher_->RemoveGestureHandler(&handler_1);
}

}  // namespace chromecast
