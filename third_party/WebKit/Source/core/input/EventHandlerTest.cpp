// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/EventHandler.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/loader/EmptyClients.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class EventHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override;

  Page& page() const { return m_dummyPageHolder->page(); }
  Document& document() const { return m_dummyPageHolder->document(); }
  FrameSelection& selection() const { return document().frame()->selection(); }

  void setHtmlInnerHTML(const char* htmlContent);

 protected:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

class TapEventBuilder : public WebGestureEvent {
 public:
  TapEventBuilder(IntPoint position, int tapCount)
      : WebGestureEvent(WebInputEvent::GestureTap,
                        WebInputEvent::NoModifiers,
                        TimeTicks::Now().InSeconds()) {
    x = globalX = position.x();
    y = globalY = position.y();
    sourceDevice = WebGestureDeviceTouchscreen;
    data.tap.tapCount = tapCount;
    data.tap.width = 5;
    data.tap.height = 5;
    m_frameScale = 1;
  }
};

class LongPressEventBuilder : public WebGestureEvent {
 public:
  LongPressEventBuilder(IntPoint position) : WebGestureEvent() {
    m_type = WebInputEvent::GestureLongPress;
    x = globalX = position.x();
    y = globalY = position.y();
    sourceDevice = WebGestureDeviceTouchscreen;
    data.longPress.width = 5;
    data.longPress.height = 5;
    m_frameScale = 1;
  }
};

class MousePressEventBuilder : public WebMouseEvent {
 public:
  MousePressEventBuilder(IntPoint position,
                         int clickCountParam,
                         WebMouseEvent::Button buttonParam)
      : WebMouseEvent(WebInputEvent::MouseDown,
                      WebInputEvent::NoModifiers,
                      TimeTicks::Now().InSeconds()) {
    clickCount = clickCountParam;
    button = buttonParam;
    x = globalX = position.x();
    y = globalY = position.y();
    m_frameScale = 1;
  }
};

void EventHandlerTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(300, 400));
}

void EventHandlerTest::setHtmlInnerHTML(const char* htmlContent) {
  document().documentElement()->setInnerHTML(String::fromUTF8(htmlContent));
  document().view()->updateAllLifecyclePhases();
}

TEST_F(EventHandlerTest, dragSelectionAfterScroll) {
  setHtmlInnerHTML(
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

  FrameView* frameView = document().view();
  frameView->layoutViewportScrollableArea()->setScrollOffset(
      ScrollOffset(0, 400), ProgrammaticScroll);

  WebMouseEvent mouseDownEvent(WebInputEvent::MouseDown, WebFloatPoint(0, 0),
                               WebFloatPoint(100, 200),
                               WebPointerProperties::Button::Left, 1,
                               WebInputEvent::Modifiers::LeftButtonDown,
                               WebInputEvent::TimeStampForTesting);
  mouseDownEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMousePressEvent(mouseDownEvent);

  WebMouseEvent mouseMoveEvent(WebInputEvent::MouseMove, WebFloatPoint(100, 50),
                               WebFloatPoint(200, 250),
                               WebPointerProperties::Button::Left, 1,
                               WebInputEvent::Modifiers::LeftButtonDown,
                               WebInputEvent::TimeStampForTesting);
  mouseMoveEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMouseMoveEvent(
      mouseMoveEvent, Vector<WebMouseEvent>());

  page().autoscrollController().animate(WTF::monotonicallyIncreasingTime());
  page().animator().serviceScriptedAnimations(
      WTF::monotonicallyIncreasingTime());

  WebMouseEvent mouseUpEvent(
      WebMouseEvent::MouseUp, WebFloatPoint(100, 50), WebFloatPoint(200, 250),
      WebPointerProperties::Button::Left, 1, WebInputEvent::NoModifiers,
      WebInputEvent::TimeStampForTesting);
  mouseUpEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMouseReleaseEvent(mouseUpEvent);

  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isRange());
  Range* range = createRange(selection()
                                 .computeVisibleSelectionInDOMTreeDeprecated()
                                 .toNormalizedEphemeralRange());
  ASSERT_TRUE(range);
  EXPECT_EQ("Line 1\nLine 2", range->text());
}

TEST_F(EventHandlerTest, multiClickSelectionFromTap) {
  setHtmlInnerHTML(
      "<style> body { margin: 0px; } .line { display: block; width: 300px; "
      "height: 30px; } </style>"
      "<body contenteditable='true'><span class='line' id='line'>One Two "
      "Three</span></body>");

  Node* line = document().getElementById("line")->firstChild();

  TapEventBuilder singleTapEvent(IntPoint(0, 0), 1);
  document().frame()->eventHandler().handleGestureEvent(singleTapEvent);
  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isCaret());
  EXPECT_EQ(Position(line, 0),
            selection().computeVisibleSelectionInDOMTreeDeprecated().start());

  // Multi-tap events on editable elements should trigger selection, just
  // like multi-click events.
  TapEventBuilder doubleTapEvent(IntPoint(0, 0), 2);
  document().frame()->eventHandler().handleGestureEvent(doubleTapEvent);
  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isRange());
  EXPECT_EQ(Position(line, 0),
            selection().computeVisibleSelectionInDOMTreeDeprecated().start());
  if (document().frame()->editor().isSelectTrailingWhitespaceEnabled()) {
    EXPECT_EQ(Position(line, 4),
              selection().computeVisibleSelectionInDOMTreeDeprecated().end());
    EXPECT_EQ("One ", WebString(selection().selectedText()).utf8());
  } else {
    EXPECT_EQ(Position(line, 3),
              selection().computeVisibleSelectionInDOMTreeDeprecated().end());
    EXPECT_EQ("One", WebString(selection().selectedText()).utf8());
  }

  TapEventBuilder tripleTapEvent(IntPoint(0, 0), 3);
  document().frame()->eventHandler().handleGestureEvent(tripleTapEvent);
  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isRange());
  EXPECT_EQ(Position(line, 0),
            selection().computeVisibleSelectionInDOMTreeDeprecated().start());
  EXPECT_EQ(Position(line, 13),
            selection().computeVisibleSelectionInDOMTreeDeprecated().end());
  EXPECT_EQ("One Two Three", WebString(selection().selectedText()).utf8());
}

TEST_F(EventHandlerTest, multiClickSelectionFromTapDisabledIfNotEditable) {
  setHtmlInnerHTML(
      "<style> body { margin: 0px; } .line { display: block; width: 300px; "
      "height: 30px; } </style>"
      "<span class='line' id='line'>One Two Three</span>");

  Node* line = document().getElementById("line")->firstChild();

  TapEventBuilder singleTapEvent(IntPoint(0, 0), 1);
  document().frame()->eventHandler().handleGestureEvent(singleTapEvent);
  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isCaret());
  EXPECT_EQ(Position(line, 0),
            selection().computeVisibleSelectionInDOMTreeDeprecated().start());

  // As the text is readonly, multi-tap events should not trigger selection.
  TapEventBuilder doubleTapEvent(IntPoint(0, 0), 2);
  document().frame()->eventHandler().handleGestureEvent(doubleTapEvent);
  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isCaret());
  EXPECT_EQ(Position(line, 0),
            selection().computeVisibleSelectionInDOMTreeDeprecated().start());

  TapEventBuilder tripleTapEvent(IntPoint(0, 0), 3);
  document().frame()->eventHandler().handleGestureEvent(tripleTapEvent);
  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isCaret());
  EXPECT_EQ(Position(line, 0),
            selection().computeVisibleSelectionInDOMTreeDeprecated().start());
}

TEST_F(EventHandlerTest, draggedInlinePositionTest) {
  setHtmlInnerHTML(
      "<style>"
      "body { margin: 0px; }"
      ".line { font-family: sans-serif; background: blue; width: 300px; "
      "height: 30px; font-size: 40px; margin-left: 250px; }"
      "</style>"
      "<div style='width: 300px; height: 100px;'>"
      "<span class='line' draggable='true'>abcd</span>"
      "</div>");
  WebMouseEvent mouseDownEvent(WebMouseEvent::MouseDown, WebFloatPoint(262, 29),
                               WebFloatPoint(329, 67),
                               WebPointerProperties::Button::Left, 1,
                               WebInputEvent::Modifiers::LeftButtonDown,
                               WebInputEvent::TimeStampForTesting);
  mouseDownEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMousePressEvent(mouseDownEvent);

  WebMouseEvent mouseMoveEvent(WebMouseEvent::MouseMove,
                               WebFloatPoint(618, 298), WebFloatPoint(685, 436),
                               WebPointerProperties::Button::Left, 1,
                               WebInputEvent::Modifiers::LeftButtonDown,
                               WebInputEvent::TimeStampForTesting);
  mouseMoveEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMouseMoveEvent(
      mouseMoveEvent, Vector<WebMouseEvent>());

  EXPECT_EQ(
      IntPoint(12, 29),
      document().frame()->eventHandler().dragDataTransferLocationForTesting());
}

TEST_F(EventHandlerTest, draggedSVGImagePositionTest) {
  setHtmlInnerHTML(
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
  WebMouseEvent mouseDownEvent(WebMouseEvent::MouseDown,
                               WebFloatPoint(145, 144), WebFloatPoint(212, 282),
                               WebPointerProperties::Button::Left, 1,
                               WebInputEvent::Modifiers::LeftButtonDown,
                               WebInputEvent::TimeStampForTesting);
  mouseDownEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMousePressEvent(mouseDownEvent);

  WebMouseEvent mouseMoveEvent(WebMouseEvent::MouseMove,
                               WebFloatPoint(618, 298), WebFloatPoint(685, 436),
                               WebPointerProperties::Button::Left, 1,
                               WebInputEvent::Modifiers::LeftButtonDown,
                               WebInputEvent::TimeStampForTesting);
  mouseMoveEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMouseMoveEvent(
      mouseMoveEvent, Vector<WebMouseEvent>());

  EXPECT_EQ(
      IntPoint(45, 44),
      document().frame()->eventHandler().dragDataTransferLocationForTesting());
}

// Regression test for http://crbug.com/641403 to verify we use up-to-date
// layout tree for dispatching "contextmenu" event.
TEST_F(EventHandlerTest, sendContextMenuEventWithHover) {
  setHtmlInnerHTML(
      "<style>*:hover { color: red; }</style>"
      "<div>foo</div>");
  document().settings()->setScriptEnabled(true);
  Element* script = document().createElement("script");
  script->setInnerHTML(
      "document.addEventListener('contextmenu', event => "
      "event.preventDefault());");
  document().body()->appendChild(script);
  document().updateStyleAndLayout();
  document().frame()->selection().setSelection(
      SelectionInDOMTree::Builder()
          .collapse(Position(document().body(), 0))
          .build());
  WebMouseEvent mouseDownEvent(
      WebMouseEvent::MouseDown, WebFloatPoint(0, 0), WebFloatPoint(100, 200),
      WebPointerProperties::Button::Right, 1,
      WebInputEvent::Modifiers::RightButtonDown, TimeTicks::Now().InSeconds());
  mouseDownEvent.setFrameScale(1);
  EXPECT_EQ(
      WebInputEventResult::HandledApplication,
      document().frame()->eventHandler().sendContextMenuEvent(mouseDownEvent));
}

TEST_F(EventHandlerTest, EmptyTextfieldInsertionOnTap) {
  setHtmlInnerHTML("<textarea cols=50 rows=50></textarea>");

  TapEventBuilder singleTapEvent(IntPoint(200, 200), 1);
  document().frame()->eventHandler().handleGestureEvent(singleTapEvent);

  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isCaret());
  ASSERT_FALSE(selection().isHandleVisible());
}

TEST_F(EventHandlerTest, NonEmptyTextfieldInsertionOnTap) {
  setHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  TapEventBuilder singleTapEvent(IntPoint(200, 200), 1);
  document().frame()->eventHandler().handleGestureEvent(singleTapEvent);

  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isCaret());
  ASSERT_TRUE(selection().isHandleVisible());
}

TEST_F(EventHandlerTest, EmptyTextfieldInsertionOnLongPress) {
  setHtmlInnerHTML("<textarea cols=50 rows=50></textarea>");

  LongPressEventBuilder longPressEvent(IntPoint(200, 200));
  document().frame()->eventHandler().handleGestureEvent(longPressEvent);

  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isCaret());
  ASSERT_TRUE(selection().isHandleVisible());

  // Single Tap on an empty edit field should clear insertion handle
  TapEventBuilder singleTapEvent(IntPoint(200, 200), 1);
  document().frame()->eventHandler().handleGestureEvent(singleTapEvent);

  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isCaret());
  ASSERT_FALSE(selection().isHandleVisible());
}

TEST_F(EventHandlerTest, NonEmptyTextfieldInsertionOnLongPress) {
  setHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  LongPressEventBuilder longPressEvent(IntPoint(200, 200));
  document().frame()->eventHandler().handleGestureEvent(longPressEvent);

  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isCaret());
  ASSERT_TRUE(selection().isHandleVisible());
}

TEST_F(EventHandlerTest, ClearHandleAfterTap) {
  setHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  // Show handle
  LongPressEventBuilder longPressEvent(IntPoint(200, 200));
  document().frame()->eventHandler().handleGestureEvent(longPressEvent);

  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isCaret());
  ASSERT_TRUE(selection().isHandleVisible());

  // Tap away from text area should clear handle
  TapEventBuilder singleTapEvent(IntPoint(700, 700), 1);
  document().frame()->eventHandler().handleGestureEvent(singleTapEvent);

  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isNone());
  ASSERT_FALSE(selection().isHandleVisible());
}

TEST_F(EventHandlerTest, HandleNotShownOnMouseEvents) {
  setHtmlInnerHTML("<textarea cols=50 rows=50>Enter text</textarea>");

  MousePressEventBuilder leftMousePressEvent(
      IntPoint(200, 200), 1, WebPointerProperties::Button::Left);
  document().frame()->eventHandler().handleMousePressEvent(leftMousePressEvent);

  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isCaret());
  ASSERT_FALSE(selection().isHandleVisible());

  MousePressEventBuilder rightMousePressEvent(
      IntPoint(200, 200), 1, WebPointerProperties::Button::Right);
  document().frame()->eventHandler().handleMousePressEvent(
      rightMousePressEvent);

  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isCaret());
  ASSERT_FALSE(selection().isHandleVisible());

  MousePressEventBuilder doubleClickMousePressEvent(
      IntPoint(200, 200), 2, WebPointerProperties::Button::Left);
  document().frame()->eventHandler().handleMousePressEvent(
      doubleClickMousePressEvent);

  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isRange());
  ASSERT_FALSE(selection().isHandleVisible());

  MousePressEventBuilder tripleClickMousePressEvent(
      IntPoint(200, 200), 3, WebPointerProperties::Button::Left);
  document().frame()->eventHandler().handleMousePressEvent(
      tripleClickMousePressEvent);

  ASSERT_TRUE(
      selection().computeVisibleSelectionInDOMTreeDeprecated().isRange());
  ASSERT_FALSE(selection().isHandleVisible());
}

TEST_F(EventHandlerTest, dragEndInNewDrag) {
  setHtmlInnerHTML(
      "<style>.box { width: 100px; height: 100px; display: block; }</style>"
      "<a class='box' href=''>Drag me</a>");

  WebMouseEvent mouseDownEvent(
      WebInputEvent::MouseDown, WebFloatPoint(50, 50), WebFloatPoint(50, 50),
      WebPointerProperties::Button::Left, 1,
      WebInputEvent::Modifiers::LeftButtonDown, TimeTicks::Now().InSeconds());
  mouseDownEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMousePressEvent(mouseDownEvent);

  WebMouseEvent mouseMoveEvent(
      WebInputEvent::MouseMove, WebFloatPoint(51, 50), WebFloatPoint(51, 50),
      WebPointerProperties::Button::Left, 1,
      WebInputEvent::Modifiers::LeftButtonDown, TimeTicks::Now().InSeconds());
  mouseMoveEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMouseMoveEvent(
      mouseMoveEvent, Vector<WebMouseEvent>());

  // This reproduces what might be the conditions of http://crbug.com/677916
  //
  // TODO(crbug.com/682047): The call sequence below should not occur outside
  // this contrived test. Given the current code, it is unclear how the
  // dragSourceEndedAt() call could occur before a drag operation is started.

  WebMouseEvent mouseUpEvent(
      WebInputEvent::MouseUp, WebFloatPoint(100, 50), WebFloatPoint(200, 250),
      WebPointerProperties::Button::Left, 1, WebInputEvent::NoModifiers,
      TimeTicks::Now().InSeconds());
  mouseUpEvent.setFrameScale(1);
  document().frame()->eventHandler().dragSourceEndedAt(mouseUpEvent,
                                                       DragOperationNone);

  // This test passes if it doesn't crash.
}

class TooltipCapturingChromeClient : public EmptyChromeClient {
 public:
  TooltipCapturingChromeClient() {}

  void setToolTip(LocalFrame&, const String& str, TextDirection) override {
    m_lastToolTip = str;
  }

  String& lastToolTip() { return m_lastToolTip; }

 private:
  String m_lastToolTip;
};

class EventHandlerTooltipTest : public EventHandlerTest {
 public:
  EventHandlerTooltipTest() {}

  void SetUp() override {
    m_chromeClient = new TooltipCapturingChromeClient();
    Page::PageClients clients;
    fillWithEmptyClients(clients);
    clients.chromeClient = m_chromeClient.get();
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600), &clients);
  }

  String& lastToolTip() { return m_chromeClient->lastToolTip(); }

 private:
  Persistent<TooltipCapturingChromeClient> m_chromeClient;
};

TEST_F(EventHandlerTooltipTest, mouseLeaveClearsTooltip) {
  setHtmlInnerHTML(
      "<style>.box { width: 100%; height: 100%; }</style>"
      "<img src='image.png' class='box' title='tooltip'>link</img>");

  EXPECT_EQ(WTF::String(), lastToolTip());

  WebMouseEvent mouseMoveEvent(
      WebInputEvent::MouseMove, WebFloatPoint(51, 50), WebFloatPoint(51, 50),
      WebPointerProperties::Button::NoButton, 0, WebInputEvent::NoModifiers,
      TimeTicks::Now().InSeconds());
  mouseMoveEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMouseMoveEvent(
      mouseMoveEvent, Vector<WebMouseEvent>());

  EXPECT_EQ("tooltip", lastToolTip());

  WebMouseEvent mouseLeaveEvent(
      WebInputEvent::MouseLeave, WebFloatPoint(0, 0), WebFloatPoint(0, 0),
      WebPointerProperties::Button::NoButton, 0, WebInputEvent::NoModifiers,
      TimeTicks::Now().InSeconds());
  mouseLeaveEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMouseLeaveEvent(mouseLeaveEvent);

  EXPECT_EQ(WTF::String(), lastToolTip());
}

}  // namespace blink
