// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/PointerEventManager.h"

#include "core/frame/FrameView.h"
#include "core/page/Page.h"
#include "public/platform/WebPointerProperties.h"
#include <climits>
#include <gtest/gtest.h>

namespace blink {

class PointerEventManagerTest : public ::testing::Test {
protected:
    void SetUp() override;
    PassRefPtrWillBeRawPtr<PointerEvent> createAndCheckTouchCancel(
        WebPointerProperties::PointerType, int rawId,
        int uniqueId, bool isPrimary);
    PassRefPtrWillBeRawPtr<PointerEvent> createAndCheckTouchEvent(
        WebPointerProperties::PointerType, int rawId,
        int uniqueId, bool isPrimary);
    PassRefPtrWillBeRawPtr<PointerEvent> createAndCheckMouseEvent(
        WebPointerProperties::PointerType, int rawId,
        int uniqueId, bool isPrimary);

    PointerEventManager m_pointerEventManager;
    unsigned m_expectedMouseId;
    unsigned m_mappedIdStart;

    class PlatformTouchPointBuilder : public PlatformTouchPoint {
    public:
        PlatformTouchPointBuilder(WebPointerProperties::PointerType, int);
    };

    class PlatformMouseEventBuilder : public PlatformMouseEvent {
    public:
        PlatformMouseEventBuilder(WebPointerProperties::PointerType, int);
    };
};

void PointerEventManagerTest::SetUp()
{
    m_expectedMouseId = 1;
    m_mappedIdStart = 2;

}

PointerEventManagerTest::PlatformTouchPointBuilder::PlatformTouchPointBuilder(
    WebPointerProperties::PointerType pointerType, int id)
{
    m_pointerProperties.id = id;
    m_pointerProperties.pointerType = pointerType;
    m_pointerProperties.force = 1.0;
}

PointerEventManagerTest::PlatformMouseEventBuilder::PlatformMouseEventBuilder(
    WebPointerProperties::PointerType pointerType, int id)
{
    m_pointerProperties.pointerType = pointerType;
    m_pointerProperties.id = id;
}

PassRefPtrWillBeRawPtr<PointerEvent> PointerEventManagerTest::createAndCheckTouchCancel(
    WebPointerProperties::PointerType pointerType, int rawId,
    int uniqueId, bool isPrimary)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent = m_pointerEventManager.createPointerCancel(
        PointerEventManagerTest::PlatformTouchPointBuilder(pointerType, rawId));
    EXPECT_EQ(uniqueId, pointerEvent->pointerId());
    EXPECT_EQ(isPrimary, pointerEvent->isPrimary());
    return pointerEvent;
}

PassRefPtrWillBeRawPtr<PointerEvent> PointerEventManagerTest::createAndCheckTouchEvent(
    WebPointerProperties::PointerType pointerType, int rawId,
    int uniqueId, bool isPrimary)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent = m_pointerEventManager.create(
        EventTypeNames::pointerdown, PointerEventManagerTest::PlatformTouchPointBuilder(pointerType, rawId), PlatformEvent::NoModifiers, 0, 0, 0, 0);
    EXPECT_EQ(uniqueId, pointerEvent->pointerId());
    EXPECT_EQ(isPrimary, pointerEvent->isPrimary());
    return pointerEvent;
}

PassRefPtrWillBeRawPtr<PointerEvent> PointerEventManagerTest::createAndCheckMouseEvent(
    WebPointerProperties::PointerType pointerType, int rawId, int uniqueId, bool isPrimary)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent = m_pointerEventManager.create(
        EventTypeNames::pointerenter, PlatformMouseEventBuilder(pointerType, rawId), nullptr, nullptr);
    EXPECT_EQ(uniqueId, pointerEvent->pointerId());
    EXPECT_EQ(isPrimary, pointerEvent->isPrimary());
    return pointerEvent;
}

TEST_F(PointerEventManagerTest, MousePointer)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent1 = createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true);
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent2 = createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true);

    m_pointerEventManager.remove(pointerEvent1);

    createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true);

    m_pointerEventManager.remove(pointerEvent1);
    m_pointerEventManager.remove(pointerEvent2);

    createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 1, m_expectedMouseId, true);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 20, m_expectedMouseId, true);
}

TEST_F(PointerEventManagerTest, TouchPointerPrimaryRemovedWhileAnotherIsThere)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent1 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 1, m_mappedIdStart+1, false);

    m_pointerEventManager.remove(pointerEvent1);

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 2, m_mappedIdStart+2, false);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 1, m_mappedIdStart+1, false);
}

TEST_F(PointerEventManagerTest, TouchPointerReleasedAndPressedAgain)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent1 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent2 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 1, m_mappedIdStart+1, false);

    m_pointerEventManager.remove(pointerEvent1);
    m_pointerEventManager.remove(pointerEvent2);

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 1, m_mappedIdStart+2, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart+3, false);

    m_pointerEventManager.clear();

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 10, m_mappedIdStart, true);
}

TEST_F(PointerEventManagerTest, TouchAndDrag)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent1 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent2 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);

    m_pointerEventManager.remove(pointerEvent1);
    m_pointerEventManager.remove(pointerEvent2);

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart+1, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart+1, true);

    // Remove an obsolete (i.e. already removed) pointer event which should have no effect
    m_pointerEventManager.remove(pointerEvent1);

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart+1, true);
    createAndCheckTouchCancel(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart+1, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart+1, true);
}

TEST_F(PointerEventManagerTest, MouseAndTouchAndPen)
{
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true);
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent1 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+1, true);

    RefPtrWillBeRawPtr<PointerEvent> pointerEvent2 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 1, m_mappedIdStart+2, false);
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent3 = createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 2, m_mappedIdStart+3, false);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+1, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 47213, m_mappedIdStart+4, false);

    m_pointerEventManager.remove(pointerEvent1);
    m_pointerEventManager.remove(pointerEvent2);
    m_pointerEventManager.remove(pointerEvent3);

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 100, m_mappedIdStart+5, true);

    m_pointerEventManager.clear();

    createAndCheckMouseEvent(WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Touch, 0, m_mappedIdStart, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+1, true);
}

TEST_F(PointerEventManagerTest, PenAsTouchAndMouseEvent)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent1 = createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart, true);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 1, m_mappedIdStart+1, false);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 2, m_mappedIdStart+2, false);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart, true);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 1, m_mappedIdStart+1, false);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 1, m_mappedIdStart+1, false);

    m_pointerEventManager.remove(pointerEvent1);

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+3, false);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+3, false);
    createAndCheckTouchCancel(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+3, false);

    m_pointerEventManager.clear();

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 1, m_mappedIdStart, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+1, false);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 1, m_mappedIdStart, true);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+1, false);
    createAndCheckTouchCancel(WebPointerProperties::PointerType::Pen, 1, m_mappedIdStart, true);
    createAndCheckTouchCancel(WebPointerProperties::PointerType::Pen, 0, m_mappedIdStart+1, false);
}


TEST_F(PointerEventManagerTest, OutOfRange)
{
    RefPtrWillBeRawPtr<PointerEvent> pointerEvent1 = createAndCheckMouseEvent(WebPointerProperties::PointerType::Unknown, 0, m_mappedIdStart, true);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Unknown, 1, m_mappedIdStart+1, false);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Unknown, 2, m_mappedIdStart+2, false);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Unknown, 0, m_mappedIdStart, true);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Unknown, 3, m_mappedIdStart+3, false);
    createAndCheckMouseEvent(WebPointerProperties::PointerType::Unknown, 2, m_mappedIdStart+2, false);
    createAndCheckTouchCancel(WebPointerProperties::PointerType::Unknown, 3, m_mappedIdStart+3, false);

    m_pointerEventManager.remove(pointerEvent1);

    createAndCheckTouchEvent(WebPointerProperties::PointerType::Unknown, 0, m_mappedIdStart+4, false);
    createAndCheckTouchEvent(WebPointerProperties::PointerType::Unknown, INT_MAX, m_mappedIdStart+5, false);

    m_pointerEventManager.clear();

    for (int i = 0; i < 100; ++i) {
        createAndCheckMouseEvent(WebPointerProperties::PointerType::Touch, i, m_mappedIdStart+i, i == 0);
    }

    for (int i = 0; i < 100; ++i) {
        createAndCheckTouchEvent(WebPointerProperties::PointerType::Mouse, i, m_expectedMouseId, true);
    }
    createAndCheckTouchCancel(WebPointerProperties::PointerType::Mouse, 0, m_expectedMouseId, true);
}

} // namespace blink
