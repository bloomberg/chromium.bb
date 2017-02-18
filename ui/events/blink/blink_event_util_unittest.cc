// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/blink_event_util.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebMouseWheelEvent.h"
#include "ui/events/gesture_event_details.h"

namespace ui {

using BlinkEventUtilTest = testing::Test;

TEST(BlinkEventUtilTest, NoScalingWith1DSF) {
  ui::GestureEventDetails details(ui::ET_GESTURE_SCROLL_UPDATE, 1, 1);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  auto event =
      CreateWebGestureEvent(details,
                            base::TimeTicks(),
                            gfx::PointF(1.f, 1.f),
                            gfx::PointF(1.f, 1.f),
                            0,
                            0U);
  EXPECT_FALSE(ScaleWebInputEvent(event, 1.f));
  EXPECT_TRUE(ScaleWebInputEvent(event, 2.f));
}

TEST(BlinkEventUtilTest, NonPaginatedWebMouseWheelEvent) {
  blink::WebMouseWheelEvent event(blink::WebInputEvent::MouseWheel,
                                  blink::WebInputEvent::NoModifiers,
                                  blink::WebInputEvent::TimeStampForTesting);
  event.deltaX = 1.f;
  event.deltaY = 1.f;
  event.wheelTicksX = 1.f;
  event.wheelTicksY = 1.f;
  event.scrollByPage = false;
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebMouseWheelEvent* mouseWheelEvent =
      static_cast<blink::WebMouseWheelEvent*>(webEvent.get());
  EXPECT_EQ(2.f, mouseWheelEvent->deltaX);
  EXPECT_EQ(2.f, mouseWheelEvent->deltaY);
  EXPECT_EQ(2.f, mouseWheelEvent->wheelTicksX);
  EXPECT_EQ(2.f, mouseWheelEvent->wheelTicksY);
}

TEST(BlinkEventUtilTest, PaginatedWebMouseWheelEvent) {
  blink::WebMouseWheelEvent event(blink::WebInputEvent::MouseWheel,
                                  blink::WebInputEvent::NoModifiers,
                                  blink::WebInputEvent::TimeStampForTesting);
  event.deltaX = 1.f;
  event.deltaY = 1.f;
  event.wheelTicksX = 1.f;
  event.wheelTicksY = 1.f;
  event.scrollByPage = true;
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebMouseWheelEvent* mouseWheelEvent =
      static_cast<blink::WebMouseWheelEvent*>(webEvent.get());
  EXPECT_EQ(1.f, mouseWheelEvent->deltaX);
  EXPECT_EQ(1.f, mouseWheelEvent->deltaY);
  EXPECT_EQ(1.f, mouseWheelEvent->wheelTicksX);
  EXPECT_EQ(1.f, mouseWheelEvent->wheelTicksY);
}

TEST(BlinkEventUtilTest, NonPaginatedScrollBeginEvent) {
  ui::GestureEventDetails details(ui::ET_GESTURE_SCROLL_BEGIN, 1, 1);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  auto event =
      CreateWebGestureEvent(details, base::TimeTicks(), gfx::PointF(1.f, 1.f),
                            gfx::PointF(1.f, 1.f), 0, 0U);
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebGestureEvent* gestureEvent =
      static_cast<blink::WebGestureEvent*>(webEvent.get());
  EXPECT_EQ(2.f, gestureEvent->data.scrollBegin.deltaXHint);
  EXPECT_EQ(2.f, gestureEvent->data.scrollBegin.deltaYHint);
}

TEST(BlinkEventUtilTest, PaginatedScrollBeginEvent) {
  ui::GestureEventDetails details(ui::ET_GESTURE_SCROLL_BEGIN, 1, 1,
                                  ui::GestureEventDetails::ScrollUnits::PAGE);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  auto event =
      CreateWebGestureEvent(details, base::TimeTicks(), gfx::PointF(1.f, 1.f),
                            gfx::PointF(1.f, 1.f), 0, 0U);
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebGestureEvent* gestureEvent =
      static_cast<blink::WebGestureEvent*>(webEvent.get());
  EXPECT_EQ(1.f, gestureEvent->data.scrollBegin.deltaXHint);
  EXPECT_EQ(1.f, gestureEvent->data.scrollBegin.deltaYHint);
}

TEST(BlinkEventUtilTest, NonPaginatedScrollUpdateEvent) {
  ui::GestureEventDetails details(ui::ET_GESTURE_SCROLL_UPDATE, 1, 1);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  auto event =
      CreateWebGestureEvent(details, base::TimeTicks(), gfx::PointF(1.f, 1.f),
                            gfx::PointF(1.f, 1.f), 0, 0U);
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebGestureEvent* gestureEvent =
      static_cast<blink::WebGestureEvent*>(webEvent.get());
  EXPECT_EQ(2.f, gestureEvent->data.scrollUpdate.deltaX);
  EXPECT_EQ(2.f, gestureEvent->data.scrollUpdate.deltaY);
}

TEST(BlinkEventUtilTest, PaginatedScrollUpdateEvent) {
  ui::GestureEventDetails details(ui::ET_GESTURE_SCROLL_UPDATE, 1, 1,
                                  ui::GestureEventDetails::ScrollUnits::PAGE);
  details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  auto event =
      CreateWebGestureEvent(details, base::TimeTicks(), gfx::PointF(1.f, 1.f),
                            gfx::PointF(1.f, 1.f), 0, 0U);
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebGestureEvent* gestureEvent =
      static_cast<blink::WebGestureEvent*>(webEvent.get());
  EXPECT_EQ(1.f, gestureEvent->data.scrollUpdate.deltaX);
  EXPECT_EQ(1.f, gestureEvent->data.scrollUpdate.deltaY);
}

TEST(BlinkEventUtilTest, TouchEventCoalescing) {
  blink::WebTouchPoint touch_point;
  touch_point.id = 1;
  touch_point.state = blink::WebTouchPoint::StateMoved;

  blink::WebTouchEvent coalesced_event;
  coalesced_event.setType(blink::WebInputEvent::TouchMove);
  touch_point.movementX = 5;
  touch_point.movementY = 10;
  coalesced_event.touches[coalesced_event.touchesLength++] = touch_point;

  blink::WebTouchEvent event_to_be_coalesced;
  event_to_be_coalesced.setType(blink::WebInputEvent::TouchMove);
  touch_point.movementX = 3;
  touch_point.movementY = -4;
  event_to_be_coalesced.touches[event_to_be_coalesced.touchesLength++] =
      touch_point;

  EXPECT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));
  Coalesce(event_to_be_coalesced, &coalesced_event);
  EXPECT_EQ(8, coalesced_event.touches[0].movementX);
  EXPECT_EQ(6, coalesced_event.touches[0].movementY);
}

}  // namespace ui
