// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/jank_tracker.h"

#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class JankTrackerTest : public RenderingTest {
 protected:
  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
  }
  LocalFrameView& GetFrameView() { return *GetFrame().View(); }
  JankTracker& GetJankTracker() { return GetFrameView().GetJankTracker(); }

  void SimulateInput() {
    GetJankTracker().NotifyInput(WebMouseEvent(
        WebInputEvent::kMouseDown, WebFloatPoint(), WebFloatPoint(),
        WebPointerProperties::Button::kLeft, 0,
        WebInputEvent::Modifiers::kLeftButtonDown, CurrentTimeTicks()));
  }
};

TEST_F(JankTrackerTest, SimpleBlockMovement) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 300px; height: 100px; }
    </style>
    <div id='j'></div>
  )HTML");

  EXPECT_EQ(0.0, GetJankTracker().Score());
  EXPECT_EQ(0.0, GetJankTracker().MaxDistance());

  GetDocument().getElementById("j")->setAttribute(HTMLNames::styleAttr,
                                                  AtomicString("top: 60px"));
  GetFrameView().UpdateAllLifecyclePhases();
  // 300 * (100 + 60) / (default viewport size 800 * 600)
  EXPECT_FLOAT_EQ(0.1, GetJankTracker().Score());
  EXPECT_FLOAT_EQ(60.0, GetJankTracker().MaxDistance());
}

TEST_F(JankTrackerTest, GranularitySnapping) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 304px; height: 104px; }
    </style>
    <div id='j'></div>
  )HTML");
  GetDocument().getElementById("j")->setAttribute(HTMLNames::styleAttr,
                                                  AtomicString("top: 58px"));
  GetFrameView().UpdateAllLifecyclePhases();
  // Rect locations and sizes should snap to multiples of 600 / 60 = 10.
  EXPECT_FLOAT_EQ(0.1, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, Transform) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #c { transform: translateX(-300px) translateY(-40px); }
      #j { position: relative; width: 600px; height: 140px; }
    </style>
    <div id='c'>
      <div id='j'></div>
    </div>
  )HTML");

  GetDocument().getElementById("j")->setAttribute(HTMLNames::styleAttr,
                                                  AtomicString("top: 60px"));
  GetFrameView().UpdateAllLifecyclePhases();
  // (600 - 300) * (140 - 40 + 60) / (default viewport size 800 * 600)
  EXPECT_FLOAT_EQ(0.1, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, RtlDistance) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 100px; height: 100px; direction: rtl; }
    </style>
    <div id='j'></div>
  )HTML");
  GetDocument().getElementById("j")->setAttribute(
      HTMLNames::styleAttr, AtomicString("width: 70px; left: 10px"));
  GetFrameView().UpdateAllLifecyclePhases();
  EXPECT_FLOAT_EQ(20.0, GetJankTracker().MaxDistance());
}

TEST_F(JankTrackerTest, SmallMovementIgnored) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 300px; height: 100px; }
    </style>
    <div id='j'></div>
  )HTML");
  GetDocument().getElementById("j")->setAttribute(HTMLNames::styleAttr,
                                                  AtomicString("top: 2px"));
  GetFrameView().UpdateAllLifecyclePhases();
  EXPECT_EQ(0.0, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, SmallMovementIgnoredWithZoom) {
  GetDocument().GetFrame()->SetPageZoomFactor(2);
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 300px; height: 100px; }
    </style>
    <div id='j'></div>
  )HTML");
  GetDocument().getElementById("j")->setAttribute(HTMLNames::styleAttr,
                                                  AtomicString("top: 2px"));
  GetFrameView().UpdateAllLifecyclePhases();
  EXPECT_EQ(0.0, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, IgnoreAfterInput) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #j { position: relative; width: 300px; height: 100px; }
    </style>
    <div id='j'></div>
  )HTML");
  GetDocument().getElementById("j")->setAttribute(HTMLNames::styleAttr,
                                                  AtomicString("top: 60px"));
  SimulateInput();
  GetFrameView().UpdateAllLifecyclePhases();
  EXPECT_EQ(0.0, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, CompositedElementMovement) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #jank {
      position: relative;
      width: 500px;
      height: 200px;
      background: yellow;
    }
    #container {
      position: absolute;
      width: 400px;
      height: 400px;
      left: 400px;
      top: 100px;
      background: #ccc;
    }
    .tr { will-change: transform; }
    </style>
    <div id='container' class='tr'>
      <div id='space'></div>
      <div id='jank' class='tr'></div>
    </div>
  )HTML");

  GetDocument().getElementById("space")->setAttribute(
      HTMLNames::styleAttr, AtomicString("height: 100px"));
  GetFrameView().UpdateAllLifecyclePhases();

  // #jank is 400x200 after viewport intersection with correct application of
  // composited #container offset, and 100px lower after janking, so jank score
  // is (400 * 300) / (viewport size 800 * 600)
  EXPECT_FLOAT_EQ(0.25, GetJankTracker().Score());
}

TEST_F(JankTrackerTest, CompositedJankBeforeFirstPaint) {
  // Tests that we don't crash if a new layer janks during a second compositing
  // update before prepaint sets up property tree state.  See crbug.com/881735
  // (which invokes UpdateLifecycleToCompositingCleanPlusScrolling through
  // accessibilityController.accessibleElementById).

  SetBodyInnerHTML(R"HTML(
    <style>
      .hide { display: none; }
      .tr { will-change: transform; }
      body { margin: 0; }
      div { height: 100px; }
    </style>
    <div id="container">
      <div id="A">A</div>
      <div id="B" class="tr hide">B</div>
    </div>
  )HTML");

  GetDocument().getElementById("B")->setAttribute(HTMLNames::classAttr,
                                                  AtomicString("tr"));
  GetFrameView().UpdateLifecycleToCompositingCleanPlusScrolling();
  GetDocument().getElementById("A")->setAttribute(HTMLNames::classAttr,
                                                  AtomicString("hide"));
  GetFrameView().UpdateAllLifecyclePhases();
}

}  // namespace blink
