// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/EventHandler.h"

#include <memory>
#include "core/dom/ClientRect.h"
#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutBox.h"
#include "core/loader/EmptyClients.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "platform/testing/HistogramTester.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_WHEEL_BUCKET(reason, count)     \
  histogram_tester.ExpectBucketCount(          \
      "Renderer4.MainThreadWheelScrollReason", \
      GetBucketIndex(MainThreadScrollingReason::reason), count);

#define EXPECT_TOUCH_BUCKET(reason, count)       \
  histogram_tester.ExpectBucketCount(            \
      "Renderer4.MainThreadGestureScrollReason", \
      GetBucketIndex(MainThreadScrollingReason::reason), count);

#define EXPECT_WHEEL_TOTAL(count)                                            \
  histogram_tester.ExpectTotalCount("Renderer4.MainThreadWheelScrollReason", \
                                    count);

#define EXPECT_TOUCH_TOTAL(count)                                              \
  histogram_tester.ExpectTotalCount("Renderer4.MainThreadGestureScrollReason", \
                                    count);

namespace blink {

class EventHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override;

  Page& GetPage() const { return dummy_page_holder_->GetPage(); }
  Document& GetDocument() const { return dummy_page_holder_->GetDocument(); }
  FrameSelection& Selection() const {
    return GetDocument().GetFrame()->Selection();
  }

  void SetHtmlInnerHTML(const char* html_content);

 protected:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

class NonCompositedMainThreadScrollingReasonRecordTest
    : public EventHandlerTest {
 protected:
  class ScrollBeginEventBuilder : public WebGestureEvent {
   public:
    ScrollBeginEventBuilder(IntPoint position,
                            FloatPoint delta,
                            WebGestureDevice device)
        : WebGestureEvent() {
      type_ = WebInputEvent::kGestureScrollBegin;
      x = global_x = position.X();
      y = global_y = position.Y();
      data.scroll_begin.delta_y_hint = delta.Y();
      source_device = device;
      frame_scale_ = 1;
    }
  };

  class ScrollUpdateEventBuilder : public WebGestureEvent {
   public:
    ScrollUpdateEventBuilder() : WebGestureEvent() {
      type_ = WebInputEvent::kGestureScrollUpdate;
      data.scroll_update.delta_x = 0.0f;
      data.scroll_update.delta_y = 1.0f;
      data.scroll_update.velocity_x = 0;
      data.scroll_update.velocity_y = 1;
      frame_scale_ = 1;
    }
  };

  class ScrollEndEventBuilder : public WebGestureEvent {
   public:
    ScrollEndEventBuilder() : WebGestureEvent() {
      type_ = WebInputEvent::kGestureScrollEnd;
      frame_scale_ = 1;
    }
  };

  int GetBucketIndex(uint32_t reason);
  void Scroll(Element*, const WebGestureDevice);
};

class TapEventBuilder : public WebGestureEvent {
 public:
  TapEventBuilder(IntPoint position, int tap_count)
      : WebGestureEvent(WebInputEvent::kGestureTap,
                        WebInputEvent::kNoModifiers,
                        TimeTicks::Now().InSeconds()) {
    x = global_x = position.X();
    y = global_y = position.Y();
    source_device = kWebGestureDeviceTouchscreen;
    data.tap.tap_count = tap_count;
    data.tap.width = 5;
    data.tap.height = 5;
    frame_scale_ = 1;
  }
};

class LongPressEventBuilder : public WebGestureEvent {
 public:
  LongPressEventBuilder(IntPoint position) : WebGestureEvent() {
    type_ = WebInputEvent::kGestureLongPress;
    x = global_x = position.X();
    y = global_y = position.Y();
    source_device = kWebGestureDeviceTouchscreen;
    data.long_press.width = 5;
    data.long_press.height = 5;
    frame_scale_ = 1;
  }
};

class MousePressEventBuilder : public WebMouseEvent {
 public:
  MousePressEventBuilder(IntPoint position_param,
                         int click_count_param,
                         WebMouseEvent::Button button_param)
      : WebMouseEvent(WebInputEvent::kMouseDown,
                      WebInputEvent::kNoModifiers,
                      TimeTicks::Now().InSeconds()) {
    click_count = click_count_param;
    button = button_param;
    SetPositionInWidget(position_param.X(), position_param.Y());
    SetPositionInScreen(position_param.X(), position_param.Y());
    frame_scale_ = 1;
  }
};

void EventHandlerTest::SetUp() {
  dummy_page_holder_ = DummyPageHolder::Create(IntSize(300, 400));
}

void EventHandlerTest::SetHtmlInnerHTML(const char* html_content) {
  GetDocument().documentElement()->setInnerHTML(String::FromUTF8(html_content));
  GetDocument().View()->UpdateAllLifecyclePhases();
}

int NonCompositedMainThreadScrollingReasonRecordTest::GetBucketIndex(
    uint32_t reason) {
  int index = 1;
  while (!(reason & 1)) {
    reason >>= 1;
    ++index;
  }
  DCHECK_EQ(reason, 1u);
  return index;
}

void NonCompositedMainThreadScrollingReasonRecordTest::Scroll(
    Element* element,
    const WebGestureDevice device) {
  DCHECK(element);
  DCHECK(element->getBoundingClientRect());
  ClientRect* rect = element->getBoundingClientRect();
  ScrollBeginEventBuilder scroll_begin(
      IntPoint(rect->left() + rect->width() / 2,
               rect->top() + rect->height() / 2),
      FloatPoint(0.f, 1.f), device);
  ScrollUpdateEventBuilder scroll_update;
  ScrollEndEventBuilder scroll_end;
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(scroll_begin);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(scroll_update);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(scroll_end);
}

TEST_F(EventHandlerTest, dragSelectionAfterScroll) {
  SetHtmlInnerHTML(
      "<style> body { margin: 0px; } .upper { width: 300px; height: 400px; }"
      ".lower { margin: 0px; width: 300px; height: 400px; } .line { display: "
      "block; width: 300px; height: 30px; } </style>"
      "<div class='upper'></div>"
      "<div class='lower'>"
      "<span class='line'>Line 1</span><span class='line'>Line 2</span><span "
      "class='line'>Line 3</span><span class='line'>Line 4</span><span "
      "class='line'>Line 5</span>"
      "<span class='line'>Line 6</span><span class='line'>Line 7</span><span "
      "class='line'>Line 8</span><span class='line'>Line 9</span><span "
      "class='line'>Line 10</span>"
      "</div>");

  FrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 400), kProgrammaticScroll);

  WebMouseEvent mouse_down_event(WebInputEvent::kMouseDown, WebFloatPoint(0, 0),
                                 WebFloatPoint(100, 200),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::kTimeStampForTesting);
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_move_event(
      WebInputEvent::kMouseMove, WebFloatPoint(100, 50),
      WebFloatPoint(200, 250), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown,
      WebInputEvent::kTimeStampForTesting);
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>());

  GetPage().GetAutoscrollController().Animate(
      WTF::MonotonicallyIncreasingTime());
  GetPage().Animator().ServiceScriptedAnimations(
      WTF::MonotonicallyIncreasingTime());

  WebMouseEvent mouse_up_event(
      WebMouseEvent::kMouseUp, WebFloatPoint(100, 50), WebFloatPoint(200, 250),
      WebPointerProperties::Button::kLeft, 1, WebInputEvent::kNoModifiers,
      WebInputEvent::kTimeStampForTesting);
  mouse_up_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_up_event);

  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsRange());
  Range* range = CreateRange(Selection()
                                 .ComputeVisibleSelectionInDOMTreeDeprecated()
                                 .ToNormalizedEphemeralRange());
  ASSERT_TRUE(range);
  EXPECT_EQ("Line 1\nLine 2", range->GetText());
}

TEST_F(EventHandlerTest, multiClickSelectionFromTap) {
  SetHtmlInnerHTML(
      "<style> body { margin: 0px; } .line { display: block; width: 300px; "
      "height: 30px; } </style>"
      "<body contenteditable='true'><span class='line' id='line'>One Two "
      "Three</span></body>");

  Node* line = GetDocument().GetElementById("line")->FirstChild();

  TapEventBuilder single_tap_event(IntPoint(0, 0), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);
  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret());
  EXPECT_EQ(Position(line, 0),
            Selection().ComputeVisibleSelectionInDOMTreeDeprecated().Start());

  // Multi-tap events on editable elements should trigger selection, just
  // like multi-click events.
  TapEventBuilder double_tap_event(IntPoint(0, 0), 2);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      double_tap_event);
  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsRange());
  EXPECT_EQ(Position(line, 0),
            Selection().ComputeVisibleSelectionInDOMTreeDeprecated().Start());
  if (GetDocument()
          .GetFrame()
          ->GetEditor()
          .IsSelectTrailingWhitespaceEnabled()) {
    EXPECT_EQ(Position(line, 4),
              Selection().ComputeVisibleSelectionInDOMTreeDeprecated().end());
    EXPECT_EQ("One ", WebString(Selection().SelectedText()).Utf8());
  } else {
    EXPECT_EQ(Position(line, 3),
              Selection().ComputeVisibleSelectionInDOMTreeDeprecated().end());
    EXPECT_EQ("One", WebString(Selection().SelectedText()).Utf8());
  }

  TapEventBuilder triple_tap_event(IntPoint(0, 0), 3);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      triple_tap_event);
  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsRange());
  EXPECT_EQ(Position(line, 0),
            Selection().ComputeVisibleSelectionInDOMTreeDeprecated().Start());
  EXPECT_EQ(Position(line, 13),
            Selection().ComputeVisibleSelectionInDOMTreeDeprecated().end());
  EXPECT_EQ("One Two Three", WebString(Selection().SelectedText()).Utf8());
}

TEST_F(EventHandlerTest, multiClickSelectionFromTapDisabledIfNotEditable) {
  SetHtmlInnerHTML(
      "<style> body { margin: 0px; } .line { display: block; width: 300px; "
      "height: 30px; } </style>"
      "<span class='line' id='line'>One Two Three</span>");

  Node* line = GetDocument().GetElementById("line")->FirstChild();

  TapEventBuilder single_tap_event(IntPoint(0, 0), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);
  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret());
  EXPECT_EQ(Position(line, 0),
            Selection().ComputeVisibleSelectionInDOMTreeDeprecated().Start());

  // As the text is readonly, multi-tap events should not trigger selection.
  TapEventBuilder double_tap_event(IntPoint(0, 0), 2);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      double_tap_event);
  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret());
  EXPECT_EQ(Position(line, 0),
            Selection().ComputeVisibleSelectionInDOMTreeDeprecated().Start());

  TapEventBuilder triple_tap_event(IntPoint(0, 0), 3);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      triple_tap_event);
  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret());
  EXPECT_EQ(Position(line, 0),
            Selection().ComputeVisibleSelectionInDOMTreeDeprecated().Start());
}

TEST_F(EventHandlerTest, draggedInlinePositionTest) {
  SetHtmlInnerHTML(
      "<style>"
      "body { margin: 0px; }"
      ".line { font-family: sans-serif; background: blue; width: 300px; "
      "height: 30px; font-size: 40px; margin-left: 250px; }"
      "</style>"
      "<div style='width: 300px; height: 100px;'>"
      "<span class='line' draggable='true'>abcd</span>"
      "</div>");
  WebMouseEvent mouse_down_event(WebMouseEvent::kMouseDown,
                                 WebFloatPoint(262, 29), WebFloatPoint(329, 67),
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::kTimeStampForTesting);
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_move_event(
      WebMouseEvent::kMouseMove, WebFloatPoint(618, 298),
      WebFloatPoint(685, 436), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown,
      WebInputEvent::kTimeStampForTesting);
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>());

  EXPECT_EQ(IntPoint(12, 29), GetDocument()
                                  .GetFrame()
                                  ->GetEventHandler()
                                  .DragDataTransferLocationForTesting());
}

TEST_F(EventHandlerTest, draggedSVGImagePositionTest) {
  SetHtmlInnerHTML(
      "<style>"
      "body { margin: 0px; }"
      "[draggable] {"
      "-webkit-user-select: none; user-select: none; -webkit-user-drag: "
      "element; }"
      "</style>"
      "<div style='width: 300px; height: 100px;'>"
      "<svg width='500' height='500'>"
      "<rect x='100' y='100' width='100px' height='100px' fill='blue' "
      "draggable='true'/>"
      "</svg>"
      "</div>");
  WebMouseEvent mouse_down_event(
      WebMouseEvent::kMouseDown, WebFloatPoint(145, 144),
      WebFloatPoint(212, 282), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown,
      WebInputEvent::kTimeStampForTesting);
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_move_event(
      WebMouseEvent::kMouseMove, WebFloatPoint(618, 298),
      WebFloatPoint(685, 436), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown,
      WebInputEvent::kTimeStampForTesting);
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>());

  EXPECT_EQ(IntPoint(45, 44), GetDocument()
                                  .GetFrame()
                                  ->GetEventHandler()
                                  .DragDataTransferLocationForTesting());
}

// Regression test for http://crbug.com/641403 to verify we use up-to-date
// layout tree for dispatching "contextmenu" event.
TEST_F(EventHandlerTest, sendContextMenuEventWithHover) {
  SetHtmlInnerHTML(
      "<style>*:hover { color: red; }</style>"
      "<div>foo</div>");
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().createElement("script");
  script->setInnerHTML(
      "document.addEventListener('contextmenu', event => "
      "event.preventDefault());");
  GetDocument().body()->AppendChild(script);
  GetDocument().UpdateStyleAndLayout();
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(GetDocument().body(), 0))
          .Build());
  WebMouseEvent mouse_down_event(
      WebMouseEvent::kMouseDown, WebFloatPoint(0, 0), WebFloatPoint(100, 200),
      WebPointerProperties::Button::kRight, 1,
      WebInputEvent::Modifiers::kRightButtonDown, TimeTicks::Now().InSeconds());
  mouse_down_event.SetFrameScale(1);
  EXPECT_EQ(WebInputEventResult::kHandledApplication,
            GetDocument().GetFrame()->GetEventHandler().SendContextMenuEvent(
                mouse_down_event));
}

TEST_F(EventHandlerTest, EmptyTextfieldInsertionOnTap) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50></textarea>");

  TapEventBuilder single_tap_event(IntPoint(200, 200), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, NonEmptyTextfieldInsertionOnTap) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  TapEventBuilder single_tap_event(IntPoint(200, 200), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, EmptyTextfieldInsertionOnLongPress) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50></textarea>");

  LongPressEventBuilder long_press_event(IntPoint(200, 200));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      long_press_event);

  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());

  // Single Tap on an empty edit field should clear insertion handle
  TapEventBuilder single_tap_event(IntPoint(200, 200), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, NonEmptyTextfieldInsertionOnLongPress) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  LongPressEventBuilder long_press_event(IntPoint(200, 200));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      long_press_event);

  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, ClearHandleAfterTap) {
  SetHtmlInnerHTML("<textarea cols=50  rows=10>Enter text</textarea>");

  // Show handle
  LongPressEventBuilder long_press_event(IntPoint(200, 10));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      long_press_event);

  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());

  // Tap away from text area should clear handle
  TapEventBuilder single_tap_event(IntPoint(200, 350), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, HandleNotShownOnMouseEvents) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  MousePressEventBuilder left_mouse_press_event(
      IntPoint(200, 200), 1, WebPointerProperties::Button::kLeft);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      left_mouse_press_event);

  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());

  MousePressEventBuilder right_mouse_press_event(
      IntPoint(200, 200), 1, WebPointerProperties::Button::kRight);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      right_mouse_press_event);

  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());

  MousePressEventBuilder double_click_mouse_press_event(
      IntPoint(200, 200), 2, WebPointerProperties::Button::kLeft);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      double_click_mouse_press_event);

  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsRange());
  ASSERT_FALSE(Selection().IsHandleVisible());

  MousePressEventBuilder triple_click_mouse_press_event(
      IntPoint(200, 200), 3, WebPointerProperties::Button::kLeft);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      triple_click_mouse_press_event);

  ASSERT_TRUE(
      Selection().ComputeVisibleSelectionInDOMTreeDeprecated().IsRange());
  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, dragEndInNewDrag) {
  SetHtmlInnerHTML(
      "<style>.box { width: 100px; height: 100px; display: block; }</style>"
      "<a class='box' href=''>Drag me</a>");

  WebMouseEvent mouse_down_event(
      WebInputEvent::kMouseDown, WebFloatPoint(50, 50), WebFloatPoint(50, 50),
      WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown, TimeTicks::Now().InSeconds());
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);

  WebMouseEvent mouse_move_event(
      WebInputEvent::kMouseMove, WebFloatPoint(51, 50), WebFloatPoint(51, 50),
      WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown, TimeTicks::Now().InSeconds());
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>());

  // This reproduces what might be the conditions of http://crbug.com/677916
  //
  // TODO(crbug.com/682047): The call sequence below should not occur outside
  // this contrived test. Given the current code, it is unclear how the
  // dragSourceEndedAt() call could occur before a drag operation is started.

  WebMouseEvent mouse_up_event(
      WebInputEvent::kMouseUp, WebFloatPoint(100, 50), WebFloatPoint(200, 250),
      WebPointerProperties::Button::kLeft, 1, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_up_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().DragSourceEndedAt(
      mouse_up_event, kDragOperationNone);

  // This test passes if it doesn't crash.
}

class TooltipCapturingChromeClient : public EmptyChromeClient {
 public:
  TooltipCapturingChromeClient() {}

  void SetToolTip(LocalFrame&, const String& str, TextDirection) override {
    last_tool_tip_ = str;
  }

  String& LastToolTip() { return last_tool_tip_; }

 private:
  String last_tool_tip_;
};

class EventHandlerTooltipTest : public EventHandlerTest {
 public:
  EventHandlerTooltipTest() {}

  void SetUp() override {
    chrome_client_ = new TooltipCapturingChromeClient();
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600), &clients);
  }

  String& LastToolTip() { return chrome_client_->LastToolTip(); }

 private:
  Persistent<TooltipCapturingChromeClient> chrome_client_;
};

TEST_F(EventHandlerTooltipTest, mouseLeaveClearsTooltip) {
  SetHtmlInnerHTML(
      "<style>.box { width: 100%; height: 100%; }</style>"
      "<img src='image.png' class='box' title='tooltip'>link</img>");

  EXPECT_EQ(WTF::String(), LastToolTip());

  WebMouseEvent mouse_move_event(
      WebInputEvent::kMouseMove, WebFloatPoint(51, 50), WebFloatPoint(51, 50),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, Vector<WebMouseEvent>());

  EXPECT_EQ("tooltip", LastToolTip());

  WebMouseEvent mouse_leave_event(
      WebInputEvent::kMouseLeave, WebFloatPoint(0, 0), WebFloatPoint(0, 0),
      WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
      TimeTicks::Now().InSeconds());
  mouse_leave_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseLeaveEvent(
      mouse_leave_event);

  EXPECT_EQ(WTF::String(), LastToolTip());
}

TEST_F(NonCompositedMainThreadScrollingReasonRecordTest,
       TouchAndWheelGeneralTest) {
  SetHtmlInnerHTML(
      "<style>"
      " .box { overflow:scroll; width: 100px; height: 100px; }"
      " .translucent { opacity: 0.5; }"
      " .spacer { height: 1000px; }"
      "</style>"
      "<div id='box' class='translucent box'>"
      " <div class='spacer'></div>"
      "</div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* box = GetDocument().getElementById("box");
  HistogramTester histogram_tester;

  // Test touch scroll.
  Scroll(box, kWebGestureDeviceTouchscreen);
  EXPECT_TOUCH_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_TOUCH_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);

  Scroll(box, kWebGestureDeviceTouchscreen);
  EXPECT_TOUCH_BUCKET(kHasOpacityAndLCDText, 2);
  EXPECT_TOUCH_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 2);
  EXPECT_TOUCH_TOTAL(4);

  // Test wheel scroll.
  Scroll(box, kWebGestureDeviceTouchpad);
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);
}

TEST_F(NonCompositedMainThreadScrollingReasonRecordTest,
       CompositedScrollableAreaTest) {
  SetHtmlInnerHTML(
      "<style>"
      " .box { overflow:scroll; width: 100px; height: 100px; }"
      " .translucent { opacity: 0.5; }"
      " .composited { will-change: transform; }"
      " .spacer { height: 1000px; }"
      "</style>"
      "<div id='box' class='translucent box'>"
      " <div class='spacer'></div>"
      "</div>");

  GetPage().GetSettings().SetAcceleratedCompositingEnabled(true);
  GetDocument().View()->SetParentVisible(true);
  GetDocument().View()->SetSelfVisible(true);
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* box = GetDocument().getElementById("box");
  HistogramTester histogram_tester;

  Scroll(box, kWebGestureDeviceTouchpad);
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);

  box->setAttribute("class", "composited translucent box");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Scroll(box, kWebGestureDeviceTouchpad);
  EXPECT_FALSE(ToLayoutBox(box->GetLayoutObject())
                   ->GetScrollableArea()
                   ->GetNonCompositedMainThreadScrollingReasons());
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);
}

TEST_F(NonCompositedMainThreadScrollingReasonRecordTest,
       NotScrollableAreaTest) {
  SetHtmlInnerHTML(
      "<style>.box { overflow:scroll; width: 100px; height: 100px; }"
      " .translucent { opacity: 0.5; }"
      " .hidden { overflow: hidden; }"
      " .spacer { height: 1000px; }"
      "</style>"
      "<div id='box' class='translucent box'>"
      " <div class='spacer'></div>"
      "</div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* box = GetDocument().getElementById("box");
  HistogramTester histogram_tester;

  Scroll(box, kWebGestureDeviceTouchpad);
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);

  box->setAttribute("class", "hidden translucent box");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Scroll(box, kWebGestureDeviceTouchpad);
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_TOTAL(2);
}

TEST_F(NonCompositedMainThreadScrollingReasonRecordTest, NestedScrollersTest) {
  SetHtmlInnerHTML(
      "<style>"
      " .container { overflow:scroll; width: 200px; height: 200px; }"
      " .box { overflow:scroll; width: 100px; height: 100px; }"
      " .translucent { opacity: 0.5; }"
      " .transform { transform: scale(0.8); }"
      " .with-border-radius { border: 5px solid; border-radius: 5px; }"
      " .spacer { height: 1000px; }"
      " .composited { will-change: transform; }"
      "</style>"
      "<div id='container' class='container with-border-radius'>"
      "  <div class='translucent box'>"
      "    <div id='inner' class='composited transform box'>"
      "      <div class='spacer'></div>"
      "    </div>"
      "  </div>"
      "  <div class='spacer'></div>"
      "</div>");

  GetPage().GetSettings().SetAcceleratedCompositingEnabled(true);
  GetDocument().View()->SetParentVisible(true);
  GetDocument().View()->SetSelfVisible(true);
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* box = GetDocument().getElementById("inner");
  HistogramTester histogram_tester;

  Scroll(box, kWebGestureDeviceTouchpad);
  // Scrolling the inner box will gather reasons from the scrolling chain. The
  // inner box itself has no reason because it's composited. Other scrollable
  // areas from the chain have corresponding reasons.
  EXPECT_WHEEL_BUCKET(kHasOpacityAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kBackgroundNotOpaqueInRectAndLCDText, 1);
  EXPECT_WHEEL_BUCKET(kHasBorderRadius, 1);
  EXPECT_WHEEL_BUCKET(kHasTransformAndLCDText, 0);
  EXPECT_WHEEL_TOTAL(3);
}

}  // namespace blink
