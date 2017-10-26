// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/input/EventHandler.h"
#include "core/input/ScrollManager.h"
#include "core/style/ComputedStyle.h"
#include "core/testing/sim/SimCompositor.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"

namespace blink {

class OverscrollBehaviorTest : public SimTest {
 protected:
  void SetUp() override;

  void SetInnerOverscrollBehavior(EOverscrollBehavior, EOverscrollBehavior);
  void Scroll(double x, double y);

 private:
  WebGestureEvent ScrollBegin(double hint_x, double hint_y);
  WebGestureEvent ScrollUpdate(double delta_x, double delta_y);
  WebGestureEvent ScrollEnd();
};

void OverscrollBehaviorTest::SetUp() {
  SimTest::SetUp();
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='outer' style='height: 300px; width: 300px; overflow: "
      "scroll;'>"
      "  <div id='inner' style='height: 500px; width: 500px; overflow: "
      "scroll;'>"
      "    <div id='content' style='height: 700px; width: 700px;'>"
      "</div></div></div>");

  Compositor().BeginFrame();

  Element* outer = GetDocument().getElementById("outer");
  Element* inner = GetDocument().getElementById("inner");

  // Scrolls the outer element to its bottom-right extent, and makes sure the
  // inner element is at its top-left extent. So that if the scroll is up and
  // left, the inner element doesn't scroll, and we are able to check if the
  // scroll is propagated to the outer element.
  outer->setScrollLeft(200);
  outer->setScrollTop(200);
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
  ASSERT_EQ(inner->scrollLeft(), 0);
  ASSERT_EQ(inner->scrollTop(), 0);
}

void OverscrollBehaviorTest::SetInnerOverscrollBehavior(EOverscrollBehavior x,
                                                        EOverscrollBehavior y) {
  Element* inner = GetDocument().getElementById("inner");
  inner->MutableComputedStyle()->SetOverscrollBehaviorX(x);
  inner->MutableComputedStyle()->SetOverscrollBehaviorY(y);
}

void OverscrollBehaviorTest::Scroll(double x, double y) {
  GetDocument().GetFrame()->GetEventHandler().HandleGestureScrollEvent(
      ScrollBegin(x, y));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureScrollEvent(
      ScrollUpdate(x, y));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureScrollEvent(
      ScrollEnd());
}

WebGestureEvent OverscrollBehaviorTest::ScrollBegin(double hint_x,
                                                    double hint_y) {
  WebGestureEvent event(WebInputEvent::kGestureScrollBegin,
                        WebInputEvent::kNoModifiers,
                        TimeTicks::Now().InSeconds());
  event.x = event.global_x = 20;
  event.y = event.global_y = 20;
  event.source_device = WebGestureDevice::kWebGestureDeviceTouchscreen;
  event.data.scroll_begin.delta_x_hint = -hint_x;
  event.data.scroll_begin.delta_y_hint = -hint_y;
  event.data.scroll_begin.pointer_count = 1;
  event.SetFrameScale(1);
  return event;
}

WebGestureEvent OverscrollBehaviorTest::ScrollUpdate(double delta_x,
                                                     double delta_y) {
  WebGestureEvent event(WebInputEvent::kGestureScrollUpdate,
                        WebInputEvent::kNoModifiers,
                        TimeTicks::Now().InSeconds());
  event.x = event.global_x = 20;
  event.y = event.global_y = 20;
  event.source_device = WebGestureDevice::kWebGestureDeviceTouchscreen;
  event.data.scroll_update.delta_x = -delta_x;
  event.data.scroll_update.delta_y = -delta_y;
  event.SetFrameScale(1);
  return event;
}

WebGestureEvent OverscrollBehaviorTest::ScrollEnd() {
  WebGestureEvent event(WebInputEvent::kGestureScrollEnd,
                        WebInputEvent::kNoModifiers,
                        TimeTicks::Now().InSeconds());
  event.x = event.global_x = 20;
  event.y = event.global_y = 20;
  event.source_device = WebGestureDevice::kWebGestureDeviceTouchscreen;
  return event;
}

TEST_F(OverscrollBehaviorTest, AutoAllowsPropagation) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kAuto,
                             EOverscrollBehavior::kAuto);
  Scroll(-100.0, -100.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 100);
  ASSERT_EQ(outer->scrollTop(), 100);
}

TEST_F(OverscrollBehaviorTest, ContainOnXPreventsPropagationsOnX) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kContain,
                             EOverscrollBehavior::kAuto);
  Scroll(-100, 0.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, ContainOnXAllowsPropagationsOnY) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kContain,
                             EOverscrollBehavior::kAuto);
  Scroll(0.0, -100.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 100);
}

TEST_F(OverscrollBehaviorTest, ContainOnXPreventsDiagonalPropagations) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kContain,
                             EOverscrollBehavior::kAuto);
  Scroll(-100.0, -100.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, ContainOnYPreventsPropagationsOnY) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kAuto,
                             EOverscrollBehavior::kContain);
  Scroll(0.0, -100.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, ContainOnYAllowsPropagationsOnX) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kAuto,
                             EOverscrollBehavior::kContain);
  Scroll(-100.0, 0.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 100);
  ASSERT_EQ(outer->scrollTop(), 200);
}

TEST_F(OverscrollBehaviorTest, ContainOnYAPreventsDiagonalPropagations) {
  SetInnerOverscrollBehavior(EOverscrollBehavior::kAuto,
                             EOverscrollBehavior::kContain);
  Scroll(-100.0, -100.0);
  Element* outer = GetDocument().getElementById("outer");
  ASSERT_EQ(outer->scrollLeft(), 200);
  ASSERT_EQ(outer->scrollTop(), 200);
}

}  // namespace blink
