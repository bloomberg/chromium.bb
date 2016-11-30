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

#include "web/WebInputEventConversion.h"

#include "core/dom/Touch.h"
#include "core/dom/TouchList.h"
#include "core/events/GestureEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/TouchEvent.h"
#include "core/events/WheelEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/VisualViewport.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/Page.h"
#include "platform/geometry/IntSize.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/web/WebFrame.h"
#include "public/web/WebSettings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"

namespace blink {

KeyboardEvent* createKeyboardEventWithLocation(
    KeyboardEvent::KeyLocationCode location) {
  KeyboardEventInit keyEventInit;
  keyEventInit.setBubbles(true);
  keyEventInit.setCancelable(true);
  keyEventInit.setLocation(location);
  return new KeyboardEvent("keydown", keyEventInit);
}

int getModifiersForKeyLocationCode(KeyboardEvent::KeyLocationCode location) {
  KeyboardEvent* event = createKeyboardEventWithLocation(location);
  WebKeyboardEventBuilder convertedEvent(*event);
  return convertedEvent.modifiers;
}

TEST(WebInputEventConversionTest, WebKeyboardEventBuilder) {
  // Test key location conversion.
  int modifiers =
      getModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationStandard);
  EXPECT_FALSE(modifiers & WebInputEvent::IsKeyPad ||
               modifiers & WebInputEvent::IsLeft ||
               modifiers & WebInputEvent::IsRight);

  modifiers =
      getModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationLeft);
  EXPECT_TRUE(modifiers & WebInputEvent::IsLeft);
  EXPECT_FALSE(modifiers & WebInputEvent::IsKeyPad ||
               modifiers & WebInputEvent::IsRight);

  modifiers =
      getModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationRight);
  EXPECT_TRUE(modifiers & WebInputEvent::IsRight);
  EXPECT_FALSE(modifiers & WebInputEvent::IsKeyPad ||
               modifiers & WebInputEvent::IsLeft);

  modifiers =
      getModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationNumpad);
  EXPECT_TRUE(modifiers & WebInputEvent::IsKeyPad);
  EXPECT_FALSE(modifiers & WebInputEvent::IsLeft ||
               modifiers & WebInputEvent::IsRight);
}

TEST(WebInputEventConversionTest, WebMouseEventBuilder) {
  TouchEvent* event = TouchEvent::create();
  WebMouseEventBuilder mouse(0, 0, *event);
  EXPECT_EQ(WebInputEvent::Undefined, mouse.type);
}

TEST(WebInputEventConversionTest, WebTouchEventBuilder) {
  const std::string baseURL("http://www.test0.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  Document* document =
      toLocalFrame(webViewImpl->page()->mainFrame())->document();
  LocalDOMWindow* domWindow = document->domWindow();
  LayoutViewItem documentLayoutView = document->layoutViewItem();

  WebTouchPoint p0, p1;
  p0.id = 1;
  p1.id = 2;
  p0.screenPosition = WebFloatPoint(100.f, 50.f);
  p1.screenPosition = WebFloatPoint(150.f, 25.f);
  p0.position = WebFloatPoint(10.f, 10.f);
  p1.position = WebFloatPoint(5.f, 5.f);
  p0.radiusX = p1.radiusY = 10.f;
  p0.radiusY = p1.radiusX = 5.f;
  p0.rotationAngle = p1.rotationAngle = 1.f;
  p0.force = p1.force = 25.f;

  Touch* touch0 = Touch::create(toLocalFrame(webViewImpl->page()->mainFrame()),
                                document, p0.id, p0.screenPosition, p0.position,
                                FloatSize(p0.radiusX, p0.radiusY),
                                p0.rotationAngle, p0.force, String());
  Touch* touch1 = Touch::create(toLocalFrame(webViewImpl->page()->mainFrame()),
                                document, p1.id, p1.screenPosition, p1.position,
                                FloatSize(p1.radiusX, p1.radiusY),
                                p1.rotationAngle, p1.force, String());

  // Test touchstart.
  {
    TouchList* touchList = TouchList::create();
    touchList->append(touch0);
    TouchEvent* touchEvent = TouchEvent::create(
        touchList, touchList, touchList, EventTypeNames::touchstart, domWindow,
        PlatformEvent::NoModifiers, false, false, true, 0, TouchActionAuto,
        WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(1u, webTouchBuilder.touchesLength);
    EXPECT_EQ(WebInputEvent::TouchStart, webTouchBuilder.type);
    EXPECT_EQ(WebTouchPoint::StatePressed, webTouchBuilder.touches[0].state);
    EXPECT_FLOAT_EQ(p0.screenPosition.x,
                    webTouchBuilder.touches[0].screenPosition.x);
    EXPECT_FLOAT_EQ(p0.screenPosition.y,
                    webTouchBuilder.touches[0].screenPosition.y);
    EXPECT_FLOAT_EQ(p0.position.x, webTouchBuilder.touches[0].position.x);
    EXPECT_FLOAT_EQ(p0.position.y, webTouchBuilder.touches[0].position.y);
    EXPECT_FLOAT_EQ(p0.radiusX, webTouchBuilder.touches[0].radiusX);
    EXPECT_FLOAT_EQ(p0.radiusY, webTouchBuilder.touches[0].radiusY);
    EXPECT_FLOAT_EQ(p0.rotationAngle, webTouchBuilder.touches[0].rotationAngle);
    EXPECT_FLOAT_EQ(p0.force, webTouchBuilder.touches[0].force);
    EXPECT_EQ(WebPointerProperties::PointerType::Touch,
              webTouchBuilder.touches[0].pointerType);
    EXPECT_EQ(WebInputEvent::EventNonBlocking, webTouchBuilder.dispatchType);
  }

  // Test cancelable touchstart.
  {
    TouchList* touchList = TouchList::create();
    touchList->append(touch0);
    TouchEvent* touchEvent = TouchEvent::create(
        touchList, touchList, touchList, EventTypeNames::touchstart, domWindow,
        PlatformEvent::NoModifiers, true, false, true, 0, TouchActionAuto,
        WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    EXPECT_EQ(WebInputEvent::Blocking, webTouchBuilder.dispatchType);
  }

  // Test touchmove.
  {
    TouchList* activeTouchList = TouchList::create();
    TouchList* movedTouchList = TouchList::create();
    activeTouchList->append(touch0);
    activeTouchList->append(touch1);
    movedTouchList->append(touch0);
    TouchEvent* touchEvent = TouchEvent::create(
        activeTouchList, activeTouchList, movedTouchList,
        EventTypeNames::touchmove, domWindow, PlatformEvent::NoModifiers, false,
        false, true, 0, TouchActionAuto,
        WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(2u, webTouchBuilder.touchesLength);
    EXPECT_EQ(WebInputEvent::TouchMove, webTouchBuilder.type);
    EXPECT_EQ(WebTouchPoint::StateMoved, webTouchBuilder.touches[0].state);
    EXPECT_EQ(WebTouchPoint::StateStationary, webTouchBuilder.touches[1].state);
    EXPECT_EQ(p0.id, webTouchBuilder.touches[0].id);
    EXPECT_EQ(p1.id, webTouchBuilder.touches[1].id);
    EXPECT_EQ(WebInputEvent::EventNonBlocking, webTouchBuilder.dispatchType);
  }

  // Test touchmove, different point yields same ordering.
  {
    TouchList* activeTouchList = TouchList::create();
    TouchList* movedTouchList = TouchList::create();
    activeTouchList->append(touch0);
    activeTouchList->append(touch1);
    movedTouchList->append(touch1);
    TouchEvent* touchEvent = TouchEvent::create(
        activeTouchList, activeTouchList, movedTouchList,
        EventTypeNames::touchmove, domWindow, PlatformEvent::NoModifiers, false,
        false, true, 0, TouchActionAuto,
        WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(2u, webTouchBuilder.touchesLength);
    EXPECT_EQ(WebInputEvent::TouchMove, webTouchBuilder.type);
    EXPECT_EQ(WebTouchPoint::StateStationary, webTouchBuilder.touches[0].state);
    EXPECT_EQ(WebTouchPoint::StateMoved, webTouchBuilder.touches[1].state);
    EXPECT_EQ(p0.id, webTouchBuilder.touches[0].id);
    EXPECT_EQ(p1.id, webTouchBuilder.touches[1].id);
    EXPECT_EQ(WebInputEvent::EventNonBlocking, webTouchBuilder.dispatchType);
  }

  // Test touchend.
  {
    TouchList* activeTouchList = TouchList::create();
    TouchList* releasedTouchList = TouchList::create();
    activeTouchList->append(touch0);
    releasedTouchList->append(touch1);
    TouchEvent* touchEvent = TouchEvent::create(
        activeTouchList, activeTouchList, releasedTouchList,
        EventTypeNames::touchend, domWindow, PlatformEvent::NoModifiers, false,
        false, false, 0, TouchActionAuto,
        WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(2u, webTouchBuilder.touchesLength);
    EXPECT_EQ(WebInputEvent::TouchEnd, webTouchBuilder.type);
    EXPECT_EQ(WebTouchPoint::StateStationary, webTouchBuilder.touches[0].state);
    EXPECT_EQ(WebTouchPoint::StateReleased, webTouchBuilder.touches[1].state);
    EXPECT_EQ(p0.id, webTouchBuilder.touches[0].id);
    EXPECT_EQ(p1.id, webTouchBuilder.touches[1].id);
    EXPECT_EQ(WebInputEvent::EventNonBlocking, webTouchBuilder.dispatchType);
  }

  // Test touchcancel.
  {
    TouchList* activeTouchList = TouchList::create();
    TouchList* cancelledTouchList = TouchList::create();
    cancelledTouchList->append(touch0);
    cancelledTouchList->append(touch1);
    TouchEvent* touchEvent = TouchEvent::create(
        activeTouchList, activeTouchList, cancelledTouchList,
        EventTypeNames::touchcancel, domWindow, PlatformEvent::NoModifiers,
        false, false, false, 0, TouchActionAuto,
        WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(2u, webTouchBuilder.touchesLength);
    EXPECT_EQ(WebInputEvent::TouchCancel, webTouchBuilder.type);
    EXPECT_EQ(WebTouchPoint::StateCancelled, webTouchBuilder.touches[0].state);
    EXPECT_EQ(WebTouchPoint::StateCancelled, webTouchBuilder.touches[1].state);
    EXPECT_EQ(p0.id, webTouchBuilder.touches[0].id);
    EXPECT_EQ(p1.id, webTouchBuilder.touches[1].id);
    EXPECT_EQ(WebInputEvent::EventNonBlocking, webTouchBuilder.dispatchType);
  }

  // Test max point limit.
  {
    TouchList* touchList = TouchList::create();
    TouchList* changedTouchList = TouchList::create();
    for (int i = 0; i <= static_cast<int>(WebTouchEvent::kTouchesLengthCap) * 2;
         ++i) {
      Touch* touch = Touch::create(
          toLocalFrame(webViewImpl->page()->mainFrame()), document, i,
          p0.screenPosition, p0.position, FloatSize(p0.radiusX, p0.radiusY),
          p0.rotationAngle, p0.force, String());
      touchList->append(touch);
      changedTouchList->append(touch);
    }
    TouchEvent* touchEvent = TouchEvent::create(
        touchList, touchList, touchList, EventTypeNames::touchstart, domWindow,
        PlatformEvent::NoModifiers, false, false, true, 0, TouchActionAuto,
        WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(static_cast<unsigned>(WebTouchEvent::kTouchesLengthCap),
              webTouchBuilder.touchesLength);
  }
}

TEST(WebInputEventConversionTest, InputEventsScaling) {
  const std::string baseURL("http://www.test1.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  webViewImpl->settings()->setViewportEnabled(true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  webViewImpl->setPageScaleFactor(2);

  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();
  Document* document =
      toLocalFrame(webViewImpl->page()->mainFrame())->document();
  LocalDOMWindow* domWindow = document->domWindow();
  LayoutViewItem documentLayoutView = document->layoutViewItem();

  {
    WebMouseEvent webMouseEvent;
    webMouseEvent.type = WebInputEvent::MouseMove;
    webMouseEvent.x = 10;
    webMouseEvent.y = 10;
    webMouseEvent.windowX = 10;
    webMouseEvent.windowY = 10;
    webMouseEvent.globalX = 10;
    webMouseEvent.globalY = 10;
    webMouseEvent.movementX = 10;
    webMouseEvent.movementY = 10;

    PlatformMouseEventBuilder platformMouseBuilder(view, webMouseEvent);
    EXPECT_EQ(5, platformMouseBuilder.position().x());
    EXPECT_EQ(5, platformMouseBuilder.position().y());
    EXPECT_EQ(10, platformMouseBuilder.globalPosition().x());
    EXPECT_EQ(10, platformMouseBuilder.globalPosition().y());
    EXPECT_EQ(5, platformMouseBuilder.movementDelta().x());
    EXPECT_EQ(5, platformMouseBuilder.movementDelta().y());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureScrollUpdate;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.x = 10;
    webGestureEvent.y = 12;
    webGestureEvent.globalX = 20;
    webGestureEvent.globalY = 22;
    webGestureEvent.data.scrollUpdate.deltaX = 30;
    webGestureEvent.data.scrollUpdate.deltaY = 32;
    webGestureEvent.data.scrollUpdate.velocityX = 40;
    webGestureEvent.data.scrollUpdate.velocityY = 42;
    webGestureEvent.data.scrollUpdate.inertialPhase =
        WebGestureEvent::MomentumPhase;
    webGestureEvent.data.scrollUpdate.preventPropagation = true;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(5, platformGestureBuilder.position().x());
    EXPECT_EQ(6, platformGestureBuilder.position().y());
    EXPECT_EQ(20, platformGestureBuilder.globalPosition().x());
    EXPECT_EQ(22, platformGestureBuilder.globalPosition().y());
    EXPECT_EQ(15, platformGestureBuilder.deltaX());
    EXPECT_EQ(16, platformGestureBuilder.deltaY());
    // TODO: The velocity values may need to be scaled to page scale in
    // order to remain consist with delta values.
    EXPECT_EQ(40, platformGestureBuilder.velocityX());
    EXPECT_EQ(42, platformGestureBuilder.velocityY());
    EXPECT_EQ(ScrollInertialPhaseMomentum,
              platformGestureBuilder.inertialPhase());
    EXPECT_TRUE(platformGestureBuilder.preventPropagation());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureScrollEnd;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.x = 10;
    webGestureEvent.y = 12;
    webGestureEvent.globalX = 20;
    webGestureEvent.globalY = 22;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(5, platformGestureBuilder.position().x());
    EXPECT_EQ(6, platformGestureBuilder.position().y());
    EXPECT_EQ(20, platformGestureBuilder.globalPosition().x());
    EXPECT_EQ(22, platformGestureBuilder.globalPosition().y());
    EXPECT_EQ(ScrollInertialPhaseUnknown,
              platformGestureBuilder.inertialPhase());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureTap;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.tap.width = 10;
    webGestureEvent.data.tap.height = 10;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(5, platformGestureBuilder.area().width());
    EXPECT_EQ(5, platformGestureBuilder.area().height());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureTapUnconfirmed;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.tap.width = 10;
    webGestureEvent.data.tap.height = 10;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(5, platformGestureBuilder.area().width());
    EXPECT_EQ(5, platformGestureBuilder.area().height());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureTapDown;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.tapDown.width = 10;
    webGestureEvent.data.tapDown.height = 10;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(5, platformGestureBuilder.area().width());
    EXPECT_EQ(5, platformGestureBuilder.area().height());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureShowPress;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.showPress.width = 10;
    webGestureEvent.data.showPress.height = 10;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(5, platformGestureBuilder.area().width());
    EXPECT_EQ(5, platformGestureBuilder.area().height());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureLongPress;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.longPress.width = 10;
    webGestureEvent.data.longPress.height = 10;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(5, platformGestureBuilder.area().width());
    EXPECT_EQ(5, platformGestureBuilder.area().height());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureTwoFingerTap;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.twoFingerTap.firstFingerWidth = 10;
    webGestureEvent.data.twoFingerTap.firstFingerHeight = 10;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(5, platformGestureBuilder.area().width());
    EXPECT_EQ(5, platformGestureBuilder.area().height());
  }

  {
    WebTouchEvent webTouchEvent;
    webTouchEvent.type = WebInputEvent::TouchMove;
    webTouchEvent.touchesLength = 1;
    webTouchEvent.touches[0].state = WebTouchPoint::StateMoved;
    webTouchEvent.touches[0].screenPosition.x = 10.6f;
    webTouchEvent.touches[0].screenPosition.y = 10.4f;
    webTouchEvent.touches[0].position.x = 10.6f;
    webTouchEvent.touches[0].position.y = 10.4f;
    webTouchEvent.touches[0].radiusX = 10.6f;
    webTouchEvent.touches[0].radiusY = 10.4f;

    EXPECT_FLOAT_EQ(10.6f, webTouchEvent.touches[0].screenPosition.x);
    EXPECT_FLOAT_EQ(10.4f, webTouchEvent.touches[0].screenPosition.y);
    EXPECT_FLOAT_EQ(10.6f, webTouchEvent.touches[0].position.x);
    EXPECT_FLOAT_EQ(10.4f, webTouchEvent.touches[0].position.y);
    EXPECT_FLOAT_EQ(10.6f, webTouchEvent.touches[0].radiusX);
    EXPECT_FLOAT_EQ(10.4f, webTouchEvent.touches[0].radiusY);

    PlatformTouchEventBuilder platformTouchBuilder(view, webTouchEvent);
    EXPECT_FLOAT_EQ(10.6f,
                    platformTouchBuilder.touchPoints()[0].screenPos().x());
    EXPECT_FLOAT_EQ(10.4f,
                    platformTouchBuilder.touchPoints()[0].screenPos().y());
    EXPECT_FLOAT_EQ(5.3f, platformTouchBuilder.touchPoints()[0].pos().x());
    EXPECT_FLOAT_EQ(5.2f, platformTouchBuilder.touchPoints()[0].pos().y());
    EXPECT_FLOAT_EQ(5.3f,
                    platformTouchBuilder.touchPoints()[0].radius().width());
    EXPECT_FLOAT_EQ(5.2f,
                    platformTouchBuilder.touchPoints()[0].radius().height());
  }

  // Reverse builders should *not* go back to physical pixels, as they are used
  // for plugins which expect CSS pixel coordinates.
  {
    PlatformMouseEvent platformMouseEvent(
        IntPoint(10, 10), IntPoint(10, 10), WebPointerProperties::Button::Left,
        PlatformEvent::MouseMoved, 1, PlatformEvent::NoModifiers,
        PlatformMouseEvent::RealOrIndistinguishable, 0);
    MouseEvent* mouseEvent = MouseEvent::create(
        EventTypeNames::mousemove, domWindow, platformMouseEvent, 0, document);
    WebMouseEventBuilder webMouseBuilder(view, documentLayoutView, *mouseEvent);

    EXPECT_EQ(10, webMouseBuilder.x);
    EXPECT_EQ(10, webMouseBuilder.y);
    EXPECT_EQ(10, webMouseBuilder.globalX);
    EXPECT_EQ(10, webMouseBuilder.globalY);
    EXPECT_EQ(10, webMouseBuilder.windowX);
    EXPECT_EQ(10, webMouseBuilder.windowY);
  }

  {
    PlatformMouseEvent platformMouseEvent(
        IntPoint(10, 10), IntPoint(10, 10),
        WebPointerProperties::Button::NoButton, PlatformEvent::MouseMoved, 1,
        PlatformEvent::NoModifiers, PlatformMouseEvent::RealOrIndistinguishable,
        0);
    MouseEvent* mouseEvent = MouseEvent::create(
        EventTypeNames::mousemove, domWindow, platformMouseEvent, 0, document);
    WebMouseEventBuilder webMouseBuilder(view, documentLayoutView, *mouseEvent);
    EXPECT_EQ(WebMouseEvent::Button::NoButton, webMouseBuilder.button);
  }

  {
    PlatformGestureEvent platformGestureEvent(
        PlatformEvent::GestureScrollUpdate, IntPoint(10, 12), IntPoint(20, 22),
        IntSize(25, 27), 0, PlatformEvent::NoModifiers,
        PlatformGestureSourceTouchscreen);
    platformGestureEvent.setScrollGestureData(30, 32, ScrollByPrecisePixel, 40,
                                              42, ScrollInertialPhaseMomentum,
                                              true, -1 /* null plugin id */);
    // FIXME: GestureEvent does not preserve velocityX, velocityY,
    // or preventPropagation. It also fails to scale
    // coordinates (x, y, deltaX, deltaY) to the page scale. This
    // may lead to unexpected bugs if a PlatformGestureEvent is
    // transformed into WebGestureEvent and back.
    GestureEvent* gestureEvent =
        GestureEvent::create(domWindow, platformGestureEvent);
    WebGestureEventBuilder webGestureBuilder(documentLayoutView, *gestureEvent);

    EXPECT_EQ(10, webGestureBuilder.x);
    EXPECT_EQ(12, webGestureBuilder.y);
    EXPECT_EQ(20, webGestureBuilder.globalX);
    EXPECT_EQ(22, webGestureBuilder.globalY);
    EXPECT_EQ(30, webGestureBuilder.data.scrollUpdate.deltaX);
    EXPECT_EQ(32, webGestureBuilder.data.scrollUpdate.deltaY);
    EXPECT_EQ(0, webGestureBuilder.data.scrollUpdate.velocityX);
    EXPECT_EQ(0, webGestureBuilder.data.scrollUpdate.velocityY);
    EXPECT_EQ(WebGestureEvent::MomentumPhase,
              webGestureBuilder.data.scrollUpdate.inertialPhase);
    EXPECT_FALSE(webGestureBuilder.data.scrollUpdate.preventPropagation);
    EXPECT_EQ(WebGestureDeviceTouchscreen, webGestureBuilder.sourceDevice);
  }

  {
    Touch* touch =
        Touch::create(toLocalFrame(webViewImpl->page()->mainFrame()), document,
                      0, FloatPoint(10, 9.5), FloatPoint(3.5, 2),
                      FloatSize(4, 4.5), 0, 0, String());
    TouchList* touchList = TouchList::create();
    touchList->append(touch);
    TouchEvent* touchEvent = TouchEvent::create(
        touchList, touchList, touchList, EventTypeNames::touchmove, domWindow,
        PlatformEvent::NoModifiers, false, false, true, 0, TouchActionAuto,
        WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(1u, webTouchBuilder.touchesLength);
    EXPECT_EQ(10, webTouchBuilder.touches[0].screenPosition.x);
    EXPECT_FLOAT_EQ(9.5, webTouchBuilder.touches[0].screenPosition.y);
    EXPECT_FLOAT_EQ(3.5, webTouchBuilder.touches[0].position.x);
    EXPECT_FLOAT_EQ(2, webTouchBuilder.touches[0].position.y);
    EXPECT_FLOAT_EQ(4, webTouchBuilder.touches[0].radiusX);
    EXPECT_FLOAT_EQ(4.5, webTouchBuilder.touches[0].radiusY);
    EXPECT_EQ(WebInputEvent::EventNonBlocking, webTouchBuilder.dispatchType);
  }
}

TEST(WebInputEventConversionTest, InputEventsTransform) {
  const std::string baseURL("http://www.test2.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  webViewImpl->settings()->setViewportEnabled(true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  webViewImpl->setPageScaleFactor(2);
  webViewImpl->mainFrameImpl()->setInputEventsTransformForEmulation(
      IntSize(10, 20), 1.5);

  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();

  {
    WebMouseEvent webMouseEvent;
    webMouseEvent.type = WebInputEvent::MouseMove;
    webMouseEvent.x = 100;
    webMouseEvent.y = 110;
    webMouseEvent.windowX = 100;
    webMouseEvent.windowY = 110;
    webMouseEvent.globalX = 100;
    webMouseEvent.globalY = 110;
    webMouseEvent.movementX = 60;
    webMouseEvent.movementY = 60;

    PlatformMouseEventBuilder platformMouseBuilder(view, webMouseEvent);
    EXPECT_EQ(30, platformMouseBuilder.position().x());
    EXPECT_EQ(30, platformMouseBuilder.position().y());
    EXPECT_EQ(100, platformMouseBuilder.globalPosition().x());
    EXPECT_EQ(110, platformMouseBuilder.globalPosition().y());
    EXPECT_EQ(20, platformMouseBuilder.movementDelta().x());
    EXPECT_EQ(20, platformMouseBuilder.movementDelta().y());
  }

  {
    WebMouseEvent webMouseEvent1;
    webMouseEvent1.type = WebInputEvent::MouseMove;
    webMouseEvent1.x = 100;
    webMouseEvent1.y = 110;
    webMouseEvent1.windowX = 100;
    webMouseEvent1.windowY = 110;
    webMouseEvent1.globalX = 100;
    webMouseEvent1.globalY = 110;
    webMouseEvent1.movementX = 60;
    webMouseEvent1.movementY = 60;

    WebMouseEvent webMouseEvent2 = webMouseEvent1;
    webMouseEvent2.y = 140;
    webMouseEvent2.windowY = 140;
    webMouseEvent2.globalY = 140;
    webMouseEvent2.movementY = 30;

    std::vector<const WebInputEvent*> events;
    events.push_back(&webMouseEvent1);
    events.push_back(&webMouseEvent2);

    Vector<PlatformMouseEvent> coalescedevents =
        createPlatformMouseEventVector(view, events);
    EXPECT_EQ(events.size(), coalescedevents.size());

    EXPECT_EQ(30, coalescedevents[0].position().x());
    EXPECT_EQ(30, coalescedevents[0].position().y());
    EXPECT_EQ(100, coalescedevents[0].globalPosition().x());
    EXPECT_EQ(110, coalescedevents[0].globalPosition().y());
    EXPECT_EQ(20, coalescedevents[0].movementDelta().x());
    EXPECT_EQ(20, coalescedevents[0].movementDelta().y());

    EXPECT_EQ(30, coalescedevents[1].position().x());
    EXPECT_EQ(40, coalescedevents[1].position().y());
    EXPECT_EQ(100, coalescedevents[1].globalPosition().x());
    EXPECT_EQ(140, coalescedevents[1].globalPosition().y());
    EXPECT_EQ(20, coalescedevents[1].movementDelta().x());
    EXPECT_EQ(10, coalescedevents[1].movementDelta().y());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureScrollUpdate;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.x = 100;
    webGestureEvent.y = 110;
    webGestureEvent.globalX = 100;
    webGestureEvent.globalY = 110;
    webGestureEvent.data.scrollUpdate.deltaX = 60;
    webGestureEvent.data.scrollUpdate.deltaY = 60;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(30, platformGestureBuilder.position().x());
    EXPECT_EQ(30, platformGestureBuilder.position().y());
    EXPECT_EQ(100, platformGestureBuilder.globalPosition().x());
    EXPECT_EQ(110, platformGestureBuilder.globalPosition().y());
    EXPECT_EQ(20, platformGestureBuilder.deltaX());
    EXPECT_EQ(20, platformGestureBuilder.deltaY());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureTap;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.tap.width = 30;
    webGestureEvent.data.tap.height = 30;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(10, platformGestureBuilder.area().width());
    EXPECT_EQ(10, platformGestureBuilder.area().height());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureTapUnconfirmed;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.tap.width = 30;
    webGestureEvent.data.tap.height = 30;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(10, platformGestureBuilder.area().width());
    EXPECT_EQ(10, platformGestureBuilder.area().height());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureTapDown;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.tapDown.width = 30;
    webGestureEvent.data.tapDown.height = 30;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(10, platformGestureBuilder.area().width());
    EXPECT_EQ(10, platformGestureBuilder.area().height());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureShowPress;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.showPress.width = 30;
    webGestureEvent.data.showPress.height = 30;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(10, platformGestureBuilder.area().width());
    EXPECT_EQ(10, platformGestureBuilder.area().height());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureLongPress;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.longPress.width = 30;
    webGestureEvent.data.longPress.height = 30;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(10, platformGestureBuilder.area().width());
    EXPECT_EQ(10, platformGestureBuilder.area().height());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureTwoFingerTap;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.twoFingerTap.firstFingerWidth = 30;
    webGestureEvent.data.twoFingerTap.firstFingerHeight = 30;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(10, platformGestureBuilder.area().width());
    EXPECT_EQ(10, platformGestureBuilder.area().height());
  }

  {
    WebTouchEvent webTouchEvent;
    webTouchEvent.type = WebInputEvent::TouchMove;
    webTouchEvent.touchesLength = 1;
    webTouchEvent.touches[0].state = WebTouchPoint::StateMoved;
    webTouchEvent.touches[0].screenPosition.x = 100;
    webTouchEvent.touches[0].screenPosition.y = 110;
    webTouchEvent.touches[0].position.x = 100;
    webTouchEvent.touches[0].position.y = 110;
    webTouchEvent.touches[0].radiusX = 30;
    webTouchEvent.touches[0].radiusY = 30;

    PlatformTouchEventBuilder platformTouchBuilder(view, webTouchEvent);
    EXPECT_FLOAT_EQ(100, platformTouchBuilder.touchPoints()[0].screenPos().x());
    EXPECT_FLOAT_EQ(110, platformTouchBuilder.touchPoints()[0].screenPos().y());
    EXPECT_FLOAT_EQ(30, platformTouchBuilder.touchPoints()[0].pos().x());
    EXPECT_FLOAT_EQ(30, platformTouchBuilder.touchPoints()[0].pos().y());
    EXPECT_FLOAT_EQ(10, platformTouchBuilder.touchPoints()[0].radius().width());
    EXPECT_FLOAT_EQ(10,
                    platformTouchBuilder.touchPoints()[0].radius().height());
  }

  {
    WebTouchEvent webTouchEvent1;
    webTouchEvent1.type = WebInputEvent::TouchMove;
    webTouchEvent1.touchesLength = 1;
    webTouchEvent1.touches[0].state = WebTouchPoint::StateMoved;
    webTouchEvent1.touches[0].screenPosition.x = 100;
    webTouchEvent1.touches[0].screenPosition.y = 110;
    webTouchEvent1.touches[0].position.x = 100;
    webTouchEvent1.touches[0].position.y = 110;
    webTouchEvent1.touches[0].radiusX = 30;
    webTouchEvent1.touches[0].radiusY = 30;

    WebTouchEvent webTouchEvent2 = webTouchEvent1;
    webTouchEvent2.touches[0].screenPosition.x = 130;
    webTouchEvent2.touches[0].position.x = 130;
    webTouchEvent2.touches[0].radiusX = 60;

    std::vector<const WebInputEvent*> events;
    events.push_back(&webTouchEvent1);
    events.push_back(&webTouchEvent2);

    Vector<PlatformTouchEvent> coalescedevents =
        createPlatformTouchEventVector(view, events);
    EXPECT_EQ(events.size(), coalescedevents.size());

    EXPECT_FLOAT_EQ(100, coalescedevents[0].touchPoints()[0].screenPos().x());
    EXPECT_FLOAT_EQ(110, coalescedevents[0].touchPoints()[0].screenPos().y());
    EXPECT_FLOAT_EQ(30, coalescedevents[0].touchPoints()[0].pos().x());
    EXPECT_FLOAT_EQ(30, coalescedevents[0].touchPoints()[0].pos().y());
    EXPECT_FLOAT_EQ(10, coalescedevents[0].touchPoints()[0].radius().width());
    EXPECT_FLOAT_EQ(10, coalescedevents[0].touchPoints()[0].radius().height());

    EXPECT_FLOAT_EQ(130, coalescedevents[1].touchPoints()[0].screenPos().x());
    EXPECT_FLOAT_EQ(110, coalescedevents[1].touchPoints()[0].screenPos().y());
    EXPECT_FLOAT_EQ(40, coalescedevents[1].touchPoints()[0].pos().x());
    EXPECT_FLOAT_EQ(30, coalescedevents[1].touchPoints()[0].pos().y());
    EXPECT_FLOAT_EQ(20, coalescedevents[1].touchPoints()[0].radius().width());
    EXPECT_FLOAT_EQ(10, coalescedevents[1].touchPoints()[0].radius().height());
  }
}

TEST(WebInputEventConversionTest, InputEventsConversions) {
  const std::string baseURL("http://www.test3.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();
  Document* document =
      toLocalFrame(webViewImpl->page()->mainFrame())->document();
  LocalDOMWindow* domWindow = document->domWindow();
  LayoutViewItem documentLayoutView = document->layoutViewItem();

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureTap;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.x = 10;
    webGestureEvent.y = 10;
    webGestureEvent.globalX = 10;
    webGestureEvent.globalY = 10;
    webGestureEvent.data.tap.tapCount = 1;
    webGestureEvent.data.tap.width = 10;
    webGestureEvent.data.tap.height = 10;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(10.f, platformGestureBuilder.position().x());
    EXPECT_EQ(10.f, platformGestureBuilder.position().y());
    EXPECT_EQ(10.f, platformGestureBuilder.globalPosition().x());
    EXPECT_EQ(10.f, platformGestureBuilder.globalPosition().y());
    EXPECT_EQ(1, platformGestureBuilder.tapCount());

    GestureEvent* coreGestureEvent =
        GestureEvent::create(domWindow, platformGestureBuilder);
    WebGestureEventBuilder recreatedWebGestureEvent(documentLayoutView,
                                                    *coreGestureEvent);
    EXPECT_EQ(webGestureEvent.type, recreatedWebGestureEvent.type);
    EXPECT_EQ(webGestureEvent.x, recreatedWebGestureEvent.x);
    EXPECT_EQ(webGestureEvent.y, recreatedWebGestureEvent.y);
    EXPECT_EQ(webGestureEvent.globalX, recreatedWebGestureEvent.globalX);
    EXPECT_EQ(webGestureEvent.globalY, recreatedWebGestureEvent.globalY);
    EXPECT_EQ(webGestureEvent.data.tap.tapCount,
              recreatedWebGestureEvent.data.tap.tapCount);
  }
}

TEST(WebInputEventConversionTest, VisualViewportOffset) {
  const std::string baseURL("http://www.test4.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  webViewImpl->setPageScaleFactor(2);

  IntPoint visualOffset(35, 60);
  webViewImpl->page()->frameHost().visualViewport().setLocation(visualOffset);

  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();

  {
    WebMouseEvent webMouseEvent;
    webMouseEvent.type = WebInputEvent::MouseMove;
    webMouseEvent.x = 10;
    webMouseEvent.y = 10;
    webMouseEvent.windowX = 10;
    webMouseEvent.windowY = 10;
    webMouseEvent.globalX = 10;
    webMouseEvent.globalY = 10;

    PlatformMouseEventBuilder platformMouseBuilder(view, webMouseEvent);
    EXPECT_EQ(5 + visualOffset.x(), platformMouseBuilder.position().x());
    EXPECT_EQ(5 + visualOffset.y(), platformMouseBuilder.position().y());
    EXPECT_EQ(10, platformMouseBuilder.globalPosition().x());
    EXPECT_EQ(10, platformMouseBuilder.globalPosition().y());
  }

  {
    WebMouseWheelEvent webMouseWheelEvent;
    webMouseWheelEvent.type = WebInputEvent::MouseWheel;
    webMouseWheelEvent.x = 10;
    webMouseWheelEvent.y = 10;
    webMouseWheelEvent.windowX = 10;
    webMouseWheelEvent.windowY = 10;
    webMouseWheelEvent.globalX = 10;
    webMouseWheelEvent.globalY = 10;

    PlatformWheelEventBuilder platformWheelBuilder(view, webMouseWheelEvent);
    EXPECT_EQ(5 + visualOffset.x(), platformWheelBuilder.position().x());
    EXPECT_EQ(5 + visualOffset.y(), platformWheelBuilder.position().y());
    EXPECT_EQ(10, platformWheelBuilder.globalPosition().x());
    EXPECT_EQ(10, platformWheelBuilder.globalPosition().y());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureScrollUpdate;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.x = 10;
    webGestureEvent.y = 10;
    webGestureEvent.globalX = 10;
    webGestureEvent.globalY = 10;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(5 + visualOffset.x(), platformGestureBuilder.position().x());
    EXPECT_EQ(5 + visualOffset.y(), platformGestureBuilder.position().y());
    EXPECT_EQ(10, platformGestureBuilder.globalPosition().x());
    EXPECT_EQ(10, platformGestureBuilder.globalPosition().y());
  }

  {
    WebTouchEvent webTouchEvent;
    webTouchEvent.type = WebInputEvent::TouchMove;
    webTouchEvent.touchesLength = 1;
    webTouchEvent.touches[0].state = WebTouchPoint::StateMoved;
    webTouchEvent.touches[0].screenPosition.x = 10.6f;
    webTouchEvent.touches[0].screenPosition.y = 10.4f;
    webTouchEvent.touches[0].position.x = 10.6f;
    webTouchEvent.touches[0].position.y = 10.4f;

    EXPECT_FLOAT_EQ(10.6f, webTouchEvent.touches[0].screenPosition.x);
    EXPECT_FLOAT_EQ(10.4f, webTouchEvent.touches[0].screenPosition.y);
    EXPECT_FLOAT_EQ(10.6f, webTouchEvent.touches[0].position.x);
    EXPECT_FLOAT_EQ(10.4f, webTouchEvent.touches[0].position.y);

    PlatformTouchEventBuilder platformTouchBuilder(view, webTouchEvent);
    EXPECT_FLOAT_EQ(10.6f,
                    platformTouchBuilder.touchPoints()[0].screenPos().x());
    EXPECT_FLOAT_EQ(10.4f,
                    platformTouchBuilder.touchPoints()[0].screenPos().y());
    EXPECT_FLOAT_EQ(5.3f + visualOffset.x(),
                    platformTouchBuilder.touchPoints()[0].pos().x());
    EXPECT_FLOAT_EQ(5.2f + visualOffset.y(),
                    platformTouchBuilder.touchPoints()[0].pos().y());
  }
}

TEST(WebInputEventConversionTest, ElasticOverscroll) {
  const std::string baseURL("http://www.test5.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();

  FloatSize elasticOverscroll(10, -20);
  webViewImpl->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                   elasticOverscroll, 1.0f, 0.0f);

  // Just elastic overscroll.
  {
    WebMouseEvent webMouseEvent;
    webMouseEvent.type = WebInputEvent::MouseMove;
    webMouseEvent.x = 10;
    webMouseEvent.y = 50;
    webMouseEvent.windowX = 10;
    webMouseEvent.windowY = 50;
    webMouseEvent.globalX = 10;
    webMouseEvent.globalY = 50;

    PlatformMouseEventBuilder platformMouseBuilder(view, webMouseEvent);
    EXPECT_EQ(webMouseEvent.x + elasticOverscroll.width(),
              platformMouseBuilder.position().x());
    EXPECT_EQ(webMouseEvent.y + elasticOverscroll.height(),
              platformMouseBuilder.position().y());
    EXPECT_EQ(webMouseEvent.globalX, platformMouseBuilder.globalPosition().x());
    EXPECT_EQ(webMouseEvent.globalY, platformMouseBuilder.globalPosition().y());
  }

  // Elastic overscroll and pinch-zoom (this doesn't actually ever happen,
  // but ensure that if it were to, the overscroll would be applied after the
  // pinch-zoom).
  float pageScale = 2;
  webViewImpl->setPageScaleFactor(pageScale);
  IntPoint visualOffset(35, 60);
  webViewImpl->page()->frameHost().visualViewport().setLocation(visualOffset);
  {
    WebMouseEvent webMouseEvent;
    webMouseEvent.type = WebInputEvent::MouseMove;
    webMouseEvent.x = 10;
    webMouseEvent.y = 10;
    webMouseEvent.windowX = 10;
    webMouseEvent.windowY = 10;
    webMouseEvent.globalX = 10;
    webMouseEvent.globalY = 10;

    PlatformMouseEventBuilder platformMouseBuilder(view, webMouseEvent);
    EXPECT_EQ(webMouseEvent.x / pageScale + visualOffset.x() +
                  elasticOverscroll.width(),
              platformMouseBuilder.position().x());
    EXPECT_EQ(webMouseEvent.y / pageScale + visualOffset.y() +
                  elasticOverscroll.height(),
              platformMouseBuilder.position().y());
    EXPECT_EQ(webMouseEvent.globalX, platformMouseBuilder.globalPosition().x());
    EXPECT_EQ(webMouseEvent.globalY, platformMouseBuilder.globalPosition().y());
  }
}

// Page reload/navigation should not reset elastic overscroll.
TEST(WebInputEventConversionTest, ElasticOverscrollWithPageReload) {
  const std::string baseURL("http://www.test6.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  FloatSize elasticOverscroll(10, -20);
  webViewImpl->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                   elasticOverscroll, 1.0f, 0.0f);
  FrameTestHelpers::reloadFrame(webViewHelper.webView()->mainFrame());
  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();

  // Just elastic overscroll.
  {
    WebMouseEvent webMouseEvent;
    webMouseEvent.type = WebInputEvent::MouseMove;
    webMouseEvent.x = 10;
    webMouseEvent.y = 50;
    webMouseEvent.windowX = 10;
    webMouseEvent.windowY = 50;
    webMouseEvent.globalX = 10;
    webMouseEvent.globalY = 50;

    PlatformMouseEventBuilder platformMouseBuilder(view, webMouseEvent);
    EXPECT_EQ(webMouseEvent.x + elasticOverscroll.width(),
              platformMouseBuilder.position().x());
    EXPECT_EQ(webMouseEvent.y + elasticOverscroll.height(),
              platformMouseBuilder.position().y());
    EXPECT_EQ(webMouseEvent.globalX, platformMouseBuilder.globalPosition().x());
    EXPECT_EQ(webMouseEvent.globalY, platformMouseBuilder.globalPosition().y());
  }
}

TEST(WebInputEventConversionTest, WebMouseWheelEventBuilder) {
  const std::string baseURL("http://www.test7.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  Document* document =
      toLocalFrame(webViewImpl->page()->mainFrame())->document();
  {
    WheelEvent* event = WheelEvent::create(
        FloatPoint(1, 3), FloatPoint(5, 10), WheelEvent::kDomDeltaPage,
        document->domWindow(), IntPoint(2, 6), IntPoint(10, 30),
        PlatformEvent::CtrlKey, 0, 0, -1 /* null plugin id */,
        true /* hasPreciseScrollingDeltas */, Event::RailsModeHorizontal,
        true /*cancelable*/
#if OS(MACOSX)
        ,
        WheelEventPhaseBegan, WheelEventPhaseChanged
#endif
        );
    WebMouseWheelEventBuilder webMouseWheel(
        toLocalFrame(webViewImpl->page()->mainFrame())->view(),
        document->layoutViewItem(), *event);
    EXPECT_EQ(1, webMouseWheel.wheelTicksX);
    EXPECT_EQ(3, webMouseWheel.wheelTicksY);
    EXPECT_EQ(5, webMouseWheel.deltaX);
    EXPECT_EQ(10, webMouseWheel.deltaY);
    EXPECT_EQ(2, webMouseWheel.globalX);
    EXPECT_EQ(6, webMouseWheel.globalY);
    EXPECT_EQ(10, webMouseWheel.windowX);
    EXPECT_EQ(30, webMouseWheel.windowY);
    EXPECT_TRUE(webMouseWheel.scrollByPage);
    EXPECT_EQ(WebInputEvent::ControlKey, webMouseWheel.modifiers);
    EXPECT_EQ(WebInputEvent::RailsModeHorizontal, webMouseWheel.railsMode);
    EXPECT_TRUE(webMouseWheel.hasPreciseScrollingDeltas);
    EXPECT_EQ(WebInputEvent::Blocking, webMouseWheel.dispatchType);
#if OS(MACOSX)
    EXPECT_EQ(WebMouseWheelEvent::PhaseBegan, webMouseWheel.phase);
    EXPECT_EQ(WebMouseWheelEvent::PhaseChanged, webMouseWheel.momentumPhase);
#endif
  }

  {
    WheelEvent* event = WheelEvent::create(
        FloatPoint(1, 3), FloatPoint(5, 10), WheelEvent::kDomDeltaPage,
        document->domWindow(), IntPoint(2, 6), IntPoint(10, 30),
        PlatformEvent::CtrlKey, 0, 0, -1 /* null plugin id */,
        true /* hasPreciseScrollingDeltas */, Event::RailsModeHorizontal, false
#if OS(MACOSX)
        ,
        WheelEventPhaseNone, WheelEventPhaseNone
#endif
        );
    WebMouseWheelEventBuilder webMouseWheel(
        toLocalFrame(webViewImpl->page()->mainFrame())->view(),
        document->layoutViewItem(), *event);
    EXPECT_EQ(WebInputEvent::EventNonBlocking, webMouseWheel.dispatchType);
  }
}

TEST(WebInputEventConversionTest, PlatformWheelEventBuilder) {
  const std::string baseURL("http://www.test8.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();

  {
    WebMouseWheelEvent webMouseWheelEvent;
    webMouseWheelEvent.type = WebInputEvent::MouseWheel;
    webMouseWheelEvent.x = 0;
    webMouseWheelEvent.y = 5;
    webMouseWheelEvent.deltaX = 10;
    webMouseWheelEvent.deltaY = 15;
    webMouseWheelEvent.modifiers = WebInputEvent::ControlKey;
    webMouseWheelEvent.hasPreciseScrollingDeltas = true;
    webMouseWheelEvent.railsMode = WebInputEvent::RailsModeHorizontal;
    webMouseWheelEvent.phase = WebMouseWheelEvent::PhaseBegan;
    webMouseWheelEvent.momentumPhase = WebMouseWheelEvent::PhaseChanged;

    PlatformWheelEventBuilder platformWheelBuilder(view, webMouseWheelEvent);
    EXPECT_EQ(0, platformWheelBuilder.position().x());
    EXPECT_EQ(5, platformWheelBuilder.position().y());
    EXPECT_EQ(10, platformWheelBuilder.deltaX());
    EXPECT_EQ(15, platformWheelBuilder.deltaY());
    EXPECT_EQ(PlatformEvent::CtrlKey, platformWheelBuilder.getModifiers());
    EXPECT_TRUE(platformWheelBuilder.hasPreciseScrollingDeltas());
    EXPECT_EQ(platformWheelBuilder.getRailsMode(),
              PlatformEvent::RailsModeHorizontal);
#if OS(MACOSX)
    EXPECT_EQ(PlatformWheelEventPhaseBegan, platformWheelBuilder.phase());
    EXPECT_EQ(PlatformWheelEventPhaseChanged,
              platformWheelBuilder.momentumPhase());
#endif
  }

  {
    WebMouseWheelEvent webMouseWheelEvent;
    webMouseWheelEvent.type = WebInputEvent::MouseWheel;
    webMouseWheelEvent.x = 5;
    webMouseWheelEvent.y = 0;
    webMouseWheelEvent.deltaX = 15;
    webMouseWheelEvent.deltaY = 10;
    webMouseWheelEvent.modifiers = WebInputEvent::ShiftKey;
    webMouseWheelEvent.hasPreciseScrollingDeltas = false;
    webMouseWheelEvent.railsMode = WebInputEvent::RailsModeFree;
    webMouseWheelEvent.phase = WebMouseWheelEvent::PhaseNone;
    webMouseWheelEvent.momentumPhase = WebMouseWheelEvent::PhaseNone;

    PlatformWheelEventBuilder platformWheelBuilder(view, webMouseWheelEvent);
    EXPECT_EQ(5, platformWheelBuilder.position().x());
    EXPECT_EQ(0, platformWheelBuilder.position().y());
    EXPECT_EQ(15, platformWheelBuilder.deltaX());
    EXPECT_EQ(10, platformWheelBuilder.deltaY());
    EXPECT_EQ(PlatformEvent::ShiftKey, platformWheelBuilder.getModifiers());
    EXPECT_FALSE(platformWheelBuilder.hasPreciseScrollingDeltas());
    EXPECT_EQ(platformWheelBuilder.getRailsMode(),
              PlatformEvent::RailsModeFree);
#if OS(MACOSX)
    EXPECT_EQ(PlatformWheelEventPhaseNone, platformWheelBuilder.phase());
    EXPECT_EQ(PlatformWheelEventPhaseNone,
              platformWheelBuilder.momentumPhase());
#endif
  }

  {
    WebMouseWheelEvent webMouseWheelEvent;
    webMouseWheelEvent.type = WebInputEvent::MouseWheel;
    webMouseWheelEvent.x = 5;
    webMouseWheelEvent.y = 0;
    webMouseWheelEvent.deltaX = 15;
    webMouseWheelEvent.deltaY = 10;
    webMouseWheelEvent.modifiers = WebInputEvent::AltKey;
    webMouseWheelEvent.hasPreciseScrollingDeltas = true;
    webMouseWheelEvent.railsMode = WebInputEvent::RailsModeVertical;
    webMouseWheelEvent.phase = WebMouseWheelEvent::PhaseNone;
    webMouseWheelEvent.momentumPhase = WebMouseWheelEvent::PhaseNone;

    PlatformWheelEventBuilder platformWheelBuilder(view, webMouseWheelEvent);
    EXPECT_EQ(5, platformWheelBuilder.position().x());
    EXPECT_EQ(0, platformWheelBuilder.position().y());
    EXPECT_EQ(15, platformWheelBuilder.deltaX());
    EXPECT_EQ(10, platformWheelBuilder.deltaY());
    EXPECT_EQ(PlatformEvent::AltKey, platformWheelBuilder.getModifiers());
    EXPECT_TRUE(platformWheelBuilder.hasPreciseScrollingDeltas());
    EXPECT_EQ(platformWheelBuilder.getRailsMode(),
              PlatformEvent::RailsModeVertical);
#if OS(MACOSX)
    EXPECT_EQ(PlatformWheelEventPhaseNone, platformWheelBuilder.phase());
    EXPECT_EQ(PlatformWheelEventPhaseNone,
              platformWheelBuilder.momentumPhase());
#endif
  }
}

TEST(WebInputEventConversionTest, PlatformGestureEventBuilder) {
  const std::string baseURL("http://www.test9.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureScrollBegin;
    webGestureEvent.x = 0;
    webGestureEvent.y = 5;
    webGestureEvent.globalX = 10;
    webGestureEvent.globalY = 15;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchpad;
    webGestureEvent.resendingPluginId = 2;
    webGestureEvent.data.scrollBegin.inertialPhase =
        WebGestureEvent::MomentumPhase;
    webGestureEvent.data.scrollBegin.synthetic = true;
    webGestureEvent.data.scrollBegin.deltaXHint = 100;
    webGestureEvent.data.scrollBegin.deltaYHint = 10;
    webGestureEvent.data.scrollBegin.deltaHintUnits = WebGestureEvent::Pixels;
    webGestureEvent.uniqueTouchEventId = 12345U;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(PlatformGestureSourceTouchpad, platformGestureBuilder.source());
    EXPECT_EQ(2, platformGestureBuilder.resendingPluginId());
    EXPECT_EQ(0, platformGestureBuilder.position().x());
    EXPECT_EQ(5, platformGestureBuilder.position().y());
    EXPECT_EQ(10, platformGestureBuilder.globalPosition().x());
    EXPECT_EQ(15, platformGestureBuilder.globalPosition().y());
    EXPECT_EQ(ScrollInertialPhaseMomentum,
              platformGestureBuilder.inertialPhase());
    EXPECT_TRUE(platformGestureBuilder.synthetic());
    EXPECT_EQ(100, platformGestureBuilder.deltaX());
    EXPECT_EQ(10, platformGestureBuilder.deltaY());
    EXPECT_EQ(ScrollGranularity::ScrollByPixel,
              platformGestureBuilder.deltaUnits());
    EXPECT_EQ(12345U, platformGestureBuilder.uniqueTouchEventId());
  }

  {
    WebGestureEvent webGestureEvent;
    webGestureEvent.type = WebInputEvent::GestureScrollEnd;
    webGestureEvent.x = 0;
    webGestureEvent.y = 5;
    webGestureEvent.globalX = 10;
    webGestureEvent.globalY = 15;
    webGestureEvent.sourceDevice = WebGestureDeviceTouchpad;
    webGestureEvent.resendingPluginId = 2;
    webGestureEvent.data.scrollEnd.inertialPhase =
        WebGestureEvent::NonMomentumPhase;
    webGestureEvent.data.scrollEnd.synthetic = true;
    webGestureEvent.data.scrollEnd.deltaUnits = WebGestureEvent::Page;
    webGestureEvent.uniqueTouchEventId = 12345U;

    PlatformGestureEventBuilder platformGestureBuilder(view, webGestureEvent);
    EXPECT_EQ(PlatformGestureSourceTouchpad, platformGestureBuilder.source());
    EXPECT_EQ(2, platformGestureBuilder.resendingPluginId());
    EXPECT_EQ(0, platformGestureBuilder.position().x());
    EXPECT_EQ(5, platformGestureBuilder.position().y());
    EXPECT_EQ(10, platformGestureBuilder.globalPosition().x());
    EXPECT_EQ(15, platformGestureBuilder.globalPosition().y());
    EXPECT_EQ(ScrollInertialPhaseNonMomentum,
              platformGestureBuilder.inertialPhase());
    EXPECT_TRUE(platformGestureBuilder.synthetic());
    EXPECT_EQ(ScrollGranularity::ScrollByPage,
              platformGestureBuilder.deltaUnits());
    EXPECT_EQ(12345U, platformGestureBuilder.uniqueTouchEventId());
  }
}

}  // namespace blink
