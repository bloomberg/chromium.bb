/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/events/WebInputEventConversion.h"

#include "core/events/GestureEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/TouchEvent.h"
#include "core/events/WheelEvent.h"
#include "core/exported/WebViewImpl.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/input/Touch.h"
#include "core/input/TouchList.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/Page.h"
#include "platform/geometry/IntSize.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/web/WebFrame.h"
#include "public/web/WebSettings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

KeyboardEvent* CreateKeyboardEventWithLocation(
    KeyboardEvent::KeyLocationCode location) {
  KeyboardEventInit key_event_init;
  key_event_init.setBubbles(true);
  key_event_init.setCancelable(true);
  key_event_init.setLocation(location);
  return new KeyboardEvent("keydown", key_event_init);
}

int GetModifiersForKeyLocationCode(KeyboardEvent::KeyLocationCode location) {
  KeyboardEvent* event = CreateKeyboardEventWithLocation(location);
  WebKeyboardEventBuilder converted_event(*event);
  return converted_event.GetModifiers();
}

void RegisterMockedURL(const std::string& base_url,
                       const std::string& file_name) {
  URLTestHelpers::RegisterMockedURLLoadFromBase(WebString::FromUTF8(base_url),
                                                testing::CoreTestDataPath(),
                                                WebString::FromUTF8(file_name));
}

}  // namespace

TEST(WebInputEventConversionTest, WebKeyboardEventBuilder) {
  // Test key location conversion.
  int modifiers =
      GetModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationStandard);
  EXPECT_FALSE(modifiers & WebInputEvent::kIsKeyPad ||
               modifiers & WebInputEvent::kIsLeft ||
               modifiers & WebInputEvent::kIsRight);

  modifiers =
      GetModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationLeft);
  EXPECT_TRUE(modifiers & WebInputEvent::kIsLeft);
  EXPECT_FALSE(modifiers & WebInputEvent::kIsKeyPad ||
               modifiers & WebInputEvent::kIsRight);

  modifiers =
      GetModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationRight);
  EXPECT_TRUE(modifiers & WebInputEvent::kIsRight);
  EXPECT_FALSE(modifiers & WebInputEvent::kIsKeyPad ||
               modifiers & WebInputEvent::kIsLeft);

  modifiers =
      GetModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationNumpad);
  EXPECT_TRUE(modifiers & WebInputEvent::kIsKeyPad);
  EXPECT_FALSE(modifiers & WebInputEvent::kIsLeft ||
               modifiers & WebInputEvent::kIsRight);
}

TEST(WebInputEventConversionTest, WebMouseEventBuilder) {
  TouchEvent* event = TouchEvent::Create();
  WebMouseEventBuilder mouse(nullptr, nullptr, *event);
  EXPECT_EQ(WebInputEvent::kUndefined, mouse.GetType());
}

TEST(WebInputEventConversionTest, InputEventsScaling) {
  const std::string base_url("http://www.test1.com/");
  const std::string file_name("fixed_layout.html");

  RegisterMockedURL(base_url, file_name);
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url + file_name);
  web_view->GetSettings()->SetViewportEnabled(true);
  int page_width = 640;
  int page_height = 480;
  web_view->Resize(WebSize(page_width, page_height));
  web_view->UpdateAllLifecyclePhases();

  web_view->SetPageScaleFactor(2);

  LocalFrameView* view = ToLocalFrame(web_view->GetPage()->MainFrame())->View();

  {
    WebMouseEvent web_mouse_event(WebInputEvent::kMouseMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::kTimeStampForTesting);
    web_mouse_event.SetPositionInWidget(10, 10);
    web_mouse_event.SetPositionInScreen(10, 10);
    web_mouse_event.movement_x = 10;
    web_mouse_event.movement_y = 10;

    WebMouseEvent transformed_event =
        TransformWebMouseEvent(view, web_mouse_event);
    IntPoint position =
        FlooredIntPoint(transformed_event.PositionInRootFrame());
    EXPECT_EQ(5, position.X());
    EXPECT_EQ(5, position.Y());
    EXPECT_EQ(10, transformed_event.PositionInScreen().x);
    EXPECT_EQ(10, transformed_event.PositionInScreen().y);

    IntPoint movement =
        FlooredIntPoint(transformed_event.MovementInRootFrame());
    EXPECT_EQ(5, movement.X());
    EXPECT_EQ(5, movement.Y());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureScrollUpdate,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.x = 10;
    web_gesture_event.y = 12;
    web_gesture_event.global_x = 20;
    web_gesture_event.global_y = 22;
    web_gesture_event.data.scroll_update.delta_x = 30;
    web_gesture_event.data.scroll_update.delta_y = 32;
    web_gesture_event.data.scroll_update.velocity_x = 40;
    web_gesture_event.data.scroll_update.velocity_y = 42;
    web_gesture_event.data.scroll_update.inertial_phase =
        WebGestureEvent::kMomentumPhase;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntPoint position =
        FlooredIntPoint(scaled_gesture_event.PositionInRootFrame());
    EXPECT_EQ(5, position.X());
    EXPECT_EQ(6, position.Y());
    EXPECT_EQ(20, scaled_gesture_event.global_x);
    EXPECT_EQ(22, scaled_gesture_event.global_y);
    EXPECT_EQ(15, scaled_gesture_event.DeltaXInRootFrame());
    EXPECT_EQ(16, scaled_gesture_event.DeltaYInRootFrame());
    // TODO: The velocity values may need to be scaled to page scale in
    // order to remain consist with delta values.
    EXPECT_EQ(40, scaled_gesture_event.VelocityX());
    EXPECT_EQ(42, scaled_gesture_event.VelocityY());
    EXPECT_EQ(WebGestureEvent::kMomentumPhase,
              scaled_gesture_event.InertialPhase());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureScrollEnd,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.x = 10;
    web_gesture_event.y = 12;
    web_gesture_event.global_x = 20;
    web_gesture_event.global_y = 22;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntPoint position =
        FlooredIntPoint(scaled_gesture_event.PositionInRootFrame());
    EXPECT_EQ(5, position.X());
    EXPECT_EQ(6, position.Y());
    EXPECT_EQ(20, scaled_gesture_event.global_x);
    EXPECT_EQ(22, scaled_gesture_event.global_y);
    EXPECT_EQ(WebGestureEvent::kUnknownMomentumPhase,
              scaled_gesture_event.InertialPhase());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureTap,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.data.tap.width = 10;
    web_gesture_event.data.tap.height = 10;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(5, area.Width());
    EXPECT_EQ(5, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureTapUnconfirmed,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.data.tap.width = 10;
    web_gesture_event.data.tap.height = 10;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(5, area.Width());
    EXPECT_EQ(5, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureTapDown,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.data.tap_down.width = 10;
    web_gesture_event.data.tap_down.height = 10;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(5, area.Width());
    EXPECT_EQ(5, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureShowPress,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.data.show_press.width = 10;
    web_gesture_event.data.show_press.height = 10;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(5, area.Width());
    EXPECT_EQ(5, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureLongPress,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.data.long_press.width = 10;
    web_gesture_event.data.long_press.height = 10;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(5, area.Width());
    EXPECT_EQ(5, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureTwoFingerTap,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.data.two_finger_tap.first_finger_width = 10;
    web_gesture_event.data.two_finger_tap.first_finger_height = 10;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(5, area.Width());
    EXPECT_EQ(5, area.Height());
  }

  {
    WebTouchEvent web_touch_event(WebInputEvent::kTouchMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::kTimeStampForTesting);
    web_touch_event.touches_length = 1;
    web_touch_event.touches[0].state = WebTouchPoint::kStateMoved;
    web_touch_event.touches[0].SetPositionInScreen(10.6f, 10.4f);
    web_touch_event.touches[0].SetPositionInWidget(10.6f, 10.4f);
    web_touch_event.touches[0].radius_x = 10.6f;
    web_touch_event.touches[0].radius_y = 10.4f;
    web_touch_event.touches[0].movement_x = 20;
    web_touch_event.touches[0].movement_y = 20;

    EXPECT_FLOAT_EQ(10.6f, web_touch_event.touches[0].PositionInScreen().x);
    EXPECT_FLOAT_EQ(10.4f, web_touch_event.touches[0].PositionInScreen().y);
    EXPECT_FLOAT_EQ(10.6f, web_touch_event.touches[0].PositionInWidget().x);
    EXPECT_FLOAT_EQ(10.4f, web_touch_event.touches[0].PositionInWidget().y);
    EXPECT_FLOAT_EQ(10.6f, web_touch_event.touches[0].radius_x);
    EXPECT_FLOAT_EQ(10.4f, web_touch_event.touches[0].radius_y);
    EXPECT_EQ(20, web_touch_event.touches[0].movement_x);
    EXPECT_EQ(20, web_touch_event.touches[0].movement_y);

    WebTouchEvent transformed_event =
        TransformWebTouchEvent(view, web_touch_event);
    WebTouchPoint transformed_point =
        transformed_event.TouchPointInRootFrame(0);
    EXPECT_FLOAT_EQ(10.6f, transformed_point.PositionInScreen().x);
    EXPECT_FLOAT_EQ(10.4f, transformed_point.PositionInScreen().y);
    EXPECT_FLOAT_EQ(5.3f, transformed_point.PositionInWidget().x);
    EXPECT_FLOAT_EQ(5.2f, transformed_point.PositionInWidget().y);
    EXPECT_FLOAT_EQ(5.3f, transformed_point.radius_x);
    EXPECT_FLOAT_EQ(5.2f, transformed_point.radius_y);
    EXPECT_EQ(10, transformed_point.movement_x);
    EXPECT_EQ(10, transformed_point.movement_y);
  }
}

TEST(WebInputEventConversionTest, InputEventsTransform) {
  const std::string base_url("http://www.test2.com/");
  const std::string file_name("fixed_layout.html");

  RegisterMockedURL(base_url, file_name);
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url + file_name);
  web_view->GetSettings()->SetViewportEnabled(true);
  int page_width = 640;
  int page_height = 480;
  web_view->Resize(WebSize(page_width, page_height));
  web_view->UpdateAllLifecyclePhases();

  web_view->SetPageScaleFactor(2);
  web_view->MainFrameImpl()->SetInputEventsScaleForEmulation(1.5);

  LocalFrameView* view = ToLocalFrame(web_view->GetPage()->MainFrame())->View();

  {
    WebMouseEvent web_mouse_event(WebInputEvent::kMouseMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::kTimeStampForTesting);
    web_mouse_event.SetPositionInWidget(90, 90);
    web_mouse_event.SetPositionInScreen(90, 90);
    web_mouse_event.movement_x = 60;
    web_mouse_event.movement_y = 60;

    WebMouseEvent transformed_event =
        TransformWebMouseEvent(view, web_mouse_event);
    FloatPoint position = transformed_event.PositionInRootFrame();

    EXPECT_FLOAT_EQ(30, position.X());
    EXPECT_FLOAT_EQ(30, position.Y());
    EXPECT_EQ(90, transformed_event.PositionInScreen().x);
    EXPECT_EQ(90, transformed_event.PositionInScreen().y);

    IntPoint movement =
        FlooredIntPoint(transformed_event.MovementInRootFrame());
    EXPECT_EQ(20, movement.X());
    EXPECT_EQ(20, movement.Y());
  }

  {
    WebMouseEvent web_mouse_event1(WebInputEvent::kMouseMove,
                                   WebInputEvent::kNoModifiers,
                                   WebInputEvent::kTimeStampForTesting);
    web_mouse_event1.SetPositionInWidget(90, 90);
    web_mouse_event1.SetPositionInScreen(90, 90);
    web_mouse_event1.movement_x = 60;
    web_mouse_event1.movement_y = 60;

    WebMouseEvent web_mouse_event2 = web_mouse_event1;
    web_mouse_event2.SetPositionInWidget(web_mouse_event1.PositionInWidget().x,
                                         120);
    web_mouse_event2.SetPositionInScreen(web_mouse_event1.PositionInScreen().x,
                                         120);
    web_mouse_event2.movement_y = 30;

    std::vector<const WebInputEvent*> events;
    events.push_back(&web_mouse_event1);
    events.push_back(&web_mouse_event2);

    Vector<WebMouseEvent> coalescedevents =
        TransformWebMouseEventVector(view, events);
    EXPECT_EQ(events.size(), coalescedevents.size());

    FloatPoint position = coalescedevents[0].PositionInRootFrame();
    EXPECT_FLOAT_EQ(30, position.X());
    EXPECT_FLOAT_EQ(30, position.Y());
    EXPECT_EQ(90, coalescedevents[0].PositionInScreen().x);
    EXPECT_EQ(90, coalescedevents[0].PositionInScreen().y);

    IntPoint movement =
        FlooredIntPoint(coalescedevents[0].MovementInRootFrame());
    EXPECT_EQ(20, movement.X());
    EXPECT_EQ(20, movement.Y());

    position = coalescedevents[1].PositionInRootFrame();
    EXPECT_FLOAT_EQ(30, position.X());
    EXPECT_FLOAT_EQ(40, position.Y());
    EXPECT_EQ(90, coalescedevents[1].PositionInScreen().x);
    EXPECT_EQ(120, coalescedevents[1].PositionInScreen().y);

    movement = FlooredIntPoint(coalescedevents[1].MovementInRootFrame());
    EXPECT_EQ(20, movement.X());
    EXPECT_EQ(10, movement.Y());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureScrollUpdate,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.x = 90;
    web_gesture_event.y = 90;
    web_gesture_event.global_x = 90;
    web_gesture_event.global_y = 90;
    web_gesture_event.data.scroll_update.delta_x = 60;
    web_gesture_event.data.scroll_update.delta_y = 60;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    FloatPoint position = scaled_gesture_event.PositionInRootFrame();

    EXPECT_FLOAT_EQ(30, position.X());
    EXPECT_FLOAT_EQ(30, position.Y());
    EXPECT_EQ(90, scaled_gesture_event.global_x);
    EXPECT_EQ(90, scaled_gesture_event.global_y);
    EXPECT_EQ(20, scaled_gesture_event.DeltaXInRootFrame());
    EXPECT_EQ(20, scaled_gesture_event.DeltaYInRootFrame());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureTap,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.data.tap.width = 30;
    web_gesture_event.data.tap.height = 30;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(10, area.Width());
    EXPECT_EQ(10, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureTapUnconfirmed,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.data.tap.width = 30;
    web_gesture_event.data.tap.height = 30;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(10, area.Width());
    EXPECT_EQ(10, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureTapDown,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.data.tap_down.width = 30;
    web_gesture_event.data.tap_down.height = 30;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(10, area.Width());
    EXPECT_EQ(10, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureShowPress,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.data.show_press.width = 30;
    web_gesture_event.data.show_press.height = 30;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(10, area.Width());
    EXPECT_EQ(10, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureLongPress,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.data.long_press.width = 30;
    web_gesture_event.data.long_press.height = 30;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(10, area.Width());
    EXPECT_EQ(10, area.Height());
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureTwoFingerTap,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.data.two_finger_tap.first_finger_width = 30;
    web_gesture_event.data.two_finger_tap.first_finger_height = 30;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntSize area = FlooredIntSize(scaled_gesture_event.TapAreaInRootFrame());
    EXPECT_EQ(10, area.Width());
    EXPECT_EQ(10, area.Height());
  }

  {
    WebTouchEvent web_touch_event(WebInputEvent::kTouchMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::kTimeStampForTesting);
    web_touch_event.touches_length = 1;
    web_touch_event.touches[0].state = WebTouchPoint::kStateMoved;
    web_touch_event.touches[0].SetPositionInScreen(90, 90);
    web_touch_event.touches[0].SetPositionInWidget(90, 90);
    web_touch_event.touches[0].radius_x = 30;
    web_touch_event.touches[0].radius_y = 30;

    WebTouchEvent transformed_event =
        TransformWebTouchEvent(view, web_touch_event);

    WebTouchPoint transformed_point =
        transformed_event.TouchPointInRootFrame(0);
    EXPECT_FLOAT_EQ(90, transformed_point.PositionInScreen().x);
    EXPECT_FLOAT_EQ(90, transformed_point.PositionInScreen().y);
    EXPECT_FLOAT_EQ(30, transformed_point.PositionInWidget().x);
    EXPECT_FLOAT_EQ(30, transformed_point.PositionInWidget().y);
    EXPECT_FLOAT_EQ(10, transformed_point.radius_x);
    EXPECT_FLOAT_EQ(10, transformed_point.radius_y);
  }

  {
    WebTouchEvent web_touch_event1(WebInputEvent::kTouchMove,
                                   WebInputEvent::kNoModifiers,
                                   WebInputEvent::kTimeStampForTesting);
    web_touch_event1.touches_length = 1;
    web_touch_event1.touches[0].state = WebTouchPoint::kStateMoved;
    web_touch_event1.touches[0].SetPositionInScreen(90, 90);
    web_touch_event1.touches[0].SetPositionInWidget(90, 90);
    web_touch_event1.touches[0].radius_x = 30;
    web_touch_event1.touches[0].radius_y = 30;

    WebTouchEvent web_touch_event2 = web_touch_event1;
    web_touch_event2.touches[0].SetPositionInScreen(120, 90);
    web_touch_event2.touches[0].SetPositionInWidget(120, 90);
    web_touch_event2.touches[0].radius_x = 60;

    std::vector<const WebInputEvent*> events;
    events.push_back(&web_touch_event1);
    events.push_back(&web_touch_event2);

    Vector<WebTouchEvent> coalescedevents =
        TransformWebTouchEventVector(view, events);
    EXPECT_EQ(events.size(), coalescedevents.size());

    WebTouchPoint transformed_point =
        coalescedevents[0].TouchPointInRootFrame(0);
    EXPECT_FLOAT_EQ(90, transformed_point.PositionInScreen().x);
    EXPECT_FLOAT_EQ(90, transformed_point.PositionInScreen().y);
    EXPECT_FLOAT_EQ(30, transformed_point.PositionInWidget().x);
    EXPECT_FLOAT_EQ(30, transformed_point.PositionInWidget().y);
    EXPECT_FLOAT_EQ(10, transformed_point.radius_x);
    EXPECT_FLOAT_EQ(10, transformed_point.radius_y);

    transformed_point = coalescedevents[1].TouchPointInRootFrame(0);
    EXPECT_FLOAT_EQ(120, transformed_point.PositionInScreen().x);
    EXPECT_FLOAT_EQ(90, transformed_point.PositionInScreen().y);
    EXPECT_FLOAT_EQ(40, transformed_point.PositionInWidget().x);
    EXPECT_FLOAT_EQ(30, transformed_point.PositionInWidget().y);
    EXPECT_FLOAT_EQ(20, transformed_point.radius_x);
    EXPECT_FLOAT_EQ(10, transformed_point.radius_y);
  }
}

TEST(WebInputEventConversionTest, InputEventsConversions) {
  const std::string base_url("http://www.test3.com/");
  const std::string file_name("fixed_layout.html");

  RegisterMockedURL(base_url, file_name);
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url + file_name);
  int page_width = 640;
  int page_height = 480;
  web_view->Resize(WebSize(page_width, page_height));
  web_view->UpdateAllLifecyclePhases();

  LocalFrameView* view = ToLocalFrame(web_view->GetPage()->MainFrame())->View();
  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureTap,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.x = 10;
    web_gesture_event.y = 10;
    web_gesture_event.global_x = 10;
    web_gesture_event.global_y = 10;
    web_gesture_event.data.tap.tap_count = 1;
    web_gesture_event.data.tap.width = 10;
    web_gesture_event.data.tap.height = 10;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntPoint position =
        FlooredIntPoint(scaled_gesture_event.PositionInRootFrame());
    EXPECT_EQ(10.f, position.X());
    EXPECT_EQ(10.f, position.Y());
    EXPECT_EQ(10.f, scaled_gesture_event.global_x);
    EXPECT_EQ(10.f, scaled_gesture_event.global_y);
    EXPECT_EQ(1, scaled_gesture_event.TapCount());
  }
}

TEST(WebInputEventConversionTest, VisualViewportOffset) {
  const std::string base_url("http://www.test4.com/");
  const std::string file_name("fixed_layout.html");

  RegisterMockedURL(base_url, file_name);
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url + file_name);
  int page_width = 640;
  int page_height = 480;
  web_view->Resize(WebSize(page_width, page_height));
  web_view->UpdateAllLifecyclePhases();

  web_view->SetPageScaleFactor(2);

  IntPoint visual_offset(35, 60);
  web_view->GetPage()->GetVisualViewport().SetLocation(visual_offset);

  LocalFrameView* view = ToLocalFrame(web_view->GetPage()->MainFrame())->View();

  {
    WebMouseEvent web_mouse_event(WebInputEvent::kMouseMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::kTimeStampForTesting);
    web_mouse_event.SetPositionInWidget(10, 10);
    web_mouse_event.SetPositionInScreen(10, 10);

    WebMouseEvent transformed_mouse_event =
        TransformWebMouseEvent(view, web_mouse_event);
    IntPoint position =
        FlooredIntPoint(transformed_mouse_event.PositionInRootFrame());
    EXPECT_EQ(5 + visual_offset.X(), position.X());
    EXPECT_EQ(5 + visual_offset.Y(), position.Y());
    EXPECT_EQ(10, transformed_mouse_event.PositionInScreen().x);
    EXPECT_EQ(10, transformed_mouse_event.PositionInScreen().y);
  }

  {
    WebMouseWheelEvent web_mouse_wheel_event(
        WebInputEvent::kMouseWheel, WebInputEvent::kNoModifiers,
        WebInputEvent::kTimeStampForTesting);
    web_mouse_wheel_event.SetPositionInWidget(10, 10);
    web_mouse_wheel_event.SetPositionInScreen(10, 10);

    WebMouseWheelEvent scaled_mouse_wheel_event =
        TransformWebMouseWheelEvent(view, web_mouse_wheel_event);
    IntPoint position =
        FlooredIntPoint(scaled_mouse_wheel_event.PositionInRootFrame());
    EXPECT_EQ(5 + visual_offset.X(), position.X());
    EXPECT_EQ(5 + visual_offset.Y(), position.Y());
    EXPECT_EQ(10, scaled_mouse_wheel_event.PositionInScreen().x);
    EXPECT_EQ(10, scaled_mouse_wheel_event.PositionInScreen().y);
  }

  {
    WebGestureEvent web_gesture_event(WebInputEvent::kGestureScrollUpdate,
                                      WebInputEvent::kNoModifiers,
                                      WebInputEvent::kTimeStampForTesting);
    web_gesture_event.source_device = kWebGestureDeviceTouchscreen;
    web_gesture_event.x = 10;
    web_gesture_event.y = 10;
    web_gesture_event.global_x = 10;
    web_gesture_event.global_y = 10;

    WebGestureEvent scaled_gesture_event =
        TransformWebGestureEvent(view, web_gesture_event);
    IntPoint position =
        FlooredIntPoint(scaled_gesture_event.PositionInRootFrame());
    EXPECT_EQ(5 + visual_offset.X(), position.X());
    EXPECT_EQ(5 + visual_offset.Y(), position.Y());
    EXPECT_EQ(10, scaled_gesture_event.global_x);
    EXPECT_EQ(10, scaled_gesture_event.global_y);
  }

  {
    WebTouchEvent web_touch_event(WebInputEvent::kTouchMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::kTimeStampForTesting);
    web_touch_event.touches_length = 1;
    web_touch_event.touches[0].state = WebTouchPoint::kStateMoved;
    web_touch_event.touches[0].SetPositionInScreen(10.6f, 10.4f);
    web_touch_event.touches[0].SetPositionInWidget(10.6f, 10.4f);

    EXPECT_FLOAT_EQ(10.6f, web_touch_event.touches[0].PositionInScreen().x);
    EXPECT_FLOAT_EQ(10.4f, web_touch_event.touches[0].PositionInScreen().y);
    EXPECT_FLOAT_EQ(10.6f, web_touch_event.touches[0].PositionInWidget().x);
    EXPECT_FLOAT_EQ(10.4f, web_touch_event.touches[0].PositionInWidget().y);

    WebTouchEvent transformed_touch_event =
        TransformWebTouchEvent(view, web_touch_event);
    WebTouchPoint transformed_point =
        transformed_touch_event.TouchPointInRootFrame(0);
    EXPECT_FLOAT_EQ(10.6f, transformed_point.PositionInScreen().x);
    EXPECT_FLOAT_EQ(10.4f, transformed_point.PositionInScreen().y);
    EXPECT_FLOAT_EQ(5.3f + visual_offset.X(),
                    transformed_point.PositionInWidget().x);
    EXPECT_FLOAT_EQ(5.2f + visual_offset.Y(),
                    transformed_point.PositionInWidget().y);
  }
}

TEST(WebInputEventConversionTest, ElasticOverscroll) {
  const std::string base_url("http://www.test5.com/");
  const std::string file_name("fixed_layout.html");

  RegisterMockedURL(base_url, file_name);
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url + file_name);
  int page_width = 640;
  int page_height = 480;
  web_view->Resize(WebSize(page_width, page_height));
  web_view->UpdateAllLifecyclePhases();

  LocalFrameView* view = ToLocalFrame(web_view->GetPage()->MainFrame())->View();

  FloatSize elastic_overscroll(10, -20);
  web_view->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                elastic_overscroll, 1.0f, 0.0f);

  // Just elastic overscroll.
  {
    WebMouseEvent web_mouse_event(WebInputEvent::kMouseMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::kTimeStampForTesting);
    web_mouse_event.SetPositionInWidget(10, 50);
    web_mouse_event.SetPositionInScreen(10, 50);

    WebMouseEvent transformed_mouse_event =
        TransformWebMouseEvent(view, web_mouse_event);
    IntPoint position =
        FlooredIntPoint(transformed_mouse_event.PositionInRootFrame());

    EXPECT_EQ(web_mouse_event.PositionInWidget().x + elastic_overscroll.Width(),
              position.X());
    EXPECT_EQ(
        web_mouse_event.PositionInWidget().y + elastic_overscroll.Height(),
        position.Y());
    EXPECT_EQ(web_mouse_event.PositionInScreen().x,
              transformed_mouse_event.PositionInScreen().x);
    EXPECT_EQ(web_mouse_event.PositionInScreen().y,
              transformed_mouse_event.PositionInScreen().y);
  }

  // Elastic overscroll and pinch-zoom (this doesn't actually ever happen,
  // but ensure that if it were to, the overscroll would be applied after the
  // pinch-zoom).
  float page_scale = 2;
  web_view->SetPageScaleFactor(page_scale);
  IntPoint visual_offset(35, 60);
  web_view->GetPage()->GetVisualViewport().SetLocation(visual_offset);
  {
    WebMouseEvent web_mouse_event(WebInputEvent::kMouseMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::kTimeStampForTesting);
    web_mouse_event.SetPositionInWidget(10, 10);
    web_mouse_event.SetPositionInScreen(10, 10);

    WebMouseEvent transformed_mouse_event =
        TransformWebMouseEvent(view, web_mouse_event);
    IntPoint position =
        FlooredIntPoint(transformed_mouse_event.PositionInRootFrame());

    EXPECT_EQ(web_mouse_event.PositionInWidget().x / page_scale +
                  visual_offset.X() + elastic_overscroll.Width(),
              position.X());
    EXPECT_EQ(web_mouse_event.PositionInWidget().y / page_scale +
                  visual_offset.Y() + elastic_overscroll.Height(),
              position.Y());
    EXPECT_EQ(web_mouse_event.PositionInScreen().x,
              transformed_mouse_event.PositionInScreen().x);
    EXPECT_EQ(web_mouse_event.PositionInScreen().y,
              transformed_mouse_event.PositionInScreen().y);
  }
}

// Page reload/navigation should not reset elastic overscroll.
TEST(WebInputEventConversionTest, ElasticOverscrollWithPageReload) {
  const std::string base_url("http://www.test6.com/");
  const std::string file_name("fixed_layout.html");

  RegisterMockedURL(base_url, file_name);
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view =
      web_view_helper.InitializeAndLoad(base_url + file_name);
  int page_width = 640;
  int page_height = 480;
  web_view->Resize(WebSize(page_width, page_height));
  web_view->UpdateAllLifecyclePhases();

  FloatSize elastic_overscroll(10, -20);
  web_view->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                elastic_overscroll, 1.0f, 0.0f);
  FrameTestHelpers::ReloadFrame(web_view_helper.WebView()->MainFrameImpl());
  LocalFrameView* view = ToLocalFrame(web_view->GetPage()->MainFrame())->View();

  // Just elastic overscroll.
  {
    WebMouseEvent web_mouse_event(WebInputEvent::kMouseMove,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::kTimeStampForTesting);
    web_mouse_event.SetPositionInWidget(10, 50);
    web_mouse_event.SetPositionInScreen(10, 50);

    WebMouseEvent transformed_mouse_event =
        TransformWebMouseEvent(view, web_mouse_event);
    IntPoint position =
        FlooredIntPoint(transformed_mouse_event.PositionInRootFrame());

    EXPECT_EQ(web_mouse_event.PositionInWidget().x + elastic_overscroll.Width(),
              position.X());
    EXPECT_EQ(
        web_mouse_event.PositionInWidget().y + elastic_overscroll.Height(),
        position.Y());
    EXPECT_EQ(web_mouse_event.PositionInScreen().x,
              transformed_mouse_event.PositionInScreen().x);
    EXPECT_EQ(web_mouse_event.PositionInScreen().y,
              transformed_mouse_event.PositionInScreen().y);
  }
}

}  // namespace blink
