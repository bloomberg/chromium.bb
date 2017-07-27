// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/PointerEventFactory.h"

#include <gtest/gtest.h>
#include <climits>
#include "core/frame/LocalFrameView.h"
#include "core/page/Page.h"
#include "public/platform/WebPointerProperties.h"

namespace blink {

namespace {

const char* PointerTypeNameForWebPointPointerType(
    WebPointerProperties::PointerType type) {
  switch (type) {
    case WebPointerProperties::PointerType::kUnknown:
      return "";
    case WebPointerProperties::PointerType::kTouch:
      return "touch";
    case WebPointerProperties::PointerType::kPen:
      return "pen";
    case WebPointerProperties::PointerType::kMouse:
      return "mouse";
    default:
      NOTREACHED();
      return "";
  }
}
}

class PointerEventFactoryTest : public ::testing::Test {
 protected:
  void SetUp() override;
  PointerEvent* CreateAndCheckTouchCancel(WebPointerProperties::PointerType,
                                          int raw_id,
                                          int unique_id,
                                          bool is_primary);
  PointerEvent* CreateAndCheckTouchEvent(
      WebPointerProperties::PointerType,
      int raw_id,
      int unique_id,
      bool is_primary,
      WebTouchPoint::State = WebTouchPoint::kStatePressed,
      size_t coalesced_event_count = 0);
  PointerEvent* CreateAndCheckMouseEvent(
      WebPointerProperties::PointerType,
      int raw_id,
      int unique_id,
      bool is_primary,
      WebInputEvent::Modifiers = WebInputEvent::kNoModifiers,
      size_t coalesced_event_count = 0);
  PointerEvent* CreateAndCheckPointerCancelEvent(
      WebPointerProperties::PointerType,
      int raw_id,
      int unique_id,
      bool is_primary,
      WebInputEvent::Modifiers = WebInputEvent::kNoModifiers);
  void CreateAndCheckPointerTransitionEvent(PointerEvent*, const AtomicString&);

  PointerEventFactory pointer_event_factory_;
  unsigned expected_mouse_id_;
  unsigned mapped_id_start_;

  class WebTouchPointBuilder : public WebTouchPoint {
   public:
    WebTouchPointBuilder(WebPointerProperties::PointerType,
                         int,
                         WebTouchPoint::State);
  };

  class WebMouseEventBuilder : public WebMouseEvent {
   public:
    WebMouseEventBuilder(WebPointerProperties::PointerType,
                         int,
                         WebInputEvent::Modifiers,
                         double);
  };
};

void PointerEventFactoryTest::SetUp() {
  expected_mouse_id_ = 1;
  mapped_id_start_ = 2;
}

PointerEventFactoryTest::WebTouchPointBuilder::WebTouchPointBuilder(
    WebPointerProperties::PointerType pointer_type_param,
    int id_param,
    WebTouchPoint::State state_param) {
  id = id_param;
  pointer_type = pointer_type_param;
  force = 1.0;
  state = state_param;
}

PointerEventFactoryTest::WebMouseEventBuilder::WebMouseEventBuilder(
    WebPointerProperties::PointerType pointer_type_param,
    int id_param,
    WebInputEvent::Modifiers modifiers_param,
    double platform_time_stamp) {
  pointer_type = pointer_type_param;
  id = id_param;
  modifiers_ = modifiers_param;
  frame_scale_ = 1;
  time_stamp_seconds_ = platform_time_stamp;
}

PointerEvent* PointerEventFactoryTest::CreateAndCheckTouchCancel(
    WebPointerProperties::PointerType pointer_type,
    int raw_id,
    int unique_id,
    bool is_primary) {
  TimeTicks now = TimeTicks::Now();
  PointerEvent* pointer_event = pointer_event_factory_.CreatePointerCancelEvent(
      unique_id, pointer_type, now);
  EXPECT_EQ(unique_id, pointer_event->pointerId());
  EXPECT_EQ(is_primary, pointer_event->isPrimary());
  EXPECT_EQ(PointerTypeNameForWebPointPointerType(pointer_type),
            pointer_event->pointerType());
  EXPECT_EQ(now, pointer_event->PlatformTimeStamp());
  return pointer_event;
}

void PointerEventFactoryTest::CreateAndCheckPointerTransitionEvent(
    PointerEvent* pointer_event,
    const AtomicString& type) {
  PointerEvent* clone_pointer_event =
      pointer_event_factory_.CreatePointerBoundaryEvent(pointer_event, type,
                                                        nullptr);
  EXPECT_EQ(clone_pointer_event->pointerType(), pointer_event->pointerType());
  EXPECT_EQ(clone_pointer_event->pointerId(), pointer_event->pointerId());
  EXPECT_EQ(clone_pointer_event->isPrimary(), pointer_event->isPrimary());
  EXPECT_EQ(clone_pointer_event->type(), type);
}

PointerEvent* PointerEventFactoryTest::CreateAndCheckTouchEvent(
    WebPointerProperties::PointerType pointer_type,
    int raw_id,
    int unique_id,
    bool is_primary,
    WebTouchPoint::State state,
    size_t coalesced_event_count) {
  Vector<std::pair<WebTouchPoint, TimeTicks>> coalesced_events;
  TimeTicks now = TimeTicks::Now();
  for (size_t i = 0; i < coalesced_event_count; i++) {
    coalesced_events.push_back(std::pair<WebTouchPoint, TimeTicks>(
        PointerEventFactoryTest::WebTouchPointBuilder(pointer_type, raw_id,
                                                      state),
        now));
  }
  PointerEvent* pointer_event = pointer_event_factory_.Create(
      PointerEventFactoryTest::WebTouchPointBuilder(pointer_type, raw_id,
                                                    state),
      coalesced_events, WebInputEvent::kNoModifiers, now, nullptr, nullptr);
  EXPECT_EQ(unique_id, pointer_event->pointerId());
  EXPECT_EQ(is_primary, pointer_event->isPrimary());
  EXPECT_EQ(now, pointer_event->PlatformTimeStamp());
  const char* expected_pointer_type =
      PointerTypeNameForWebPointPointerType(pointer_type);
  EXPECT_EQ(expected_pointer_type, pointer_event->pointerType());
  EXPECT_EQ(coalesced_event_count, pointer_event->getCoalescedEvents().size());
  for (size_t i = 0; i < coalesced_event_count; i++) {
    EXPECT_EQ(unique_id, pointer_event->getCoalescedEvents()[i]->pointerId());
    EXPECT_EQ(is_primary, pointer_event->getCoalescedEvents()[i]->isPrimary());
    EXPECT_EQ(expected_pointer_type, pointer_event->pointerType());
    EXPECT_EQ(now, pointer_event->PlatformTimeStamp());
  }
  return pointer_event;
}

PointerEvent* PointerEventFactoryTest::CreateAndCheckPointerCancelEvent(
    WebPointerProperties::PointerType pointer_type,
    int raw_id,
    int unique_id,
    bool is_primary,
    WebInputEvent::Modifiers modifiers) {
  PointerEvent* pointer_event = pointer_event_factory_.CreatePointerCancelEvent(
      WebPointerEvent(WebInputEvent::Type::kPointerCancel,
                      PointerEventFactoryTest::WebMouseEventBuilder(
                          pointer_type, raw_id, modifiers,
                          WebInputEvent::kTimeStampForTesting)));
  EXPECT_EQ("pointercancel", pointer_event->type());
  EXPECT_EQ(unique_id, pointer_event->pointerId());
  EXPECT_EQ(is_primary, pointer_event->isPrimary());
  EXPECT_EQ(TimeTicks::FromSeconds(WebInputEvent::kTimeStampForTesting),
            pointer_event->PlatformTimeStamp());
  const char* expected_pointer_type =
      PointerTypeNameForWebPointPointerType(pointer_type);
  EXPECT_EQ(expected_pointer_type, pointer_event->pointerType());
  return pointer_event;
}

PointerEvent* PointerEventFactoryTest::CreateAndCheckMouseEvent(
    WebPointerProperties::PointerType pointer_type,
    int raw_id,
    int unique_id,
    bool is_primary,
    WebInputEvent::Modifiers modifiers,
    size_t coalesced_event_count) {
  Vector<WebMouseEvent> coalesced_events;
  for (size_t i = 0; i < coalesced_event_count; i++) {
    coalesced_events.push_back(PointerEventFactoryTest::WebMouseEventBuilder(
        pointer_type, raw_id, modifiers,
        WebInputEvent::kTimeStampForTesting + i));
  }
  PointerEvent* pointer_event = pointer_event_factory_.Create(
      coalesced_event_count ? EventTypeNames::mousemove
                            : EventTypeNames::mousedown,
      PointerEventFactoryTest::WebMouseEventBuilder(
          pointer_type, raw_id, modifiers, WebInputEvent::kTimeStampForTesting),
      coalesced_events, nullptr);
  EXPECT_EQ(unique_id, pointer_event->pointerId());
  EXPECT_EQ(is_primary, pointer_event->isPrimary());
  EXPECT_EQ(TimeTicks::FromSeconds(WebInputEvent::kTimeStampForTesting),
            pointer_event->PlatformTimeStamp());
  const char* expected_pointer_type =
      PointerTypeNameForWebPointPointerType(pointer_type);
  EXPECT_EQ(expected_pointer_type, pointer_event->pointerType());
  EXPECT_EQ(coalesced_event_count, pointer_event->getCoalescedEvents().size());
  for (size_t i = 0; i < coalesced_event_count; i++) {
    EXPECT_EQ(unique_id, pointer_event->getCoalescedEvents()[i]->pointerId());
    EXPECT_EQ(is_primary, pointer_event->getCoalescedEvents()[i]->isPrimary());
    EXPECT_EQ(expected_pointer_type, pointer_event->pointerType());
    EXPECT_EQ(TimeTicks::FromSeconds(WebInputEvent::kTimeStampForTesting + i),
              pointer_event->getCoalescedEvents()[i]->PlatformTimeStamp());
  }
  return pointer_event;
}

TEST_F(PointerEventFactoryTest, MousePointer) {
  EXPECT_TRUE(pointer_event_factory_.IsActive(expected_mouse_id_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(expected_mouse_id_));

  PointerEvent* pointer_event1 = CreateAndCheckMouseEvent(
      WebPointerProperties::PointerType::kMouse, 0, expected_mouse_id_, true);
  PointerEvent* pointer_event2 = CreateAndCheckMouseEvent(
      WebPointerProperties::PointerType::kMouse, 0, expected_mouse_id_, true,
      WebInputEvent::kLeftButtonDown);

  CreateAndCheckPointerTransitionEvent(pointer_event1,
                                       EventTypeNames::pointerout);

  EXPECT_TRUE(pointer_event_factory_.IsActive(expected_mouse_id_));
  EXPECT_TRUE(pointer_event_factory_.IsActiveButtonsState(expected_mouse_id_));

  pointer_event_factory_.Remove(pointer_event1->pointerId());

  EXPECT_TRUE(pointer_event_factory_.IsActive(expected_mouse_id_));
  EXPECT_TRUE(pointer_event_factory_.IsActiveButtonsState(expected_mouse_id_));

  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kMouse, 0,
                           expected_mouse_id_, true);

  EXPECT_TRUE(pointer_event_factory_.IsActive(expected_mouse_id_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(expected_mouse_id_));

  pointer_event_factory_.Remove(pointer_event1->pointerId());
  pointer_event_factory_.Remove(pointer_event2->pointerId());

  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kMouse, 1,
                           expected_mouse_id_, true);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kMouse, 20,
                           expected_mouse_id_, true);

  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kMouse, 0,
                           expected_mouse_id_, true,
                           WebInputEvent::kLeftButtonDown);

  EXPECT_TRUE(pointer_event_factory_.IsActive(expected_mouse_id_));
  EXPECT_TRUE(pointer_event_factory_.IsActiveButtonsState(expected_mouse_id_));

  CreateAndCheckPointerCancelEvent(WebPointerProperties::PointerType::kMouse, 0,
                                   expected_mouse_id_, true);

  EXPECT_TRUE(pointer_event_factory_.IsActive(expected_mouse_id_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(expected_mouse_id_));
}

TEST_F(PointerEventFactoryTest, TouchPointerPrimaryRemovedWhileAnotherIsThere) {
  PointerEvent* pointer_event1 = CreateAndCheckTouchEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_, true);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 1,
                           mapped_id_start_ + 1, false);

  pointer_event_factory_.Remove(pointer_event1->pointerId());

  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 2,
                           mapped_id_start_ + 2, false);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 1,
                           mapped_id_start_ + 1, false);
}

TEST_F(PointerEventFactoryTest, TouchPointerReleasedAndPressedAgain) {
  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));
  EXPECT_FALSE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  PointerEvent* pointer_event1 = CreateAndCheckTouchEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_, true);
  PointerEvent* pointer_event2 =
      CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 1,
                               mapped_id_start_ + 1, false);

  CreateAndCheckPointerTransitionEvent(pointer_event1,
                                       EventTypeNames::pointerleave);
  CreateAndCheckPointerTransitionEvent(pointer_event2,
                                       EventTypeNames::pointerenter);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_TRUE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));
  EXPECT_TRUE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  pointer_event_factory_.Remove(pointer_event1->pointerId());
  pointer_event_factory_.Remove(pointer_event2->pointerId());

  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));
  EXPECT_FALSE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 1,
                           mapped_id_start_ + 2, true);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 0,
                           mapped_id_start_ + 3, false);

  pointer_event_factory_.Clear();

  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 10,
                           mapped_id_start_, true);
}

TEST_F(PointerEventFactoryTest, TouchAndDrag) {
  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));

  PointerEvent* pointer_event1 = CreateAndCheckTouchEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_, true);
  PointerEvent* pointer_event2 = CreateAndCheckTouchEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_, true);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_TRUE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));

  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 0,
                           mapped_id_start_, true,
                           WebTouchPoint::kStateReleased);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));

  pointer_event_factory_.Remove(pointer_event1->pointerId());
  pointer_event_factory_.Remove(pointer_event2->pointerId());

  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));

  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_FALSE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 0,
                           mapped_id_start_ + 1, true);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 0,
                           mapped_id_start_ + 1, true);

  // Remove an obsolete (i.e. already removed) pointer event which should have
  // no effect.
  pointer_event_factory_.Remove(pointer_event1->pointerId());

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_TRUE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 0,
                           mapped_id_start_ + 1, true);
  CreateAndCheckTouchCancel(WebPointerProperties::PointerType::kTouch, 0,
                            mapped_id_start_ + 1, true);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_FALSE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 0,
                           mapped_id_start_ + 1, true);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_TRUE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));
}

TEST_F(PointerEventFactoryTest, MouseAndTouchAndPen) {
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kMouse, 0,
                           expected_mouse_id_, true);
  PointerEvent* pointer_event1 = CreateAndCheckTouchEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_, true);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kPen, 0,
                           mapped_id_start_ + 1, true);

  PointerEvent* pointer_event2 =
      CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 1,
                               mapped_id_start_ + 2, false);
  PointerEvent* pointer_event3 =
      CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 2,
                               mapped_id_start_ + 3, false);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kPen, 0,
                           mapped_id_start_ + 1, true);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kPen, 47213,
                           mapped_id_start_ + 4, false);

  pointer_event_factory_.Remove(pointer_event1->pointerId());
  pointer_event_factory_.Remove(pointer_event2->pointerId());
  pointer_event_factory_.Remove(pointer_event3->pointerId());

  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 100,
                           mapped_id_start_ + 5, true);

  pointer_event_factory_.Clear();

  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kMouse, 0,
                           expected_mouse_id_, true);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 0,
                           mapped_id_start_, true);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kPen, 0,
                           mapped_id_start_ + 1, true);
}

TEST_F(PointerEventFactoryTest, PenAsTouchAndMouseEvent) {
  PointerEvent* pointer_event1 = CreateAndCheckMouseEvent(
      WebPointerProperties::PointerType::kPen, 0, mapped_id_start_, true);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kPen, 1,
                           mapped_id_start_ + 1, false);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kPen, 2,
                           mapped_id_start_ + 2, false);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kPen, 0,
                           mapped_id_start_, true);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kPen, 1,
                           mapped_id_start_ + 1, false);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kPen, 0,
                           mapped_id_start_, true);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kPen, 1,
                           mapped_id_start_ + 1, false);

  pointer_event_factory_.Remove(pointer_event1->pointerId());

  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kPen, 0,
                           mapped_id_start_ + 3, false);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kPen, 0,
                           mapped_id_start_ + 3, false);
  CreateAndCheckTouchCancel(WebPointerProperties::PointerType::kPen, 0,
                            mapped_id_start_ + 3, false);

  pointer_event_factory_.Clear();

  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kPen, 1,
                           mapped_id_start_, true);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kPen, 0,
                           mapped_id_start_ + 1, false);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kPen, 1,
                           mapped_id_start_, true);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kPen, 0,
                           mapped_id_start_ + 1, false);
  CreateAndCheckTouchCancel(WebPointerProperties::PointerType::kPen, 1,
                            mapped_id_start_, true);
  CreateAndCheckTouchCancel(WebPointerProperties::PointerType::kPen, 0,
                            mapped_id_start_ + 1, false);
}

TEST_F(PointerEventFactoryTest, OutOfRange) {
  PointerEvent* pointer_event1 = CreateAndCheckMouseEvent(
      WebPointerProperties::PointerType::kUnknown, 0, mapped_id_start_, true);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kUnknown, 1,
                           mapped_id_start_ + 1, false);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kUnknown, 2,
                           mapped_id_start_ + 2, false);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kUnknown, 0,
                           mapped_id_start_, true);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kUnknown, 3,
                           mapped_id_start_ + 3, false);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kUnknown, 2,
                           mapped_id_start_ + 2, false);
  CreateAndCheckTouchCancel(WebPointerProperties::PointerType::kUnknown, 3,
                            mapped_id_start_ + 3, false);

  pointer_event_factory_.Remove(pointer_event1->pointerId());

  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kUnknown, 0,
                           mapped_id_start_ + 4, false);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kUnknown, INT_MAX,
                           mapped_id_start_ + 5, false);

  pointer_event_factory_.Clear();

  for (int i = 0; i < 100; ++i) {
    CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kTouch, i,
                             mapped_id_start_ + i, i == 0);
  }

  for (int i = 0; i < 100; ++i) {
    CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kMouse, i,
                             expected_mouse_id_, true);
  }
  CreateAndCheckTouchCancel(WebPointerProperties::PointerType::kMouse, 0,
                            expected_mouse_id_, true);
}

TEST_F(PointerEventFactoryTest, CoalescedEvents) {
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kMouse, 0,
                           expected_mouse_id_, true,
                           WebInputEvent::kNoModifiers, 4);
  CreateAndCheckTouchEvent(WebPointerProperties::PointerType::kTouch, 0,
                           mapped_id_start_, true, WebTouchPoint::kStateMoved,
                           3);
}

}  // namespace blink
