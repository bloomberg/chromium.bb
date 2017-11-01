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

  WebTouchEvent event;
  event.SetFrameScale(1);
  WebTouchPoint point(
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                           WebPointerProperties::Button::kLeft,
                           WebFloatPoint(100, 100), WebFloatPoint(100, 100)));
  point.state = WebTouchPoint::State::kStatePressed;
  event.touches[event.touches_length++] = point;
  event.SetType(WebInputEvent::kTouchStart);
  EventHandler().HandleTouchEvent(event, Vector<WebTouchEvent>());

  point.pointer_type = WebPointerProperties::PointerType::kPen;
  event.touches[0] = point;
  event.SetType(WebInputEvent::kTouchStart);
  EventHandler().HandleTouchEvent(event, Vector<WebTouchEvent>());

  WebMouseEvent mouse_down_event(
      WebInputEvent::kMouseDown, WebFloatPoint(100, 100),
      WebFloatPoint(100, 100), WebPointerProperties::Button::kLeft, 0, 0, 0);
  mouse_down_event.SetFrameScale(1);
  EventHandler().HandleMousePressEvent(mouse_down_event);

  ASSERT_EQ(callback->mouseEventCount(), 0);
  ASSERT_EQ(callback->touchEventCount(), 0);
  ASSERT_EQ(callback->penEventCount(), 0);

  point.pointer_type = WebPointerProperties::PointerType::kPen;
  event.touches[0] = point;
  event.SetType(WebInputEvent::kTouchScrollStarted);
  EventHandler().HandleTouchEvent(event, Vector<WebTouchEvent>());
  ASSERT_EQ(callback->mouseEventCount(), 0);
  ASSERT_EQ(callback->touchEventCount(), 1);
  ASSERT_EQ(callback->penEventCount(), 1);

  point.pointer_type = WebPointerProperties::PointerType::kTouch;
  event.touches[0] = point;
  event.SetType(WebInputEvent::kTouchScrollStarted);
  EventHandler().HandleTouchEvent(event, Vector<WebTouchEvent>());
  ASSERT_EQ(callback->mouseEventCount(), 0);
  ASSERT_EQ(callback->touchEventCount(), 1);
  ASSERT_EQ(callback->penEventCount(), 1);

  WebMouseEvent mouse_move_event(
      WebInputEvent::kMouseMove, WebFloatPoint(200, 200),
      WebFloatPoint(200, 200), WebPointerProperties::Button::kLeft, 0, 0, 0);
  mouse_move_event.SetFrameScale(1);
  EventHandler().HandleMouseMoveEvent(mouse_move_event,
                                      Vector<WebMouseEvent>());

  ASSERT_EQ(callback->mouseEventCount(), 1);
  ASSERT_EQ(callback->touchEventCount(), 1);
  ASSERT_EQ(callback->penEventCount(), 1);
}

}  // namespace blink
