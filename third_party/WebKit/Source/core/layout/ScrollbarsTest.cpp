// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "platform/scroll/ScrollbarThemeOverlayMock.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebThemeEngine.h"
#include "public/web/WebScriptSource.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class ScrollbarsTest : private ScopedRootLayerScrollingForTest, public SimTest {
 public:
  ScrollbarsTest() : ScopedRootLayerScrollingForTest(true) {}
};

TEST_F(ScrollbarsTest, DocumentStyleRecalcPreservesScrollbars) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style> body { width: 1600px; height: 1200px; } </style>");
  PaintLayerScrollableArea* plsa =
      GetDocument().GetLayoutView()->GetScrollableArea();

  Compositor().BeginFrame();
  ASSERT_TRUE(plsa->VerticalScrollbar() && plsa->HorizontalScrollbar());

  // Forces recalc of LayoutView's computed style in Document::updateStyle,
  // without invalidating layout.
  MainFrame().ExecuteScriptAndReturnValue(WebScriptSource(
      "document.querySelector('style').sheet.insertRule('body {}', 1);"));

  Compositor().BeginFrame();
  ASSERT_TRUE(plsa->VerticalScrollbar() && plsa->HorizontalScrollbar());
}

// Ensure that causing a change in scrollbar existence causes a nested layout
// to recalculate the existence of the opposite scrollbar. The bug here was
// caused by trying to avoid the layout when overlays are enabled but not
// checking whether the scrollbars should be custom - which do take up layout
// space. https://crbug.com/668387.
TEST_F(ScrollbarsTest, CustomScrollbarsCauseLayoutOnExistenceChange) {
  // The bug reproduces only with RLS off. When RLS ships we can keep the test
  // but remove this setting.
  ScopedRootLayerScrollingForTest turn_off_root_layer_scrolling(false);

  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  DCHECK(ScrollbarTheme::GetTheme().UsesOverlayScrollbars());

  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style>"
      "  ::-webkit-scrollbar {"
      "      height: 16px;"
      "      width: 16px"
      "  }"
      "  ::-webkit-scrollbar-thumb {"
      "      background-color: rgba(0,0,0,.2);"
      "  }"
      "  html, body{"
      "    margin: 0;"
      "    height: 100%;"
      "  }"
      "  .box {"
      "    width: 100%;"
      "    height: 100%;"
      "  }"
      "  .transformed {"
      "    transform: translateY(100px);"
      "  }"
      "</style>"
      "<div id='box' class='box'></div>");

  ScrollableArea* layout_viewport =
      GetDocument().View()->LayoutViewportScrollableArea();

  Compositor().BeginFrame();
  ASSERT_FALSE(layout_viewport->VerticalScrollbar());
  ASSERT_FALSE(layout_viewport->HorizontalScrollbar());

  // Adding translation will cause a vertical scrollbar to appear but not dirty
  // layout otherwise. Ensure the change of scrollbar causes a layout to
  // recalculate the page width with the vertical scrollbar added.
  MainFrame().ExecuteScript(WebScriptSource(
      "document.getElementById('box').className = 'box transformed';"));
  Compositor().BeginFrame();

  ASSERT_TRUE(layout_viewport->VerticalScrollbar());
  ASSERT_FALSE(layout_viewport->HorizontalScrollbar());
}

TEST_F(ScrollbarsTest, TransparentBackgroundUsesDarkOverlayColorTheme) {
  // The bug reproduces only with RLS off. When RLS ships we can keep the test
  // but remove this setting.
  ScopedRootLayerScrollingForTest turn_off_root_layer_scrolling(false);

  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  DCHECK(ScrollbarTheme::GetTheme().UsesOverlayScrollbars());

  WebView().Resize(WebSize(800, 600));
  WebView().SetBaseBackgroundColorOverride(SK_ColorTRANSPARENT);
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style>"
      "  body{"
      "    height: 300%;"
      "  }"
      "</style>");
  Compositor().BeginFrame();

  ScrollableArea* layout_viewport =
      GetDocument().View()->LayoutViewportScrollableArea();

  EXPECT_EQ(kScrollbarOverlayColorThemeDark,
            layout_viewport->GetScrollbarOverlayColorTheme());
}

class ParameterizedScrollbarsTest : public ::testing::WithParamInterface<bool>,
                                    private ScopedRootLayerScrollingForTest,
                                    public SimTest {
 public:
  ParameterizedScrollbarsTest() : ScopedRootLayerScrollingForTest(GetParam()) {}

  HitTestResult HitTest(int x, int y) {
    return WebView().CoreHitTestResultAt(WebPoint(x, y));
  }

  EventHandler& EventHandler() {
    return GetDocument().GetFrame()->GetEventHandler();
  }

  void HandleMouseMoveEvent(int x, int y) {
    WebMouseEvent event(
        WebInputEvent::kMouseMove, WebFloatPoint(x, y), WebFloatPoint(x, y),
        WebPointerProperties::Button::kNoButton, 0, WebInputEvent::kNoModifiers,
        TimeTicks::Now().InSeconds());
    event.SetFrameScale(1);
    EventHandler().HandleMouseMoveEvent(event, Vector<WebMouseEvent>());
  }

  void HandleMousePressEvent(int x, int y) {
    WebMouseEvent event(WebInputEvent::kMouseDown, WebFloatPoint(x, y),
                        WebFloatPoint(x, y),
                        WebPointerProperties::Button::kLeft, 0,
                        WebInputEvent::Modifiers::kLeftButtonDown,
                        TimeTicks::Now().InSeconds());
    event.SetFrameScale(1);
    EventHandler().HandleMousePressEvent(event);
  }

  void HandleMouseReleaseEvent(int x, int y) {
    WebMouseEvent event(WebInputEvent::kMouseUp, WebFloatPoint(x, y),
                        WebFloatPoint(x, y),
                        WebPointerProperties::Button::kLeft, 0,
                        WebInputEvent::Modifiers::kLeftButtonDown,
                        TimeTicks::Now().InSeconds());
    event.SetFrameScale(1);
    EventHandler().HandleMouseReleaseEvent(event);
  }

  void HandleMouseLeaveEvent() {
    WebMouseEvent event(WebInputEvent::kMouseMove, WebFloatPoint(1, 1),
                        WebFloatPoint(1, 1),
                        WebPointerProperties::Button::kLeft, 0,
                        WebInputEvent::Modifiers::kLeftButtonDown,
                        TimeTicks::Now().InSeconds());
    event.SetFrameScale(1);
    EventHandler().HandleMouseLeaveEvent(event);
  }

  Cursor::Type CursorType() {
    return GetDocument()
        .GetFrame()
        ->GetChromeClient()
        .LastSetCursorForTesting()
        .GetType();
  }
};

INSTANTIATE_TEST_CASE_P(All, ParameterizedScrollbarsTest, ::testing::Bool());

// Ensure overlay scrollbar change to display:none correctly.
TEST_P(ParameterizedScrollbarsTest,
       OverlayScrollbarChangeToDisplayNoneDynamically) {
  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  DCHECK(ScrollbarTheme::GetTheme().UsesOverlayScrollbars());

  WebView().Resize(WebSize(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style>"
      ".noscrollbars::-webkit-scrollbar { display: none; }"
      "#div{ height: 100px; width:100px; overflow:scroll; }"
      ".big{ height: 2000px; }"
      "body { overflow:scroll; }"
      "</style>"
      "<div id='div'>"
      "  <div class='big'>"
      "  </div>"
      "</div>"
      "<div class='big'>"
      "</div>");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  Element* div = document.getElementById("div");

  // Ensure we have overlay scrollbar for div and root.
  ScrollableArea* scrollable_div =
      ToLayoutBox(div->GetLayoutObject())->GetScrollableArea();

  ScrollableArea* scrollable_root =
      GetDocument().View()->LayoutViewportScrollableArea();

  DCHECK(scrollable_div->VerticalScrollbar());
  DCHECK(scrollable_div->VerticalScrollbar()->IsOverlayScrollbar());

  DCHECK(!scrollable_div->HorizontalScrollbar());

  DCHECK(scrollable_root->VerticalScrollbar());
  DCHECK(scrollable_root->VerticalScrollbar()->IsOverlayScrollbar());

  // For PaintLayer Overlay Scrollbar we will remove the scrollbar when it is
  // not necessary even with overflow:scroll. Should remove after RLS ships.
  if (GetParam() == 0)
    DCHECK(scrollable_root->HorizontalScrollbar());
  else
    DCHECK(!scrollable_root->HorizontalScrollbar());

  // Set display:none.
  div->setAttribute(HTMLNames::classAttr, "noscrollbars");
  document.body()->setAttribute(HTMLNames::classAttr, "noscrollbars");
  Compositor().BeginFrame();

  EXPECT_TRUE(scrollable_div->VerticalScrollbar());
  EXPECT_TRUE(scrollable_div->VerticalScrollbar()->IsCustomScrollbar());
  EXPECT_TRUE(scrollable_div->VerticalScrollbar()->FrameRect().IsEmpty());

  EXPECT_TRUE(scrollable_div->HorizontalScrollbar());
  EXPECT_TRUE(scrollable_div->HorizontalScrollbar()->IsCustomScrollbar());
  EXPECT_TRUE(scrollable_div->HorizontalScrollbar()->FrameRect().IsEmpty());

  EXPECT_TRUE(scrollable_root->VerticalScrollbar());
  EXPECT_TRUE(scrollable_root->VerticalScrollbar()->IsCustomScrollbar());
  EXPECT_TRUE(scrollable_root->VerticalScrollbar()->FrameRect().IsEmpty());

  EXPECT_TRUE(scrollable_root->HorizontalScrollbar());
  EXPECT_TRUE(scrollable_root->HorizontalScrollbar()->IsCustomScrollbar());
  EXPECT_TRUE(scrollable_root->HorizontalScrollbar()->FrameRect().IsEmpty());
}

TEST_P(ParameterizedScrollbarsTest, scrollbarIsNotHandlingTouchpadScroll) {
  WebView().Resize(WebSize(200, 200));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style>"
      " #scrollable { height: 100px; width: 100px; overflow: scroll; }"
      " #content { height: 200px; width: 200px;}"
      "</style>"
      "<div id='scrollable'>"
      " <div id='content'></div>"
      "</div>");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  Element* scrollable = document.getElementById("scrollable");

  ScrollableArea* scrollable_area =
      ToLayoutBox(scrollable->GetLayoutObject())->GetScrollableArea();
  DCHECK(scrollable_area->VerticalScrollbar());
  WebGestureEvent scroll_begin(WebInputEvent::kGestureScrollBegin,
                               WebInputEvent::kNoModifiers,
                               TimeTicks::Now().InSeconds());
  scroll_begin.x = scroll_begin.global_x =
      scrollable->OffsetLeft() + scrollable->OffsetWidth() - 2;
  scroll_begin.y = scroll_begin.global_y = scrollable->OffsetTop();
  scroll_begin.source_device = kWebGestureDeviceTouchpad;
  scroll_begin.data.scroll_begin.delta_x_hint = 0;
  scroll_begin.data.scroll_begin.delta_y_hint = 10;
  scroll_begin.SetFrameScale(1);
  EventHandler().HandleGestureScrollEvent(scroll_begin);
  DCHECK(!EventHandler().IsScrollbarHandlingGestures());
  bool should_update_capture = false;
  DCHECK(!scrollable_area->VerticalScrollbar()->GestureEvent(
      scroll_begin, &should_update_capture));
}

TEST_P(ParameterizedScrollbarsTest,
       HidingScrollbarsOnScrollableAreaDisablesScrollbars) {
  WebView().Resize(WebSize(800, 600));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style>"
      "  #scroller { overflow: scroll; width: 1000px; height: 1000px }"
      "  #spacer { width: 2000px; height: 2000px }"
      "</style>"
      "<div id='scroller'>"
      "  <div id='spacer'></div>"
      "</div>");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  LocalFrameView* frame_view = WebView().MainFrameImpl()->GetFrameView();
  Element* scroller = document.getElementById("scroller");
  ScrollableArea* scroller_area =
      ToLayoutBox(scroller->GetLayoutObject())->GetScrollableArea();
  ScrollableArea* frame_scroller_area =
      frame_view->LayoutViewportScrollableArea();

  ASSERT_TRUE(scroller_area->HorizontalScrollbar());
  ASSERT_TRUE(scroller_area->VerticalScrollbar());
  ASSERT_TRUE(frame_scroller_area->HorizontalScrollbar());
  ASSERT_TRUE(frame_scroller_area->VerticalScrollbar());

  EXPECT_FALSE(frame_scroller_area->ScrollbarsHidden());
  EXPECT_TRUE(frame_scroller_area->HorizontalScrollbar()
                  ->ShouldParticipateInHitTesting());
  EXPECT_TRUE(frame_scroller_area->VerticalScrollbar()
                  ->ShouldParticipateInHitTesting());

  EXPECT_FALSE(scroller_area->ScrollbarsHidden());
  EXPECT_TRUE(
      scroller_area->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_TRUE(
      scroller_area->VerticalScrollbar()->ShouldParticipateInHitTesting());

  frame_scroller_area->SetScrollbarsHidden(true);
  EXPECT_FALSE(frame_scroller_area->HorizontalScrollbar()
                   ->ShouldParticipateInHitTesting());
  EXPECT_FALSE(frame_scroller_area->VerticalScrollbar()
                   ->ShouldParticipateInHitTesting());
  frame_scroller_area->SetScrollbarsHidden(false);
  EXPECT_TRUE(frame_scroller_area->HorizontalScrollbar()
                  ->ShouldParticipateInHitTesting());
  EXPECT_TRUE(frame_scroller_area->VerticalScrollbar()
                  ->ShouldParticipateInHitTesting());

  scroller_area->SetScrollbarsHidden(true);
  EXPECT_FALSE(
      scroller_area->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_FALSE(
      scroller_area->VerticalScrollbar()->ShouldParticipateInHitTesting());
  scroller_area->SetScrollbarsHidden(false);
  EXPECT_TRUE(
      scroller_area->HorizontalScrollbar()->ShouldParticipateInHitTesting());
  EXPECT_TRUE(
      scroller_area->VerticalScrollbar()->ShouldParticipateInHitTesting());
}

// Ensure mouse curosr should be pointer when hover scrollbar.
TEST_P(ParameterizedScrollbarsTest, MouseOverScrollbarInCustomCursorElement) {
  WebView().Resize(WebSize(250, 250));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style>"
      "body {"
      "  margin: 0;"
      "}"
      "#d1 {"
      "  width: 200px;"
      "  height: 200px;"
      "  overflow: auto;"
      "  cursor: move;"
      "}"
      "#d2 {"
      "  height: 400px;"
      "}"
      "</style>"
      "<div id='d1'>"
      "    <div id='d2'></div>"
      "</div>");
  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* div = document.getElementById("d1");

  // Ensure hittest has DIV and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 5);

  EXPECT_EQ(hit_test_result.InnerElement(), div);
  EXPECT_TRUE(hit_test_result.GetScrollbar());

  HandleMouseMoveEvent(195, 5);

  EXPECT_EQ(Cursor::Type::kPointer, CursorType());
}

// Makes sure that mouse hover over an overlay scrollbar doesn't activate
// elements below(except the Element that owns the scrollbar) unless the
// scrollbar is faded out.
TEST_P(ParameterizedScrollbarsTest, MouseOverLinkAndOverlayScrollbar) {
  WebView().Resize(WebSize(20, 20));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<a id='a' href='javascript:void(0);'>"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "</a>"
      "<div style='position: absolute; top: 1000px'>"
      "  end"
      "</div>");

  Compositor().BeginFrame();

  Document& document = GetDocument();
  Element* a_tag = document.getElementById("a");

  // Ensure hittest only has scrollbar.
  HitTestResult hit_test_result = HitTest(18, a_tag->OffsetTop());

  EXPECT_FALSE(hit_test_result.URLElement());
  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_FALSE(hit_test_result.GetScrollbar()->IsCustomScrollbar());

  // Mouse over link. Mouse cursor should be hand.
  HandleMouseMoveEvent(a_tag->OffsetLeft(), a_tag->OffsetTop());

  EXPECT_EQ(Cursor::Type::kHand, CursorType());

  // Mouse over enabled overlay scrollbar. Mouse cursor should be pointer and no
  // active hover element.
  HandleMouseMoveEvent(18, a_tag->OffsetTop());

  EXPECT_EQ(Cursor::Type::kPointer, CursorType());

  HandleMousePressEvent(18, a_tag->OffsetTop());

  EXPECT_TRUE(document.GetActiveElement());
  EXPECT_TRUE(document.HoverElement());

  HandleMouseReleaseEvent(18, a_tag->OffsetTop());

  // Mouse over disabled overlay scrollbar. Mouse cursor should be hand and has
  // active hover element.
  WebView()
      .MainFrameImpl()
      ->GetFrameView()
      ->LayoutViewportScrollableArea()
      ->SetScrollbarsHidden(true);

  // Ensure hittest only has link
  hit_test_result = HitTest(18, a_tag->OffsetTop());

  EXPECT_TRUE(hit_test_result.URLElement());
  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  HandleMouseMoveEvent(18, a_tag->OffsetTop());

  EXPECT_EQ(Cursor::Type::kHand, CursorType());

  HandleMousePressEvent(18, a_tag->OffsetTop());

  EXPECT_TRUE(document.GetActiveElement());
  EXPECT_TRUE(document.HoverElement());
}

// Makes sure that mouse hover over an custom scrollbar doesn't change the
// activate elements.
TEST_P(ParameterizedScrollbarsTest, MouseOverCustomScrollbar) {
  WebView().Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style>"
      "#scrollbar {"
      "  position: absolute;"
      "  top: 0;"
      "  left: 0;"
      "  height: 180px;"
      "  width: 180px;"
      "  overflow-x: auto;"
      "}"
      "::-webkit-scrollbar {"
      "  width: 8px;"
      "}"
      "::-webkit-scrollbar-thumb {"
      "  background-color: hsla(0, 0%, 56%, 0.6);"
      "}"
      "</style>"
      "<div id='scrollbar'>"
      "  <div style='position: absolute; top: 1000px;'>"
      "    make scrollbar show"
      "  </div>"
      "</div>");

  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* scrollbar_div = document.getElementById("scrollbar");
  EXPECT_TRUE(scrollbar_div);

  // Ensure hittest only has DIV
  HitTestResult hit_test_result = HitTest(1, 1);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  // Mouse over DIV
  HandleMouseMoveEvent(1, 1);

  // DIV :hover
  EXPECT_EQ(document.HoverElement(), scrollbar_div);

  // Ensure hittest has DIV and scrollbar
  hit_test_result = HitTest(175, 1);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_TRUE(hit_test_result.GetScrollbar()->IsCustomScrollbar());

  // Mouse over scrollbar
  HandleMouseMoveEvent(175, 1);

  // Custom not change the DIV :hover
  EXPECT_EQ(document.HoverElement(), scrollbar_div);
  EXPECT_EQ(hit_test_result.GetScrollbar()->HoveredPart(),
            ScrollbarPart::kThumbPart);
}

// Makes sure that mouse hover over an overlay scrollbar doesn't hover iframe
// below.
TEST_P(ParameterizedScrollbarsTest, MouseOverScrollbarAndIFrame) {
  WebView().Resize(WebSize(200, 200));

  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest frame_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<!DOCTYPE html>"
      "<style>"
      "body {"
      "  margin: 0;"
      "  height: 2000px;"
      "}"
      "iframe {"
      "  height: 200px;"
      "  width: 200px;"
      "}"
      "</style>"
      "<iframe id='iframe' src='iframe.html'>"
      "</iframe>");
  Compositor().BeginFrame();

  frame_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();

  Document& document = GetDocument();
  Element* iframe = document.getElementById("iframe");
  DCHECK(iframe);

  // Ensure hittest only has IFRAME.
  HitTestResult hit_test_result = HitTest(5, 5);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  // Mouse over IFRAME.
  HandleMouseMoveEvent(5, 5);

  // IFRAME hover.
  EXPECT_EQ(document.HoverElement(), iframe);

  // Ensure hittest has scrollbar.
  hit_test_result = HitTest(195, 5);
  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_TRUE(hit_test_result.GetScrollbar()->Enabled());

  // Mouse over scrollbar.
  HandleMouseMoveEvent(195, 5);

  // IFRAME not hover.
  EXPECT_NE(document.HoverElement(), iframe);

  // Disable the Scrollbar.
  WebView()
      .MainFrameImpl()
      ->GetFrameView()
      ->LayoutViewportScrollableArea()
      ->SetScrollbarsHidden(true);

  // Ensure hittest has IFRAME and no scrollbar.
  hit_test_result = HitTest(196, 5);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  // Mouse over disabled scrollbar.
  HandleMouseMoveEvent(196, 5);

  // IFRAME hover.
  EXPECT_EQ(document.HoverElement(), iframe);
}

// Makes sure that mouse hover over a scrollbar also hover the element owns the
// scrollbar.
TEST_P(ParameterizedScrollbarsTest, MouseOverScrollbarAndParentElement) {
  RuntimeEnabledFeatures::SetOverlayScrollbarsEnabled(false);
  WebView().Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style>"
      "#parent {"
      "  position: absolute;"
      "  top: 0;"
      "  left: 0;"
      "  height: 180px;"
      "  width: 180px;"
      "  overflow-y: scroll;"
      "}"
      "</style>"
      "<div id='parent'>"
      "  <div id='child' style='position: absolute; top: 1000px;'>"
      "    make scrollbar enabled"
      "  </div>"
      "</div>");

  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* parent_div = document.getElementById("parent");
  Element* child_div = document.getElementById("child");
  EXPECT_TRUE(parent_div);
  EXPECT_TRUE(child_div);

  ScrollableArea* scrollable_area =
      ToLayoutBox(parent_div->GetLayoutObject())->GetScrollableArea();

  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  EXPECT_FALSE(scrollable_area->VerticalScrollbar()->IsOverlayScrollbar());
  EXPECT_TRUE(scrollable_area->VerticalScrollbar()->GetTheme().IsMockTheme());

  // Ensure hittest only has DIV.
  HitTestResult hit_test_result = HitTest(1, 1);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_FALSE(hit_test_result.GetScrollbar());

  // Mouse over DIV.
  HandleMouseMoveEvent(1, 1);

  // DIV :hover.
  EXPECT_EQ(document.HoverElement(), parent_div);

  // Ensure hittest has DIV and scrollbar.
  hit_test_result = HitTest(175, 5);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_FALSE(hit_test_result.GetScrollbar()->IsCustomScrollbar());
  EXPECT_TRUE(hit_test_result.GetScrollbar()->Enabled());

  // Mouse over scrollbar.
  HandleMouseMoveEvent(175, 5);

  // Not change the DIV :hover.
  EXPECT_EQ(document.HoverElement(), parent_div);

  // Disable the Scrollbar by remove the childDiv.
  child_div->remove();
  Compositor().BeginFrame();

  // Ensure hittest has DIV and no scrollbar.
  hit_test_result = HitTest(175, 5);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());
  EXPECT_FALSE(hit_test_result.GetScrollbar()->Enabled());
  EXPECT_LT(hit_test_result.InnerElement()->clientWidth(), 180);

  // Mouse over disabled scrollbar.
  HandleMouseMoveEvent(175, 5);

  // Not change the DIV :hover.
  EXPECT_EQ(document.HoverElement(), parent_div);
}

// Makes sure that mouse over a root scrollbar also hover the html element.
TEST_P(ParameterizedScrollbarsTest, MouseOverRootScrollbar) {
  RuntimeEnabledFeatures::SetOverlayScrollbarsEnabled(false);

  WebView().Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style>"
      "body {"
      "  overflow: scroll;"
      "}"
      "</style>");

  Compositor().BeginFrame();

  Document& document = GetDocument();

  // Ensure hittest has <html> element and scrollbar.
  HitTestResult hit_test_result = HitTest(195, 5);

  EXPECT_TRUE(hit_test_result.InnerElement());
  EXPECT_EQ(hit_test_result.InnerElement(), document.documentElement());
  EXPECT_TRUE(hit_test_result.GetScrollbar());

  // Mouse over scrollbar.
  HandleMouseMoveEvent(195, 5);

  // Hover <html element.
  EXPECT_EQ(document.HoverElement(), document.documentElement());
}

TEST_P(ParameterizedScrollbarsTest, MouseReleaseUpdatesScrollbarHoveredPart) {
  WebView().Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style>"
      "#scrollbar {"
      "  position: absolute;"
      "  top: 0;"
      "  left: 0;"
      "  height: 180px;"
      "  width: 180px;"
      "  overflow-x: auto;"
      "}"
      "::-webkit-scrollbar {"
      "  width: 8px;"
      "}"
      "::-webkit-scrollbar-thumb {"
      "  background-color: hsla(0, 0%, 56%, 0.6);"
      "}"
      "</style>"
      "<div id='scrollbar'>"
      "  <div style='position: absolute; top: 1000px;'>make scrollbar "
      "shows</div>"
      "</div>");

  Compositor().BeginFrame();

  Document& document = GetDocument();

  Element* scrollbar_div = document.getElementById("scrollbar");
  EXPECT_TRUE(scrollbar_div);

  ScrollableArea* scrollable_area =
      ToLayoutBox(scrollbar_div->GetLayoutObject())->GetScrollableArea();

  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kNoPart);

  // Mouse moved over the scrollbar.
  HandleMouseMoveEvent(175, 1);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kThumbPart);

  // Mouse pressed.
  HandleMousePressEvent(175, 1);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kThumbPart);

  // Mouse moved off the scrollbar while still pressed.
  HandleMouseLeaveEvent();
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kThumbPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kThumbPart);

  // Mouse released.
  HandleMouseReleaseEvent(1, 1);
  EXPECT_EQ(scrollbar->PressedPart(), ScrollbarPart::kNoPart);
  EXPECT_EQ(scrollbar->HoveredPart(), ScrollbarPart::kNoPart);
}

TEST_P(ParameterizedScrollbarsTest,
       CustomScrollbarInOverlayScrollbarThemeWillNotCauseDCHECKFails) {
  WebView().Resize(WebSize(200, 200));

  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style type='text/css'>"
      "   ::-webkit-scrollbar {"
      "    width: 16px;"
      "    height: 16px;"
      "    overflow: visible;"
      "  }"
      "  div {"
      "    width: 1000px;"
      "  }"
      "</style>"
      "<div style='position: absolute; top: 1000px;'>"
      "  end"
      "</div>");

  // No DCHECK Fails. Issue 676678.
  Compositor().BeginFrame();
}

static void DisableCompositing(WebSettings* settings) {
  settings->SetAcceleratedCompositingEnabled(false);
  settings->SetPreferCompositingToLCDTextEnabled(false);
}

#if defined(THREAD_SANITIZER)
#define DISABLE_ON_TSAN(test_name) DISABLED_##test_name
#else
#define DISABLE_ON_TSAN(test_name) test_name
#endif  // defined(THREAD_SANITIZER)

// Make sure overlay scrollbars on non-composited scrollers fade out and set
// the hidden bit as needed.
TEST_P(ParameterizedScrollbarsTest,
       DISABLE_ON_TSAN(TestNonCompositedOverlayScrollbarsFade)) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view_impl = web_view_helper.Initialize(
      nullptr, nullptr, nullptr, &DisableCompositing);

  constexpr double kMockOverlayFadeOutDelayMs = 5.0;
  constexpr TimeDelta kMockOverlayFadeOutDelay =
      TimeDelta::FromMillisecondsD(kMockOverlayFadeOutDelayMs);

  ScrollbarTheme& theme = ScrollbarTheme::GetTheme();
  // This test relies on mock overlay scrollbars.
  ASSERT_TRUE(theme.IsMockTheme());
  ASSERT_TRUE(theme.UsesOverlayScrollbars());
  ScrollbarThemeOverlayMock& mock_overlay_theme =
      (ScrollbarThemeOverlayMock&)theme;
  mock_overlay_theme.SetOverlayScrollbarFadeOutDelay(
      kMockOverlayFadeOutDelayMs / 1000.0);

  web_view_impl->ResizeWithBrowserControls(WebSize(640, 480), 0, 0, false);

  WebURL base_url = URLTestHelpers::ToKURL("http://example.com/");
  FrameTestHelpers::LoadHTMLString(web_view_impl->MainFrameImpl(),
                                   "<!DOCTYPE html>"
                                   "<style>"
                                   "  #space {"
                                   "    width: 1000px;"
                                   "    height: 1000px;"
                                   "  }"
                                   "  #container {"
                                   "    width: 200px;"
                                   "    height: 200px;"
                                   "    overflow: scroll;"
                                   "  }"
                                   "  div { height:1000px; width: 200px; }"
                                   "</style>"
                                   "<div id='container'>"
                                   "  <div id='space'></div>"
                                   "</div>",
                                   base_url);
  web_view_impl->UpdateAllLifecyclePhases();

  WebLocalFrameImpl* frame = web_view_helper.LocalMainFrame();
  Document* document =
      ToLocalFrame(web_view_impl->GetPage()->MainFrame())->GetDocument();
  Element* container = document->getElementById("container");
  ScrollableArea* scrollable_area =
      ToLayoutBox(container->GetLayoutObject())->GetScrollableArea();

  EXPECT_FALSE(scrollable_area->ScrollbarsHidden());
  testing::RunDelayedTasks(kMockOverlayFadeOutDelay);
  EXPECT_TRUE(scrollable_area->ScrollbarsHidden());

  scrollable_area->SetScrollOffset(ScrollOffset(10, 10), kProgrammaticScroll,
                                   kScrollBehaviorInstant);

  EXPECT_FALSE(scrollable_area->ScrollbarsHidden());
  testing::RunDelayedTasks(kMockOverlayFadeOutDelay);
  EXPECT_TRUE(scrollable_area->ScrollbarsHidden());

  frame->ExecuteScript(WebScriptSource(
      "document.getElementById('space').style.height = '500px';"));
  frame->View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(scrollable_area->ScrollbarsHidden());

  frame->ExecuteScript(WebScriptSource(
      "document.getElementById('container').style.height = '300px';"));
  frame->View()->UpdateAllLifecyclePhases();

  EXPECT_FALSE(scrollable_area->ScrollbarsHidden());
  testing::RunDelayedTasks(kMockOverlayFadeOutDelay);
  EXPECT_TRUE(scrollable_area->ScrollbarsHidden());

  // Non-composited scrollbars don't fade out while mouse is over.
  EXPECT_TRUE(scrollable_area->VerticalScrollbar());
  scrollable_area->SetScrollOffset(ScrollOffset(20, 20), kProgrammaticScroll,
                                   kScrollBehaviorInstant);
  EXPECT_FALSE(scrollable_area->ScrollbarsHidden());
  scrollable_area->MouseEnteredScrollbar(*scrollable_area->VerticalScrollbar());
  testing::RunDelayedTasks(kMockOverlayFadeOutDelay);
  EXPECT_FALSE(scrollable_area->ScrollbarsHidden());
  scrollable_area->MouseExitedScrollbar(*scrollable_area->VerticalScrollbar());
  testing::RunDelayedTasks(kMockOverlayFadeOutDelay);
  EXPECT_TRUE(scrollable_area->ScrollbarsHidden());

  mock_overlay_theme.SetOverlayScrollbarFadeOutDelay(0.0);
}

typedef bool TestParamOverlayScrollbar;
class ScrollbarAppearanceTest
    : public SimTest,
      public ::testing::WithParamInterface<TestParamOverlayScrollbar> {
 public:
  // Use real scrollbars to ensure we're testing the real ScrollbarThemes.
  ScrollbarAppearanceTest() : mock_scrollbars_(false, GetParam()) {}

 private:
  FrameTestHelpers::UseMockScrollbarSettings mock_scrollbars_;
};

class StubWebThemeEngine : public WebThemeEngine {
 public:
  WebSize GetSize(Part part) override {
    switch (part) {
      case kPartScrollbarHorizontalThumb:
        return blink::WebSize(kMinimumHorizontalLength, 15);
      case kPartScrollbarVerticalThumb:
        return blink::WebSize(15, kMinimumVerticalLength);
      default:
        return WebSize();
    }
  }
  void GetOverlayScrollbarStyle(ScrollbarStyle* style) override {
    style->fade_out_delay_seconds = 0;
    style->fade_out_duration_seconds = 0;
    style->thumb_thickness = 3;
    style->scrollbar_margin = 0;
    style->color = SkColorSetARGB(128, 64, 64, 64);
  }
  static constexpr int kMinimumHorizontalLength = 51;
  static constexpr int kMinimumVerticalLength = 52;
};

constexpr int StubWebThemeEngine::kMinimumHorizontalLength;
constexpr int StubWebThemeEngine::kMinimumVerticalLength;

class ScrollbarTestingPlatformSupport : public TestingPlatformSupport {
 public:
  WebThemeEngine* ThemeEngine() override { return &stub_theme_engine_; }

 private:
  StubWebThemeEngine stub_theme_engine_;
};

// Test both overlay and non-overlay scrollbars.
INSTANTIATE_TEST_CASE_P(All, ScrollbarAppearanceTest, ::testing::Bool());

#if !defined(OS_MACOSX)
// Ensure that the minimum length for a scrollbar thumb comes from the
// WebThemeEngine. Note, Mac scrollbars differ from all other platforms so this
// test doesn't apply there. https://crbug.com/682209.
TEST_P(ScrollbarAppearanceTest, ThemeEngineDefinesMinimumThumbLength) {
  ScopedTestingPlatformSupport<ScrollbarTestingPlatformSupport> platform;

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style> body { width: 1000000px; height: 1000000px; } </style>");
  ScrollableArea* scrollable_area =
      GetDocument().View()->LayoutViewportScrollableArea();

  Compositor().BeginFrame();
  ASSERT_TRUE(scrollable_area->VerticalScrollbar());
  ASSERT_TRUE(scrollable_area->HorizontalScrollbar());

  ScrollbarTheme& theme = scrollable_area->VerticalScrollbar()->GetTheme();
  EXPECT_EQ(StubWebThemeEngine::kMinimumHorizontalLength,
            theme.ThumbLength(*scrollable_area->HorizontalScrollbar()));
  EXPECT_EQ(StubWebThemeEngine::kMinimumVerticalLength,
            theme.ThumbLength(*scrollable_area->VerticalScrollbar()));
}

// Ensure thumb position is correctly calculated even at ridiculously large
// scales.
TEST_P(ScrollbarAppearanceTest, HugeScrollingThumbPosition) {
  ScopedTestingPlatformSupport<ScrollbarTestingPlatformSupport> platform;

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(1000, 1000));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style> body { margin: 0px; height: 10000000px; } </style>");
  ScrollableArea* scrollable_area =
      GetDocument().View()->LayoutViewportScrollableArea();

  Compositor().BeginFrame();

  scrollable_area->SetScrollOffset(ScrollOffset(0, 10000000),
                                   kProgrammaticScroll);

  int scroll_y = scrollable_area->GetScrollOffset().Height();
  ASSERT_EQ(9999000, scroll_y);

  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();
  ASSERT_TRUE(scrollbar);

  int maximumThumbPosition =
      WebView().Size().height - StubWebThemeEngine::kMinimumVerticalLength;

  EXPECT_EQ(maximumThumbPosition,
            scrollbar->GetTheme().ThumbPosition(*scrollbar));
}
#endif

}  // namespace

}  // namespace blink
