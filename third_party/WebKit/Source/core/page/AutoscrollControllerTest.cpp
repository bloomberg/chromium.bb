// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Element.h"
#include "core/input/EventHandler.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Page.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class AutoscrollControllerTest : public SimTest {
 public:
  AutoscrollController& GetAutoscrollController() {
    return WebView().GetPage()->GetAutoscrollController();
  }
};

// Ensure Autoscroll not crash by layout called in UpdateSelectionForMouseDrag.
TEST_F(AutoscrollControllerTest,
       CrashWhenLayoutStopAnimationBeforeScheduleAnimation) {
  WebView().Resize(WebSize(800, 600));
  WebView().SetBaseBackgroundColorOverride(SK_ColorTRANSPARENT);
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #scrollable {
        overflow: auto;
        width: 10px;
        height: 10px;
      }
    </style>
    <div id='scrollable'>
      <p id='p'>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
      <p>Some text here for selection autoscroll.</p>
    </div>
  )HTML");

  Compositor().BeginFrame();

  AutoscrollController& controller = GetAutoscrollController();
  Document& document = GetDocument();

  Element* scrollable = document.getElementById("scrollable");
  DCHECK(scrollable);
  DCHECK(scrollable->GetLayoutObject());

  WebMouseEvent event(WebInputEvent::kMouseDown, WebFloatPoint(5, 5),
                      WebFloatPoint(5, 5), WebPointerProperties::Button::kLeft,
                      0, WebInputEvent::Modifiers::kLeftButtonDown,
                      CurrentTimeTicks().InSeconds());
  event.SetFrameScale(1);

  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);

  controller.StartAutoscrollForSelection(scrollable->GetLayoutObject());

  DCHECK(controller.IsAutoscrolling());

  // Hide scrollable here will cause UpdateSelectionForMouseDrag stop animation.
  scrollable->SetInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);

  // BeginFrame will call AutoscrollController::Animate.
  Compositor().BeginFrame();

  EXPECT_FALSE(controller.IsAutoscrolling());
}

}  // namespace blink
