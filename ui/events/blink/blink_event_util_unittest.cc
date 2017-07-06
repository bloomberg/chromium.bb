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
  blink::WebMouseWheelEvent event(blink::WebInputEvent::kMouseWheel,
                                  blink::WebInputEvent::kNoModifiers,
                                  blink::WebInputEvent::kTimeStampForTesting);
  event.delta_x = 1.f;
  event.delta_y = 1.f;
  event.wheel_ticks_x = 1.f;
  event.wheel_ticks_y = 1.f;
  event.scroll_by_page = false;
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebMouseWheelEvent* mouseWheelEvent =
      static_cast<blink::WebMouseWheelEvent*>(webEvent.get());
  EXPECT_EQ(2.f, mouseWheelEvent->delta_x);
  EXPECT_EQ(2.f, mouseWheelEvent->delta_y);
  EXPECT_EQ(2.f, mouseWheelEvent->wheel_ticks_x);
  EXPECT_EQ(2.f, mouseWheelEvent->wheel_ticks_y);
}

TEST(BlinkEventUtilTest, PaginatedWebMouseWheelEvent) {
  blink::WebMouseWheelEvent event(blink::WebInputEvent::kMouseWheel,
                                  blink::WebInputEvent::kNoModifiers,
                                  blink::WebInputEvent::kTimeStampForTesting);
  event.delta_x = 1.f;
  event.delta_y = 1.f;
  event.wheel_ticks_x = 1.f;
  event.wheel_ticks_y = 1.f;
  event.scroll_by_page = true;
  std::unique_ptr<blink::WebInputEvent> webEvent =
      ScaleWebInputEvent(event, 2.f);
  EXPECT_TRUE(webEvent);
  blink::WebMouseWheelEvent* mouseWheelEvent =
      static_cast<blink::WebMouseWheelEvent*>(webEvent.get());
  EXPECT_EQ(1.f, mouseWheelEvent->delta_x);
  EXPECT_EQ(1.f, mouseWheelEvent->delta_y);
  EXPECT_EQ(1.f, mouseWheelEvent->wheel_ticks_x);
  EXPECT_EQ(1.f, mouseWheelEvent->wheel_ticks_y);
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
  EXPECT_EQ(2.f, gestureEvent->data.scroll_begin.delta_x_hint);
  EXPECT_EQ(2.f, gestureEvent->data.scroll_begin.delta_y_hint);
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
  EXPECT_EQ(1.f, gestureEvent->data.scroll_begin.delta_x_hint);
  EXPECT_EQ(1.f, gestureEvent->data.scroll_begin.delta_y_hint);
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
  EXPECT_EQ(2.f, gestureEvent->data.scroll_update.delta_x);
  EXPECT_EQ(2.f, gestureEvent->data.scroll_update.delta_y);
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
  EXPECT_EQ(1.f, gestureEvent->data.scroll_update.delta_x);
  EXPECT_EQ(1.f, gestureEvent->data.scroll_update.delta_y);
}

TEST(BlinkEventUtilTest, TouchEventCoalescing) {
  blink::WebTouchPoint touch_point;
  touch_point.id = 1;
  touch_point.state = blink::WebTouchPoint::kStateMoved;

  blink::WebTouchEvent coalesced_event;
  coalesced_event.SetType(blink::WebInputEvent::kTouchMove);
  touch_point.movement_x = 5;
  touch_point.movement_y = 10;
  coalesced_event.touches[coalesced_event.touches_length++] = touch_point;

  blink::WebTouchEvent event_to_be_coalesced;
  event_to_be_coalesced.SetType(blink::WebInputEvent::kTouchMove);
  touch_point.movement_x = 3;
  touch_point.movement_y = -4;
  event_to_be_coalesced.touches[event_to_be_coalesced.touches_length++] =
      touch_point;

  EXPECT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));
  Coalesce(event_to_be_coalesced, &coalesced_event);
  EXPECT_EQ(8, coalesced_event.touches[0].movement_x);
  EXPECT_EQ(6, coalesced_event.touches[0].movement_y);
}

TEST(BlinkEventUtilTest, WebMouseWheelEventCoalescing) {
  blink::WebMouseWheelEvent coalesced_event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::kTimeStampForTesting);
  coalesced_event.delta_x = 1;
  coalesced_event.delta_y = 1;

  blink::WebMouseWheelEvent event_to_be_coalesced(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::kTimeStampForTesting);
  event_to_be_coalesced.delta_x = 3;
  event_to_be_coalesced.delta_y = 4;

  EXPECT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));
  Coalesce(event_to_be_coalesced, &coalesced_event);
  EXPECT_EQ(4, coalesced_event.delta_x);
  EXPECT_EQ(5, coalesced_event.delta_y);

  event_to_be_coalesced.resending_plugin_id = 3;
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));
}

TEST(BlinkEventUtilTest, WebGestureEventCoalescing) {
  blink::WebGestureEvent coalesced_event(
      blink::WebInputEvent::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::kTimeStampForTesting);
  coalesced_event.data.scroll_update.delta_x = 1;
  coalesced_event.data.scroll_update.delta_y = 1;

  blink::WebGestureEvent event_to_be_coalesced(
      blink::WebInputEvent::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::kTimeStampForTesting);
  event_to_be_coalesced.data.scroll_update.delta_x = 3;
  event_to_be_coalesced.data.scroll_update.delta_y = 4;

  EXPECT_TRUE(CanCoalesce(event_to_be_coalesced, coalesced_event));
  Coalesce(event_to_be_coalesced, &coalesced_event);
  EXPECT_EQ(4, coalesced_event.data.scroll_update.delta_x);
  EXPECT_EQ(5, coalesced_event.data.scroll_update.delta_y);

  event_to_be_coalesced.resending_plugin_id = 3;
  EXPECT_FALSE(CanCoalesce(event_to_be_coalesced, coalesced_event));
}

}  // namespace ui
