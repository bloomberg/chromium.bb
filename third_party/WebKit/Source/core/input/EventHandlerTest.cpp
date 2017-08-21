// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/EventHandler.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionController.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutObject.h"
#include "core/loader/EmptyClients.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  ShadowRoot* SetShadowContent(const char* shadow_content, const char* host);

 protected:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
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

ShadowRoot* EventHandlerTest::SetShadowContent(const char* shadow_content,
                                               const char* host) {
  ShadowRoot* shadow_root =
      EditingTestBase::CreateShadowRootForElementWithIDAndSetInnerHTML(
          GetDocument(), host, shadow_content);
  return shadow_root;
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

  LocalFrameView* frame_view = GetDocument().View();
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

  ASSERT_TRUE(GetDocument()
                  .GetFrame()
                  ->GetEventHandler()
                  .GetSelectionController()
                  .MouseDownMayStartSelect());

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

  ASSERT_FALSE(GetDocument()
                   .GetFrame()
                   ->GetEventHandler()
                   .GetSelectionController()
                   .MouseDownMayStartSelect());

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  Range* range =
      CreateRange(EphemeralRange(Selection().GetSelectionInDOMTree().Base(),
                                 Selection().GetSelectionInDOMTree().Extent()));
  ASSERT_TRUE(range);
  EXPECT_EQ("Line 1\nLine 2", range->GetText());
}

TEST_F(EventHandlerTest, multiClickSelectionFromTap) {
  SetHtmlInnerHTML(
      "<style> body { margin: 0px; } .line { display: block; width: 300px; "
      "height: 30px; } </style>"
      "<body contenteditable='true'><span class='line' id='line'>One Two "
      "Three</span></body>");

  Node* line = GetDocument().getElementById("line")->firstChild();

  TapEventBuilder single_tap_event(IntPoint(0, 0), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Base());

  // Multi-tap events on editable elements should trigger selection, just
  // like multi-click events.
  TapEventBuilder double_tap_event(IntPoint(0, 0), 2);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      double_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Base());
  if (GetDocument()
          .GetFrame()
          ->GetEditor()
          .IsSelectTrailingWhitespaceEnabled()) {
    EXPECT_EQ(Position(line, 4), Selection().GetSelectionInDOMTree().Extent());
    EXPECT_EQ("One ", WebString(Selection().SelectedText()).Utf8());
  } else {
    EXPECT_EQ(Position(line, 3), Selection().GetSelectionInDOMTree().Extent());
    EXPECT_EQ("One", WebString(Selection().SelectedText()).Utf8());
  }

  TapEventBuilder triple_tap_event(IntPoint(0, 0), 3);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      triple_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Base());
  EXPECT_EQ(Position(line, 13), Selection().GetSelectionInDOMTree().Extent());
  EXPECT_EQ("One Two Three", WebString(Selection().SelectedText()).Utf8());
}

TEST_F(EventHandlerTest, multiClickSelectionFromTapDisabledIfNotEditable) {
  SetHtmlInnerHTML(
      "<style> body { margin: 0px; } .line { display: block; width: 300px; "
      "height: 30px; } </style>"
      "<span class='line' id='line'>One Two Three</span>");

  Node* line = GetDocument().getElementById("line")->firstChild();

  TapEventBuilder single_tap_event(IntPoint(0, 0), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Base());

  // As the text is readonly, multi-tap events should not trigger selection.
  TapEventBuilder double_tap_event(IntPoint(0, 0), 2);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      double_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Base());

  TapEventBuilder triple_tap_event(IntPoint(0, 0), 3);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      triple_tap_event);
  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(line, 0), Selection().GetSelectionInDOMTree().Base());
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

TEST_F(EventHandlerTest, HitOnNothingDoesNotShowIBeam) {
  SetHtmlInnerHTML("");
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtPoint(
          LayoutPoint(10, 10));
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(
          GetDocument().body(), hit));
}

TEST_F(EventHandlerTest, HitOnTextShowsIBeam) {
  SetHtmlInnerHTML("blabla");
  Node* const text = GetDocument().body()->firstChild();
  LayoutPoint location = text->GetLayoutObject()->VisualRect().Center();
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtPoint(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, HitOnUserSelectNoneDoesNotShowIBeam) {
  SetHtmlInnerHTML("<span style='user-select: none'>blabla</span>");
  Node* const text = GetDocument().body()->firstChild()->firstChild();
  LayoutPoint location = text->GetLayoutObject()->VisualRect().Center();
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtPoint(
          location);
  EXPECT_FALSE(text->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, ChildCanOverrideUserSelectNone) {
  SetHtmlInnerHTML(
      "<div style='user-select: none'>"
      "<span style='user-select: text'>blabla</span>"
      "</div>");
  Node* const text = GetDocument().body()->firstChild()->firstChild()->firstChild();
  LayoutPoint location = text->GetLayoutObject()->VisualRect().Center();
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtPoint(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, ShadowChildCanOverrideUserSelectNone) {
  SetHtmlInnerHTML("<p style='user-select: none' id='host'></p>");
  ShadowRoot* const shadow_root = SetShadowContent(
      "<span style='user-select: text' id='bla'>blabla</span>", "host");

  Node* const text = shadow_root->getElementById("bla")->firstChild();
  LayoutPoint location = text->GetLayoutObject()->VisualRect().Center();
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtPoint(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, ChildCanOverrideUserSelectText) {
  SetHtmlInnerHTML(
      "<div style='user-select: text'>"
      "<span style='user-select: none'>blabla</span>"
      "</div>");
  Node* const text = GetDocument().body()->firstChild()->firstChild()->firstChild();
  LayoutPoint location = text->GetLayoutObject()->VisualRect().Center();
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtPoint(
          location);
  EXPECT_FALSE(text->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, ShadowChildCanOverrideUserSelectText) {
  SetHtmlInnerHTML("<p style='user-select: text' id='host'></p>");
  ShadowRoot* const shadow_root = SetShadowContent(
      "<span style='user-select: none' id='bla'>blabla</span>", "host");

  Node* const text = shadow_root->getElementById("bla")->firstChild();
  LayoutPoint location = text->GetLayoutObject()->VisualRect().Center();
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtPoint(
          location);
  EXPECT_FALSE(text->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, InputFieldsCanStartSelection) {
  SetHtmlInnerHTML("<input value='blabla'>");
  Element* const field = ToElement(GetDocument().body()->firstChild());
  ShadowRoot* const shadow_root = field->UserAgentShadowRoot();

  Element* const text = shadow_root->getElementById("inner-editor");
  LayoutPoint location = text->GetLayoutObject()->VisualRect().Center();
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtPoint(
          location);
  EXPECT_TRUE(text->CanStartSelection());
  EXPECT_TRUE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(text,
                                                                         hit));
}

TEST_F(EventHandlerTest, ImagesCannotStartSelection) {
  SetHtmlInnerHTML("<img>");
  Element* const img = ToElement(GetDocument().body()->firstChild());
  LayoutPoint location = img->GetLayoutObject()->VisualRect().Center();
  HitTestResult hit =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtPoint(
          location);
  EXPECT_FALSE(img->CanStartSelection());
  EXPECT_FALSE(
      GetDocument().GetFrame()->GetEventHandler().ShouldShowIBeamForNode(img,
                                                                         hit));
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

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, NonEmptyTextfieldInsertionOnTap) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  TapEventBuilder single_tap_event(IntPoint(200, 200), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, NewlineDivInsertionOnTap) {
  SetHtmlInnerHTML("<div contenteditable><br/></div>");

  TapEventBuilder single_tap_event(IntPoint(10, 10), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, EmptyTextfieldInsertionOnLongPress) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50></textarea>");

  LongPressEventBuilder long_press_event(IntPoint(200, 200));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      long_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());

  // Single Tap on an empty edit field should clear insertion handle
  TapEventBuilder single_tap_event(IntPoint(200, 200), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, NonEmptyTextfieldInsertionOnLongPress) {
  SetHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  LongPressEventBuilder long_press_event(IntPoint(200, 200));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      long_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, ClearHandleAfterTap) {
  SetHtmlInnerHTML("<textarea cols=50  rows=10>Enter text</textarea>");

  // Show handle
  LongPressEventBuilder long_press_event(IntPoint(200, 10));
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      long_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
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

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());

  MousePressEventBuilder right_mouse_press_event(
      IntPoint(200, 200), 1, WebPointerProperties::Button::kRight);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      right_mouse_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_FALSE(Selection().IsHandleVisible());

  MousePressEventBuilder double_click_mouse_press_event(
      IntPoint(200, 200), 2, WebPointerProperties::Button::kLeft);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      double_click_mouse_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  ASSERT_FALSE(Selection().IsHandleVisible());

  MousePressEventBuilder triple_click_mouse_press_event(
      IntPoint(200, 200), 3, WebPointerProperties::Button::kLeft);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      triple_click_mouse_press_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  ASSERT_FALSE(Selection().IsHandleVisible());
}

TEST_F(EventHandlerTest, MisspellingContextMenuEvent) {
  if (GetDocument()
          .GetFrame()
          ->GetEditor()
          .Behavior()
          .ShouldSelectOnContextualMenuClick())
    return;

  SetHtmlInnerHTML("<textarea cols=50 rows=50>Mispellinggg</textarea>");

  TapEventBuilder single_tap_event(IntPoint(10, 10), 1);
  GetDocument().GetFrame()->GetEventHandler().HandleGestureEvent(
      single_tap_event);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());

  GetDocument().GetFrame()->GetEventHandler().ShowNonLocatedContextMenu(
      nullptr, kMenuSourceTouchHandle);

  ASSERT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  ASSERT_TRUE(Selection().IsHandleVisible());
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

// This test mouse move with modifier kRelativeMotionEvent
// should not start drag.
TEST_F(EventHandlerTest, FakeMouseMoveNotStartDrag) {
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

  WebMouseEvent fake_mouse_move(
      WebMouseEvent::kMouseMove, WebFloatPoint(618, 298),
      WebFloatPoint(685, 436), WebPointerProperties::Button::kLeft, 1,
      WebInputEvent::Modifiers::kLeftButtonDown |
          WebInputEvent::Modifiers::kRelativeMotionEvent,
      WebInputEvent::kTimeStampForTesting);
  fake_mouse_move.SetFrameScale(1);
  EXPECT_EQ(WebInputEventResult::kHandledSuppressed,
            GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
                fake_mouse_move, Vector<WebMouseEvent>()));

  EXPECT_EQ(IntPoint(0, 0), GetDocument()
                                .GetFrame()
                                ->GetEventHandler()
                                .DragDataTransferLocationForTesting());
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

}  // namespace blink
