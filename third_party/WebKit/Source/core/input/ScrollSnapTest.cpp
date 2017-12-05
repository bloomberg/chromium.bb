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

class ScrollSnapTest : public SimTest {
 protected:
  void SetUp() override;
  // The following x, y, hint_x, hint_y, delta_x, delta_y are represents
  // the pointer/finger's location on touch screen.
  void ScrollBegin(double x, double y, double hint_x, double hint_y);
  void ScrollUpdate(double x, double y, double delta_x, double delta_y);
  void ScrollEnd(double x, double y);
  void SetInitialScrollOffset(double x, double y);
};

void ScrollSnapTest::SetUp() {
  SimTest::SetUp();
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(400, 400));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <style>
    #scroller {
      width: 140px;
      height: 160px;
      overflow: scroll;
      scroll-snap-type: both mandatory;
      padding: 0px;
    }
    #container {
      margin: 0px;
      padding: 0px;
      width: 500px;
      height: 500px;
    }
    #area {
      position: relative;
      left: 200px;
      top: 200px;
      width: 100px;
      height: 100px;
      scroll-snap-align: start;
    }
    </style>
    <div id='scroller'>
      <div id='container'>
        <div id='area'></div>
      </div>
    </div>
  )HTML");

  Compositor().BeginFrame();
}

void ScrollSnapTest::ScrollBegin(double x,
                                 double y,
                                 double hint_x,
                                 double hint_y) {
  WebGestureEvent event(WebInputEvent::kGestureScrollBegin,
                        WebInputEvent::kNoModifiers,
                        TimeTicks::Now().InSeconds());
  event.x = event.global_x = x;
  event.y = event.global_y = y;
  event.source_device = WebGestureDevice::kWebGestureDeviceTouchscreen;
  event.data.scroll_begin.delta_x_hint = hint_x;
  event.data.scroll_begin.delta_y_hint = hint_y;
  event.data.scroll_begin.pointer_count = 1;
  event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureScrollEvent(event);
}

void ScrollSnapTest::ScrollUpdate(double x,
                                  double y,
                                  double delta_x,
                                  double delta_y) {
  WebGestureEvent event(WebInputEvent::kGestureScrollUpdate,
                        WebInputEvent::kNoModifiers,
                        TimeTicks::Now().InSeconds());
  event.x = event.global_x = x;
  event.y = event.global_y = y;
  event.source_device = WebGestureDevice::kWebGestureDeviceTouchscreen;
  event.data.scroll_update.delta_x = delta_x;
  event.data.scroll_update.delta_y = delta_y;
  event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureScrollEvent(event);
}

void ScrollSnapTest::ScrollEnd(double x, double y) {
  WebGestureEvent event(WebInputEvent::kGestureScrollEnd,
                        WebInputEvent::kNoModifiers,
                        TimeTicks::Now().InSeconds());
  event.x = event.global_x = x;
  event.y = event.global_y = y;
  event.source_device = WebGestureDevice::kWebGestureDeviceTouchscreen;
  GetDocument().GetFrame()->GetEventHandler().HandleGestureScrollEvent(event);
}

void ScrollSnapTest::SetInitialScrollOffset(double x, double y) {
  Element* scroller = GetDocument().getElementById("scroller");
  scroller->setScrollLeft(x);
  scroller->setScrollTop(y);
  ASSERT_EQ(scroller->scrollLeft(), x);
  ASSERT_EQ(scroller->scrollTop(), y);
}

TEST_F(ScrollSnapTest, ScrollSnapOnX) {
  SetInitialScrollOffset(50, 150);
  ScrollBegin(100, 100, -50, 0);
  // The finger moves left.
  ScrollUpdate(100, 100, -50, 0);
  ScrollEnd(50, 100);

  // Wait for animation to finish.
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);

  Element* scroller = GetDocument().getElementById("scroller");
  // Snaps to align the area at start.
  ASSERT_EQ(scroller->scrollLeft(), 200);
  // An x-locked scroll ignores snap points on y.
  ASSERT_EQ(scroller->scrollTop(), 150);
}

TEST_F(ScrollSnapTest, ScrollSnapOnY) {
  SetInitialScrollOffset(150, 50);
  ScrollBegin(100, 100, 0, -50);
  // The finger moves up.
  ScrollUpdate(100, 100, 0, -50);
  ScrollEnd(100, 50);

  // Wait for animation to finish.
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);

  Element* scroller = GetDocument().getElementById("scroller");
  // A y-locked scroll ignores snap points on x.
  ASSERT_EQ(scroller->scrollLeft(), 150);
  // Snaps to align the area at start.
  ASSERT_EQ(scroller->scrollTop(), 200);
}

TEST_F(ScrollSnapTest, ScrollSnapOnBoth) {
  SetInitialScrollOffset(50, 50);
  ScrollBegin(100, 100, 0, -50);
  // The finger moves left.
  ScrollUpdate(100, 100, 0, -50);
  // The finger moves up.
  ScrollUpdate(100, 50, -50, 0);
  ScrollEnd(50, 50);

  // Wait for animation to finish.
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);

  Element* scroller = GetDocument().getElementById("scroller");
  // A scroll gesture that has move in both x and y would snap on both axes.
  ASSERT_EQ(scroller->scrollLeft(), 200);
  ASSERT_EQ(scroller->scrollTop(), 200);
}

}  // namespace blink
