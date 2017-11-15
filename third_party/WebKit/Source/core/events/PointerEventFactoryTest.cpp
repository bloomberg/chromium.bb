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
  PointerEvent* CreateAndCheckPointerCancel(WebPointerProperties::PointerType,
                                            int raw_id,
                                            int unique_id,
                                            bool is_primary);
  PointerEvent* CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType pointer_type,
      int raw_id,
      int unique_id,
      bool is_primary,
      WebInputEvent::Type type = WebInputEvent::kPointerDown,
      size_t coalesced_event_count = 0) {
    WebPointerEvent web_pointer_event;
    web_pointer_event.pointer_type = pointer_type;
    web_pointer_event.id = raw_id;
    web_pointer_event.SetType(type);
    web_pointer_event.SetTimeStampSeconds(WebInputEvent::kTimeStampForTesting);
    web_pointer_event.force = 1.0;
    Vector<WebPointerEvent> coalesced_events;
    for (size_t i = 0; i < coalesced_event_count; i++) {
      coalesced_events.push_back(web_pointer_event);
    }
    PointerEvent* pointer_event = pointer_event_factory_.Create(
        web_pointer_event, coalesced_events, nullptr);
    EXPECT_EQ(unique_id, pointer_event->pointerId());
    EXPECT_EQ(is_primary, pointer_event->isPrimary());
    EXPECT_EQ(TimeTicks::FromSeconds(WebInputEvent::kTimeStampForTesting),
              pointer_event->PlatformTimeStamp());
    const char* expected_pointer_type =
        PointerTypeNameForWebPointPointerType(pointer_type);
    EXPECT_EQ(expected_pointer_type, pointer_event->pointerType());
    EXPECT_EQ(coalesced_event_count,
              pointer_event->getCoalescedEvents().size());
    for (size_t i = 0; i < coalesced_event_count; i++) {
      EXPECT_EQ(unique_id, pointer_event->getCoalescedEvents()[i]->pointerId());
      EXPECT_EQ(is_primary,
                pointer_event->getCoalescedEvents()[i]->isPrimary());
      EXPECT_EQ(expected_pointer_type, pointer_event->pointerType());
      EXPECT_EQ(TimeTicks::FromSeconds(WebInputEvent::kTimeStampForTesting),
                pointer_event->PlatformTimeStamp());
    }
    return pointer_event;
  }
  PointerEvent* CreateAndCheckMouseEvent(
      WebPointerProperties::PointerType,
      int raw_id,
      int unique_id,
      bool is_primary,
      WebInputEvent::Modifiers = WebInputEvent::kNoModifiers,
      size_t coalesced_event_count = 0);
  void CreateAndCheckPointerTransitionEvent(PointerEvent*, const AtomicString&);
  void CheckScrollCapablePointers(const std::set<int>& expected);

  PointerEventFactory pointer_event_factory_;
  int expected_mouse_id_;
  int mapped_id_start_;

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

PointerEvent* PointerEventFactoryTest::CreateAndCheckPointerCancel(
    WebPointerProperties::PointerType pointer_type,
    int raw_id,
    int unique_id,
    bool is_primary) {
  PointerEvent* pointer_event = pointer_event_factory_.CreatePointerCancelEvent(
      unique_id, TimeTicks::FromSeconds(WebInputEvent::kTimeStampForTesting));
  EXPECT_EQ("pointercancel", pointer_event->type());
  EXPECT_EQ(unique_id, pointer_event->pointerId());
  EXPECT_EQ(is_primary, pointer_event->isPrimary());
  EXPECT_EQ(PointerTypeNameForWebPointPointerType(pointer_type),
            pointer_event->pointerType());
  EXPECT_EQ(TimeTicks::FromSeconds(WebInputEvent::kTimeStampForTesting),
            pointer_event->PlatformTimeStamp());

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

void PointerEventFactoryTest::CheckScrollCapablePointers(
    const std::set<int>& expected_pointers) {
  Vector<int> pointers =
      pointer_event_factory_.GetPointerIdsOfScrollCapablePointers();
  EXPECT_EQ(pointers.size(), expected_pointers.size());
  for (int p : pointers) {
    EXPECT_TRUE(expected_pointers.find(p) != expected_pointers.end());
  }
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

  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kMouse, 0,
                              expected_mouse_id_, true);

  EXPECT_TRUE(pointer_event_factory_.IsActive(expected_mouse_id_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(expected_mouse_id_));
}

TEST_F(PointerEventFactoryTest, TouchPointerPrimaryRemovedWhileAnotherIsThere) {
  PointerEvent* pointer_event1 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_, true);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 1,
                                mapped_id_start_ + 1, false);

  pointer_event_factory_.Remove(pointer_event1->pointerId());

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 2,
                                mapped_id_start_ + 2, false);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 1,
                                mapped_id_start_ + 1, false);
}

TEST_F(PointerEventFactoryTest, TouchPointerReleasedAndPressedAgain) {
  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));
  EXPECT_FALSE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  PointerEvent* pointer_event1 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_, true);
  PointerEvent* pointer_event2 =
      CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch,
                                    1, mapped_id_start_ + 1, false);

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

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 1,
                                mapped_id_start_ + 2, true);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_ + 3, false);

  pointer_event_factory_.Clear();

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 10,
                                mapped_id_start_, true);
}

TEST_F(PointerEventFactoryTest, TouchAndDrag) {
  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));

  PointerEvent* pointer_event1 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_, true);
  PointerEvent* pointer_event2 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_, true);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_TRUE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_, true,
                                WebInputEvent::kPointerUp);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));

  pointer_event_factory_.Remove(pointer_event1->pointerId());
  pointer_event_factory_.Remove(pointer_event2->pointerId());

  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_));
  EXPECT_FALSE(pointer_event_factory_.IsActiveButtonsState(mapped_id_start_));

  EXPECT_FALSE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_FALSE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_ + 1, true);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_ + 1, true);

  // Remove an obsolete (i.e. already removed) pointer event which should have
  // no effect.
  pointer_event_factory_.Remove(pointer_event1->pointerId());

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_TRUE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_ + 1, true);
  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kTouch, 0,
                              mapped_id_start_ + 1, true);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_FALSE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_ + 1, true);

  EXPECT_TRUE(pointer_event_factory_.IsActive(mapped_id_start_ + 1));
  EXPECT_TRUE(
      pointer_event_factory_.IsActiveButtonsState(mapped_id_start_ + 1));
}

TEST_F(PointerEventFactoryTest, MouseAndTouchAndPen) {
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kMouse, 0,
                           expected_mouse_id_, true);
  PointerEvent* pointer_event1 = CreateAndCheckWebPointerEvent(
      WebPointerProperties::PointerType::kTouch, 0, mapped_id_start_, true);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 1, true);

  PointerEvent* pointer_event2 =
      CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch,
                                    1, mapped_id_start_ + 2, false);
  PointerEvent* pointer_event3 =
      CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch,
                                    2, mapped_id_start_ + 3, false);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 1, true);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 47213,
                                mapped_id_start_ + 4, false);

  pointer_event_factory_.Remove(pointer_event1->pointerId());
  pointer_event_factory_.Remove(pointer_event2->pointerId());
  pointer_event_factory_.Remove(pointer_event3->pointerId());

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 100,
                                mapped_id_start_ + 5, true);

  pointer_event_factory_.Clear();

  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kMouse, 0,
                           expected_mouse_id_, true);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_, true);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 1, true);
}

TEST_F(PointerEventFactoryTest, ScrollCapablePointers) {
  CheckScrollCapablePointers({});

  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kMouse, 0,
                           expected_mouse_id_, true);
  PointerEvent* pointer_event1 = CreateAndCheckMouseEvent(
      WebPointerProperties::PointerType::kPen, 0, mapped_id_start_, true);
  CheckScrollCapablePointers({});

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_, true);
  CheckScrollCapablePointers({mapped_id_start_});

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_ + 1, true);
  CheckScrollCapablePointers({mapped_id_start_, mapped_id_start_ + 1});

  pointer_event_factory_.Remove(pointer_event1->pointerId());
  CheckScrollCapablePointers({mapped_id_start_ + 1});

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 1,
                                mapped_id_start_ + 2, false);

  CheckScrollCapablePointers({mapped_id_start_ + 1, mapped_id_start_ + 2});

  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kTouch, 1,
                           mapped_id_start_ + 2, false);

  CheckScrollCapablePointers({mapped_id_start_ + 1});

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 3, true);

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 1,
                                mapped_id_start_ + 4, false);

  CheckScrollCapablePointers(
      {mapped_id_start_ + 1, mapped_id_start_ + 3, mapped_id_start_ + 4});

  pointer_event_factory_.Clear();
  CheckScrollCapablePointers({});
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
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_, true);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 1,
                                mapped_id_start_ + 1, false);

  pointer_event_factory_.Remove(pointer_event1->pointerId());

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 3, false);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kPen, 0,
                           mapped_id_start_ + 3, false);
  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kPen, 0,
                              mapped_id_start_ + 3, false);

  pointer_event_factory_.Clear();

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 1,
                                mapped_id_start_, true);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kPen, 0,
                                mapped_id_start_ + 1, false);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kPen, 1,
                           mapped_id_start_, true);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kPen, 0,
                           mapped_id_start_ + 1, false);
  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kPen, 1,
                              mapped_id_start_, true);
  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kPen, 0,
                              mapped_id_start_ + 1, false);
}

TEST_F(PointerEventFactoryTest, OutOfRange) {
  PointerEvent* pointer_event1 = CreateAndCheckMouseEvent(
      WebPointerProperties::PointerType::kUnknown, 0, mapped_id_start_, true);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kUnknown, 1,
                           mapped_id_start_ + 1, false);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kUnknown, 2,
                           mapped_id_start_ + 2, false);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kUnknown, 0,
                                mapped_id_start_, true);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kUnknown, 3,
                                mapped_id_start_ + 3, false);
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kUnknown, 2,
                           mapped_id_start_ + 2, false);
  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kUnknown, 3,
                              mapped_id_start_ + 3, false);

  pointer_event_factory_.Remove(pointer_event1->pointerId());

  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kUnknown, 0,
                                mapped_id_start_ + 4, false);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kUnknown,
                                INT_MAX, mapped_id_start_ + 5, false);

  pointer_event_factory_.Clear();

  for (int i = 0; i < 100; ++i) {
    CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kTouch, i,
                             mapped_id_start_ + i, i == 0);
  }

  for (int i = 0; i < 100; ++i) {
    CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kMouse, i,
                                  expected_mouse_id_, true);
  }
  CreateAndCheckPointerCancel(WebPointerProperties::PointerType::kMouse, 0,
                              expected_mouse_id_, true);
}

TEST_F(PointerEventFactoryTest, CoalescedEvents) {
  CreateAndCheckMouseEvent(WebPointerProperties::PointerType::kMouse, 0,
                           expected_mouse_id_, true,
                           WebInputEvent::kNoModifiers, 4);
  CreateAndCheckWebPointerEvent(WebPointerProperties::PointerType::kTouch, 0,
                                mapped_id_start_, true,
                                WebInputEvent::kPointerMove, 3);
}

}  // namespace blink
