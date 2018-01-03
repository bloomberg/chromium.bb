// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/dom/events/EventListener.h"
#include "core/html/HTMLElement.h"
#include "core/input/EventHandler.h"
#include "core/input/PointerEventManager.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"

namespace blink {

namespace {
class CheckPointerEventListenerCallback final : public EventListener {
 public:
  static CheckPointerEventListenerCallback* Create() {
    return new CheckPointerEventListenerCallback();
  }

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event* event) override {
    const String pointer_type = ((PointerEvent*)event)->pointerType();
    if (pointer_type == "mouse")
      mouse_event_received_count_++;
    else if (pointer_type == "touch")
      touch_event_received_count_++;
    else if (pointer_type == "pen")
      pen_event_received_count_++;
  }

  int mouseEventCount() const { return mouse_event_received_count_; }
  int touchEventCount() const { return touch_event_received_count_; }
  int penEventCount() const { return pen_event_received_count_; }

 private:
  CheckPointerEventListenerCallback()
      : EventListener(EventListener::kCPPEventListenerType) {}
  int mouse_event_received_count_ = 0;
  int touch_event_received_count_ = 0;
  int pen_event_received_count_ = 0;
};

}  // namespace

class PointerEventManagerTest : public SimTest {
 protected:
  EventHandler& EventHandler() {
    return GetDocument().GetFrame()->GetEventHandler();
  }
  WebPointerEvent CreateTestPointerEvent(
      WebInputEvent::Type type,
      WebPointerProperties::PointerType pointer_type) {
    WebPointerEvent event(
        type,
        WebPointerProperties(1, pointer_type,
                             WebPointerProperties::Button::kLeft,
                             WebFloatPoint(100, 100), WebFloatPoint(100, 100)),
        1, 1);
    event.SetFrameScale(1);
    return event;
  }
  WebMouseEvent CreateTestMouseEvent(WebInputEvent::Type type,
                                     const WebFloatPoint& coordinates) {
    WebMouseEvent event(type, coordinates, coordinates,
                        WebPointerProperties::Button::kLeft, 0, 0, 0);
    event.SetFrameScale(1);
    return event;
  }
};

TEST_F(PointerEventManagerTest, PointerCancelsOfAllTypes) {
  WebView().Resize(WebSize(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<body style='padding: 0px; width: 400px; height: 400px;'>"
      "<div draggable='true' style='width: 150px; height: 150px;'></div>"
      "</body>");
  CheckPointerEventListenerCallback* callback =
      CheckPointerEventListenerCallback::Create();
  GetDocument().body()->addEventListener(EventTypeNames::pointercancel,
                                         callback);

  EventHandler().HandlePointerEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerDown,
                             WebPointerProperties::PointerType::kTouch),
      Vector<WebPointerEvent>());

  EventHandler().HandlePointerEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerDown,
                             WebPointerProperties::PointerType::kPen),
      Vector<WebPointerEvent>());

  EventHandler().HandleMousePressEvent(
      CreateTestMouseEvent(WebInputEvent::kMouseDown, WebFloatPoint(100, 100)));

  ASSERT_EQ(callback->mouseEventCount(), 0);
  ASSERT_EQ(callback->touchEventCount(), 0);
  ASSERT_EQ(callback->penEventCount(), 0);

  EventHandler().HandlePointerEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerCausedUaAction,
                             WebPointerProperties::PointerType::kPen),
      Vector<WebPointerEvent>());
  ASSERT_EQ(callback->mouseEventCount(), 0);
  ASSERT_EQ(callback->touchEventCount(), 1);
  ASSERT_EQ(callback->penEventCount(), 1);

  EventHandler().HandlePointerEvent(
      CreateTestPointerEvent(WebInputEvent::kPointerCausedUaAction,
                             WebPointerProperties::PointerType::kTouch),
      Vector<WebPointerEvent>());
  ASSERT_EQ(callback->mouseEventCount(), 0);
  ASSERT_EQ(callback->touchEventCount(), 1);
  ASSERT_EQ(callback->penEventCount(), 1);

  EventHandler().HandleMouseMoveEvent(
      CreateTestMouseEvent(WebInputEvent::kMouseMove, WebFloatPoint(200, 200)),
      Vector<WebMouseEvent>());

  ASSERT_EQ(callback->mouseEventCount(), 1);
  ASSERT_EQ(callback->touchEventCount(), 1);
  ASSERT_EQ(callback->penEventCount(), 1);
}

}  // namespace blink
