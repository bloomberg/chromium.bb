// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/PointerEventFactory.h"

#include "core/frame/FrameView.h"
#include "core/page/Page.h"
#include "public/platform/WebPointerProperties.h"
#include <climits>
#include <gtest/gtest.h>

namespace blink {

namespace {

const char* pointerTypeNameForWebPointPointerType(
    WebPointerProperties::PointerType type) {
  switch (type) {
    case WebPointerProperties::PointerType::Unknown:
      return "";
    case WebPointerProperties::PointerType::Touch:
      return "touch";
    case WebPointerProperties::PointerType::Pen:
    case WebPointerProperties::PointerType::Eraser:
      return "pen";
    case WebPointerProperties::PointerType::Mouse:
      return "mouse";
  }
  NOTREACHED();
  return "";
}
}

class PointerEventFactoryTest : public ::testing::Test {
 protected:
  void SetUp() override;
  PointerEvent* createAndCheckTouchCancel(WebPointerProperties::PointerType,
                                          int rawId,
                                          int uniqueId,
                                          bool isPrimary);
  PointerEvent* createAndCheckTouchEvent(
      WebPointerProperties::PointerType,
      int rawId,
      int uniqueId,
      bool isPrimary,
      WebTouchPoint::State = WebTouchPoint::StatePressed,
      size_t coalescedEventCount = 0);
  PointerEvent* createAndCheckMouseEvent(
      WebPointerProperties::PointerType,
      int rawId,
      int uniqueId,
      bool isPrimary,
      WebInputEvent::Modifiers = WebInputEvent::NoModifiers,
      size_t coalescedEventCount = 0);
  void createAndCheckPointerTransitionEvent(PointerEvent*, const AtomicString&);

  PointerEventFactory m_pointerEventFactory;
  unsigned m_expectedMouseId;
  unsigned m_mappedIdStart;

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
                         WebInputEvent::Modifiers);
  };
};

void PointerEventFactoryTest::SetUp() {
  m_expectedMouseId = 1;
  m_mappedIdStart = 2;
}

PointerEventFactoryTest::WebTouchPointBuilder::WebTouchPointBuilder(
    WebPointerProperties::PointerType pointerTypeParam,
    int idParam,
    WebTouchPoint::State stateParam) {
  id = idParam;
  pointerType = pointerTypeParam;
  force = 1.0;
  state = stateParam;
}

PointerEventFactoryTest::WebMouseEventBuilder::WebMouseEventBuilder(
    WebPointerProperties::PointerType pointerTypeParam,
    int idParam,
    WebInputEvent::Modifiers modifiersParam) {
  pointerType = pointerTypeParam;
  id = idParam;
  m_modifiers = modifiersParam;
  m_frameScale = 1;
}

PointerEvent* PointerEventFactoryTest::createAndCheckTouchCancel(
    WebPointerProperties::PointerType pointerType,
    int rawId,
    int uniqueId,
    bool isPrimary) {
  PointerEvent* pointerEvent =
      m_pointerEventFactory.createPointerCancelEvent(uniqueId, pointerType);
  EXPECT_EQ(uniqueId, pointerEvent->pointerId());
  EXPECT_EQ(isPrimary, pointerEvent->isPrimary());
  EXPECT_EQ(pointerTypeNameForWebPointPointerType(pointerType),
            pointerEvent->pointerType());
  return pointerEvent;
}

void PointerEventFactoryTest::createAndCheckPointerTransitionEvent(
    PointerEvent* pointerEvent,
    const AtomicString& type) {
  PointerEvent* clonePointerEvent =
      m_pointerEventFactory.createPointerBoundaryEvent(pointerEvent, type,
                                                       nullptr);
  EXPECT_EQ(clonePointerEvent->pointerType(), pointerEvent->pointerType());
  EXPECT_EQ(clonePointerEvent->pointerId(), pointerEvent->pointerId());
  EXPECT_EQ(clonePointerEvent->isPrimary(), pointerEvent->isPrimary());
  EXPECT_EQ(clonePointerEvent->type(), type);
}

PointerEvent* PointerEventFactoryTest::createAndCheckTouchEvent(
    WebPointerProperties::PointerType pointerType,
    int rawId,
    int uniqueId,
    bool isPrimary,
    WebTouchPoint::State state,
    size_t coalescedEventCount) {
  Vector<WebTouchPoint> coalescedEvents;
  for (size_t i = 0; i < coalescedEventCount; i++) {
    coalescedEvents.push_back(PointerEventFactoryTest::WebTouchPointBuilder(
        pointerType, rawId, state));
  }
  PointerEvent* pointerEvent = m_pointerEventFactory.create(
      PointerEventFactoryTest::WebTouchPointBuilder(pointerType, rawId, state),
      coalescedEvents, WebInputEvent::NoModifiers, nullptr, nullptr);
  EXPECT_EQ(uniqueId, pointerEvent->pointerId());
  EXPECT_EQ(isPrimary, pointerEvent->isPrimary());
  const char* expectedPointerType =
      pointerTypeNameForWebPointPointerType(pointerType);
  EXPECT_EQ(expectedPointerType, pointerEvent->pointerType());
  EXPECT_EQ(coalescedEventCount, pointerEvent->getCoalescedEvents().size());
  for (size_t i = 0; i < coalescedEventCount; i++) {
    EXPECT_EQ(uniqueId, pointerEvent->getCoalescedEvents()[i]->pointerId());
    EXPECT_EQ(isPrimary, pointerEvent->getCoalescedEvents()[i]->isPrimary());
    EXPECT_EQ(expectedPointerType, pointerEvent->pointerType());
  }
  return pointerEvent;
}

PointerEvent* PointerEventFactoryTest::createAndCheckMouseEvent(
    WebPointerProperties::PointerType pointerType,
    int rawId,
    int uniqueId,
    bool isPrimary,
    WebInputEvent::Modifiers modifiers,
    size_t coalescedEventCount) {
  Vector<WebMouseEvent> coalescedEvents;
  for (size_t i = 0; i < coalescedEventCount; i++) {
    coalescedEvents.push_back(PointerEventFactoryTest::WebMouseEventBuilder(
        pointerType, rawId, modifiers));
  }
  PointerEvent* pointerEvent = m_pointerEventFactory.create(
      coalescedEventCount ? EventTypeNames::mousemove
                          : EventTypeNames::mousedown,
      PointerEventFactoryTest::WebMouseEventBuilder(pointerType, rawId,
                                                    modifiers),
      coalescedEvents, nullptr);
  EXPECT_EQ(uniqueId, pointerEvent->pointerId());
  EXPECT_EQ(isPrimary, pointerEvent->isPrimary());
  const char* expectedPointerType =
      pointerTypeNameForWebPointPointerType(pointerType);
  EXPECT_EQ(expectedPointerType, pointerEvent->pointerType());
  EXPECT_EQ(coalescedEventCount, pointerEvent->getCoalescedEvents().size());
  for (size_t i = 0; i < coalescedEventCount; i++) {
    EXPECT_EQ(uniqueId, pointerEvent->getCoalescedEvents()[i]->pointerId());
    EXPECT_EQ(isPrimary, pointerEvent->getCoalescedEvents()[i]->isPrimary());
    EXPECT_EQ(expectedPointerType, pointerEvent->pointerType());
  }
  return pointerEvent;
}

TEST_F(PointerEventFactoryTest, MousePointer) {
  EXPECT_TRUE(m_pointerEventFactory.isActive(m_expectedMouseId));
  EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_expectedMouseId));

  PointerEvent* pointerEvent1 = createAndCheckMouseEvent(
      WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true);
  PointerEvent* pointerEvent2 = createAndCheckMouseEvent(
      WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true,
      WebInputEvent::LeftButtonDown);

  createAndCheckPointerTransitionEvent(pointerEvent1,
                                       EventTypeNames::pointerout);

  EXPECT_TRUE(m_pointerEventFactory.isActive(m_expectedMouseId));
  EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_expectedMouseId));

  m_pointerEventFactory.remove(pointerEvent1->pointerId());

  EXPECT_TRUE(m_pointerEventFactory.isActive(m_expectedMouseId));
  EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_expectedMouseId));

  createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0,
                           m_expectedMouseId, true);

  EXPECT_TRUE(m_pointerEventFactory.isActive(m_expectedMouseId));
  EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_expectedMouseId));

  m_pointerEventFactory.remove(pointerEvent1->pointerId());
  m_pointerEventFactory.remove(pointerEvent2->pointerId());

  createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 1,
                           m_expectedMouseId, true);
  createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 20,
                           m_expectedMouseId, true);
}

TEST_F(PointerEventFactoryTest, TouchPointerPrimaryRemovedWhileAnotherIsThere) {
  PointerEvent* pointerEvent1 = createAndCheckTouchEvent(
      WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 1,
                           m_mappedIdStart + 1, false);

  m_pointerEventFactory.remove(pointerEvent1->pointerId());

  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 2,
                           m_mappedIdStart + 2, false);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 1,
                           m_mappedIdStart + 1, false);
}

TEST_F(PointerEventFactoryTest, TouchPointerReleasedAndPressedAgain) {
  EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart));
  EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart + 1));
  EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));
  EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart + 1));

  PointerEvent* pointerEvent1 = createAndCheckTouchEvent(
      WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
  PointerEvent* pointerEvent2 = createAndCheckTouchEvent(
      WebPointerProperties::PointerType::Touch, 1, m_mappedIdStart + 1, false);

  createAndCheckPointerTransitionEvent(pointerEvent1,
                                       EventTypeNames::pointerleave);
  createAndCheckPointerTransitionEvent(pointerEvent2,
                                       EventTypeNames::pointerenter);

  EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart));
  EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart + 1));
  EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));
  EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart + 1));

  m_pointerEventFactory.remove(pointerEvent1->pointerId());
  m_pointerEventFactory.remove(pointerEvent2->pointerId());

  EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart));
  EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart + 1));
  EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));
  EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart + 1));

  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 1,
                           m_mappedIdStart + 2, true);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0,
                           m_mappedIdStart + 3, false);

  m_pointerEventFactory.clear();

  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 10,
                           m_mappedIdStart, true);
}

TEST_F(PointerEventFactoryTest, TouchAndDrag) {
  EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart));
  EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));

  PointerEvent* pointerEvent1 = createAndCheckTouchEvent(
      WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
  PointerEvent* pointerEvent2 = createAndCheckTouchEvent(
      WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);

  EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart));
  EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));

  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0,
                           m_mappedIdStart, true, WebTouchPoint::StateReleased);

  EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart));
  EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));

  m_pointerEventFactory.remove(pointerEvent1->pointerId());
  m_pointerEventFactory.remove(pointerEvent2->pointerId());

  EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart));
  EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart));

  EXPECT_FALSE(m_pointerEventFactory.isActive(m_mappedIdStart + 1));
  EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart + 1));

  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0,
                           m_mappedIdStart + 1, true);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0,
                           m_mappedIdStart + 1, true);

  // Remove an obsolete (i.e. already removed) pointer event which should have
  // no effect.
  m_pointerEventFactory.remove(pointerEvent1->pointerId());

  EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart + 1));
  EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart + 1));

  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0,
                           m_mappedIdStart + 1, true);
  createAndCheckTouchCancel(WebPointerProperties::PointerType::Touch, 0,
                            m_mappedIdStart + 1, true);

  EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart + 1));
  EXPECT_FALSE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart + 1));

  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0,
                           m_mappedIdStart + 1, true);

  EXPECT_TRUE(m_pointerEventFactory.isActive(m_mappedIdStart + 1));
  EXPECT_TRUE(m_pointerEventFactory.isActiveButtonsState(m_mappedIdStart + 1));
}

TEST_F(PointerEventFactoryTest, MouseAndTouchAndPen) {
  createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0,
                           m_expectedMouseId, true);
  PointerEvent* pointerEvent1 = createAndCheckTouchEvent(
      WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0,
                           m_mappedIdStart + 1, true);

  PointerEvent* pointerEvent2 = createAndCheckTouchEvent(
      WebPointerProperties::PointerType::Touch, 1, m_mappedIdStart + 2, false);
  PointerEvent* pointerEvent3 = createAndCheckTouchEvent(
      WebPointerProperties::PointerType::Touch, 2, m_mappedIdStart + 3, false);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0,
                           m_mappedIdStart + 1, true);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 47213,
                           m_mappedIdStart + 4, false);

  m_pointerEventFactory.remove(pointerEvent1->pointerId());
  m_pointerEventFactory.remove(pointerEvent2->pointerId());
  m_pointerEventFactory.remove(pointerEvent3->pointerId());

  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 100,
                           m_mappedIdStart + 5, true);

  m_pointerEventFactory.clear();

  createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0,
                           m_expectedMouseId, true);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0,
                           m_mappedIdStart, true);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0,
                           m_mappedIdStart + 1, true);
}

TEST_F(PointerEventFactoryTest, PenAsTouchAndMouseEvent) {
  PointerEvent* pointerEvent1 = createAndCheckMouseEvent(
      WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart, true);
  createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 1,
                           m_mappedIdStart + 1, false);
  createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 2,
                           m_mappedIdStart + 2, false);
  createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 0,
                           m_mappedIdStart, true);
  createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 1,
                           m_mappedIdStart + 1, false);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0,
                           m_mappedIdStart, true);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 1,
                           m_mappedIdStart + 1, false);

  m_pointerEventFactory.remove(pointerEvent1->pointerId());

  createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0,
                           m_mappedIdStart + 3, false);
  createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 0,
                           m_mappedIdStart + 3, false);
  createAndCheckTouchCancel(WebPointerProperties::PointerType::Pen, 0,
                            m_mappedIdStart + 3, false);

  m_pointerEventFactory.clear();

  createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 1,
                           m_mappedIdStart, true);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0,
                           m_mappedIdStart + 1, false);
  createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 1,
                           m_mappedIdStart, true);
  createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 0,
                           m_mappedIdStart + 1, false);
  createAndCheckTouchCancel(WebPointerProperties::PointerType::Pen, 1,
                            m_mappedIdStart, true);
  createAndCheckTouchCancel(WebPointerProperties::PointerType::Pen, 0,
                            m_mappedIdStart + 1, false);
}

TEST_F(PointerEventFactoryTest, OutOfRange) {
  PointerEvent* pointerEvent1 = createAndCheckMouseEvent(
      WebPointerProperties::PointerType::Unknown, 0, m_mappedIdStart, true);
  createAndCheckMouseEvent(WebPointerProperties::PointerType::Unknown, 1,
                           m_mappedIdStart + 1, false);
  createAndCheckMouseEvent(WebPointerProperties::PointerType::Unknown, 2,
                           m_mappedIdStart + 2, false);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Unknown, 0,
                           m_mappedIdStart, true);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Unknown, 3,
                           m_mappedIdStart + 3, false);
  createAndCheckMouseEvent(WebPointerProperties::PointerType::Unknown, 2,
                           m_mappedIdStart + 2, false);
  createAndCheckTouchCancel(WebPointerProperties::PointerType::Unknown, 3,
                            m_mappedIdStart + 3, false);

  m_pointerEventFactory.remove(pointerEvent1->pointerId());

  createAndCheckTouchEvent(WebPointerProperties::PointerType::Unknown, 0,
                           m_mappedIdStart + 4, false);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Unknown, INT_MAX,
                           m_mappedIdStart + 5, false);

  m_pointerEventFactory.clear();

  for (int i = 0; i < 100; ++i) {
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Touch, i,
                             m_mappedIdStart + i, i == 0);
  }

  for (int i = 0; i < 100; ++i) {
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Mouse, i,
                             m_expectedMouseId, true);
  }
  createAndCheckTouchCancel(WebPointerProperties::PointerType::Mouse, 0,
                            m_expectedMouseId, true);
}

TEST_F(PointerEventFactoryTest, CoalescedEvents) {
  createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0,
                           m_expectedMouseId, true, WebInputEvent::NoModifiers,
                           4);
  createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0,
                           m_mappedIdStart, true, WebTouchPoint::StateMoved, 3);
}

}  // namespace blink
