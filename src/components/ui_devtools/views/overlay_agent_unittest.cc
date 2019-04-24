// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/overlay_agent_aura.h"

#include "components/ui_devtools/ui_devtools_unittest_utils.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/views/dom_agent_aura.h"
#include "components/ui_devtools/views/overlay_agent_aura.h"
#include "components/ui_devtools/views/view_element.h"
#include "components/ui_devtools/views/widget_element.h"
#include "components/ui_devtools/views/window_element.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_utils.h"
#include "ui/views/window/non_client_view.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#endif

namespace ui_devtools {

namespace {
const SkColor kBackgroundColor = 0;

gfx::Point GetOriginInScreen(views::View* view) {
  gfx::Point point(0, 0);  // Since it's local bounds, origin is always 0,0.
  views::View::ConvertPointToScreen(view, &point);
  return point;
}

}  // namespace

class OverlayAgentTest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    fake_frontend_channel_ = std::make_unique<FakeFrontendChannel>();
    uber_dispatcher_ = std::make_unique<protocol::UberDispatcher>(
        fake_frontend_channel_.get());
    aura::Env* env = aura::Env::GetInstance();
    dom_agent_ = std::make_unique<DOMAgentAura>(env);
    dom_agent_->Init(uber_dispatcher_.get());
    overlay_agent_ = std::make_unique<OverlayAgentAura>(dom_agent_.get(), env);
    overlay_agent_->Init(uber_dispatcher_.get());
    overlay_agent_->enable();
    views::ViewsTestBase::SetUp();
  }

  void TearDown() override {
    // Ensure DOMAgent shuts down before the root window closes to avoid
    // lifetime issues.
    overlay_agent_.reset();
    dom_agent_.reset();
    uber_dispatcher_.reset();
    fake_frontend_channel_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<ui::MouseEvent> MouseEventAtRootLocation(gfx::Point p) {
    auto event = std::make_unique<ui::MouseEvent>(ui::ET_MOUSE_MOVED, p, p,
                                                  ui::EventTimeForNow(),
                                                  ui::EF_NONE, ui::EF_NONE);
    ui::Event::DispatcherApi(event.get()).set_target(GetContext());
    return event;
  }
  views::View* GetViewAtPoint(int x, int y) {
    gfx::Point point(x, y);
    int element_id = overlay_agent()->FindElementIdTargetedByPoint(
        MouseEventAtRootLocation(point).get());
    UIElement* element = dom_agent()->GetElementFromNodeId(element_id);
    DCHECK_EQ(element->type(), UIElementType::VIEW);
    return UIElement::GetBackingElement<views::View, ViewElement>(element);
  }
  int GetOverlayNodeHighlightRequestedCount(int node_id) {
    return frontend_channel()->CountProtocolNotificationMessage(
        base::StringPrintf(
            "{\"method\":\"Overlay.nodeHighlightRequested\",\"params\":{"
            "\"nodeId\":%d}}",
            node_id));
  }

  int GetOverlayInspectNodeRequestedCount(int node_id) {
    return frontend_channel()->CountProtocolNotificationMessage(
        base::StringPrintf(
            "{\"method\":\"Overlay.inspectNodeRequested\",\"params\":{"
            "\"backendNodeId\":%d}}",
            node_id));
  }

  std::unique_ptr<aura::Window> CreateWindowElement(const gfx::Rect& bounds) {
    std::unique_ptr<aura::Window> window = std::make_unique<aura::Window>(
        nullptr, aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_NOT_DRAWN);
    window->SetBounds(bounds);
    GetContext()->AddChild(window.get());
    window->Show();
    return window;
  }

  std::unique_ptr<views::Widget> CreateWidget(const gfx::Rect& bounds) {
    auto widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params;
    params.delegate = nullptr;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = bounds;
    params.parent = GetContext();
    widget->Init(params);
    widget->Show();
    return widget;
  }

  std::unique_ptr<views::Widget> CreateWidget() {
    // Create a widget with default bounds.
    return CreateWidget(gfx::Rect(0, 0, 400, 400));
  }

  DOMAgent* dom_agent() { return dom_agent_.get(); }
  OverlayAgentAura* overlay_agent() { return overlay_agent_.get(); }
  FakeFrontendChannel* frontend_channel() {
    return fake_frontend_channel_.get();
  }

 private:
  std::unique_ptr<protocol::UberDispatcher> uber_dispatcher_;
  std::unique_ptr<FakeFrontendChannel> fake_frontend_channel_;
  std::unique_ptr<DOMAgentAura> dom_agent_;
  std::unique_ptr<OverlayAgentAura> overlay_agent_;
};

#if defined(USE_AURA)
TEST_F(OverlayAgentTest, FindElementIdTargetedByPointWindow) {
  //  Windows without delegates won't act as an event handler.
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window = std::make_unique<aura::Window>(
      &delegate, aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetBounds(GetContext()->bounds());
  GetContext()->AddChild(window.get());
  window->Show();

  std::unique_ptr<protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);

  int element_id = overlay_agent()->FindElementIdTargetedByPoint(
      MouseEventAtRootLocation(gfx::Point(1, 1)).get());
  UIElement* element = dom_agent()->GetElementFromNodeId(element_id);
  DCHECK_EQ(element->type(), UIElementType::WINDOW);
  aura::Window* element_window =
      UIElement::GetBackingElement<aura::Window, WindowElement>(element);
  EXPECT_EQ(element_window, window.get());

  gfx::Point out_of_bounds =
      window->bounds().bottom_right() + gfx::Vector2d(20, 20);
  EXPECT_EQ(0, overlay_agent()->FindElementIdTargetedByPoint(
                   MouseEventAtRootLocation(out_of_bounds).get()));
}
#endif

TEST_F(OverlayAgentTest, FindElementIdTargetedByPointViews) {
  std::unique_ptr<views::Widget> widget = CreateWidget();

  std::unique_ptr<protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);

  views::View* contents_view = widget->GetContentsView();
  contents_view->RemoveAllChildViews(true);

  views::View* child_1 = new views::View;
  views::View* child_2 = new views::View;

  // Not to scale!
  // ------------------------
  // | contents_view        |
  // |    ----------        |
  // |    |child_1 |------- |
  // |    |        |      | |
  // |    ----------      | |
  // |            |child_2| |
  // |            --------- |
  // |                      |
  // ------------------------
  contents_view->AddChildView(child_2);
  contents_view->AddChildView(child_1);
  child_1->SetBounds(20, 20, 100, 100);
  child_2->SetBounds(90, 50, 100, 100);

  EXPECT_EQ(GetViewAtPoint(1, 1), widget->GetContentsView());
  EXPECT_EQ(GetViewAtPoint(21, 21), child_1);
  EXPECT_EQ(GetViewAtPoint(170, 130), child_2);
  // At the overlap.
  EXPECT_EQ(GetViewAtPoint(110, 110), child_1);
}

TEST_F(OverlayAgentTest, HighlightRects) {
  const struct {
    std::string name;
    gfx::Rect first_element_bounds;
    gfx::Rect second_element_bounds;
    HighlightRectsConfiguration expected_configuration;
  } kTestCases[] = {
      {"R1_CONTAINS_R2", gfx::Rect(1, 1, 100, 100), gfx::Rect(2, 2, 50, 50),
       R1_CONTAINS_R2},
      {"R1_HORIZONTAL_FULL_LEFT_R2", gfx::Rect(1, 1, 50, 50),
       gfx::Rect(60, 1, 60, 60), R1_HORIZONTAL_FULL_LEFT_R2},
      {"R1_TOP_FULL_LEFT_R2", gfx::Rect(30, 30, 50, 50),
       gfx::Rect(100, 100, 50, 50), R1_TOP_FULL_LEFT_R2},
      {"R1_BOTTOM_FULL_LEFT_R2", gfx::Rect(100, 100, 50, 50),
       gfx::Rect(200, 50, 40, 40), R1_BOTTOM_FULL_LEFT_R2},
      {"R1_TOP_PARTIAL_LEFT_R2", gfx::Rect(100, 100, 50, 50),
       gfx::Rect(120, 200, 50, 50), R1_TOP_PARTIAL_LEFT_R2},
      {"R1_BOTTOM_PARTIAL_LEFT_R2", gfx::Rect(50, 200, 100, 100),
       gfx::Rect(100, 50, 50, 50), R1_BOTTOM_PARTIAL_LEFT_R2},
      {"R1_INTERSECTS_R2", gfx::Rect(100, 100, 50, 50),
       gfx::Rect(120, 120, 50, 50), R1_INTERSECTS_R2},
  };
  // Use a non-zero origin to test screen coordinates.
  const gfx::Rect kWidgetBounds(10, 10, 510, 510);

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Case: " << test_case.name);
    std::unique_ptr<views::Widget> widget = CreateWidget(kWidgetBounds);

    std::unique_ptr<protocol::DOM::Node> root;
    dom_agent()->getDocument(&root);

    // Fish out the client view to serve as superview. Emptying out the content
    // view and adding the subviews directly causes NonClientView's hit test to
    // fail.
    views::View* contents_view = widget->GetContentsView();
    DCHECK_EQ(contents_view->GetClassName(),
              views::NonClientView::kViewClassName);
    views::NonClientView* non_client_view =
        static_cast<views::NonClientView*>(contents_view);
    views::View* client_view = non_client_view->client_view();

    views::View* child_1 = new views::View;
    views::View* child_2 = new views::View;
    client_view->AddChildView(child_1);
    client_view->AddChildView(child_2);
    child_1->SetBoundsRect(test_case.first_element_bounds);
    child_2->SetBoundsRect(test_case.second_element_bounds);

    overlay_agent()->setInspectMode(
        "searchForNode", protocol::Maybe<protocol::Overlay::HighlightConfig>());
    ui::test::EventGenerator generator(GetRootWindow(widget.get()));

    // Highlight child 1.
    generator.MoveMouseTo(GetOriginInScreen(child_1));
    // Click to pin it.
    generator.PressLeftButton();
    // Highlight child 2. Now, the distance overlay is showing.
    generator.MoveMouseTo(GetOriginInScreen(child_2));

    // Check calculated highlight config.
    EXPECT_EQ(test_case.expected_configuration,
              overlay_agent()->highlight_rect_config());
    // Check results of pinned and hovered rectangles.
    gfx::Rect expected_pinned_rect =
        client_view->ConvertRectToParent(test_case.first_element_bounds);
    expected_pinned_rect.Offset(kWidgetBounds.OffsetFromOrigin());
    EXPECT_EQ(expected_pinned_rect, overlay_agent()->pinned_rect_);
    gfx::Rect expected_hovered_rect =
        client_view->ConvertRectToParent(test_case.second_element_bounds);
    expected_hovered_rect.Offset(kWidgetBounds.OffsetFromOrigin());
    EXPECT_EQ(expected_hovered_rect, overlay_agent()->hovered_rect_);
    // If we don't explicitly stop inspecting, we'll leave ourselves as
    // a pretarget handler for the root window and UAF in the next test.
    // TODO(lgrey): Fix this when refactoring to support Mac.
    overlay_agent()->setInspectMode(
        "none", protocol::Maybe<protocol::Overlay::HighlightConfig>());
  }
}

// Tests that the correct Overlay events are dispatched to the frontend when
// hovering and clicking over a UI element in inspect mode.
TEST_F(OverlayAgentTest, MouseEventsGenerateFEEventsInInspectMode) {
  std::unique_ptr<views::Widget> widget = CreateWidget();

  std::unique_ptr<protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);

  gfx::Point p(1, 1);
  int node_id = overlay_agent()->FindElementIdTargetedByPoint(
      MouseEventAtRootLocation(p).get());

  EXPECT_EQ(0, GetOverlayInspectNodeRequestedCount(node_id));
  EXPECT_EQ(0, GetOverlayNodeHighlightRequestedCount(node_id));
  overlay_agent()->setInspectMode(
      "searchForNode", protocol::Maybe<protocol::Overlay::HighlightConfig>());

  // Moving the mouse cursor over the widget bounds should request a node
  // highlight.
  ui::test::EventGenerator generator(GetRootWindow(widget.get()));
  generator.MoveMouseBy(p.x(), p.y());

  // 2 mouse events ET_MOUSE_ENTERED and ET_MOUSE_MOVED are generated.
  EXPECT_EQ(2, GetOverlayNodeHighlightRequestedCount(node_id));
  EXPECT_EQ(0, GetOverlayInspectNodeRequestedCount(node_id));

  // Clicking on the widget should pin that element.
  generator.PressLeftButton();

  // Pin parent node after mouse wheel moves up.
  int parent_id = dom_agent()->GetParentIdOfNodeId(node_id);
  EXPECT_NE(parent_id, overlay_agent()->pinned_id());
  generator.MoveMouseWheel(0, 1);
  EXPECT_EQ(parent_id, overlay_agent()->pinned_id());

  // Re-assign pin node.
  node_id = parent_id;

  int inspect_node_notification_count =
      GetOverlayInspectNodeRequestedCount(node_id);

  // Press escape to exit inspect mode.
  generator.PressKey(ui::KeyboardCode::VKEY_ESCAPE, ui::EventFlags::EF_NONE);

  // Upon exiting inspect mode, the element is inspected and highlighted.
  EXPECT_EQ(inspect_node_notification_count + 1,
            GetOverlayInspectNodeRequestedCount(node_id));
  ui::Layer* highlighting_layer = overlay_agent()->layer_for_highlighting();
  EXPECT_EQ(kBackgroundColor, highlighting_layer->GetTargetColor());
  EXPECT_TRUE(highlighting_layer->visible());

  int highlight_notification_count =
      GetOverlayNodeHighlightRequestedCount(node_id);
  inspect_node_notification_count =
      GetOverlayInspectNodeRequestedCount(node_id);

  // Since inspect mode is exited, a subsequent mouse move should generate no
  // nodeHighlightRequested or inspectNodeRequested events.
  generator.MoveMouseBy(p.x(), p.y());
  EXPECT_EQ(highlight_notification_count,
            GetOverlayNodeHighlightRequestedCount(node_id));
  EXPECT_EQ(inspect_node_notification_count,
            GetOverlayInspectNodeRequestedCount(node_id));
}

TEST_F(OverlayAgentTest, HighlightNonexistentNode) {
  std::unique_ptr<protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);

  const int id = 1000;
  DCHECK(dom_agent()->GetElementFromNodeId(id) == nullptr);

  overlay_agent()->highlightNode(nullptr, id);
  if (overlay_agent()->layer_for_highlighting()) {
    EXPECT_FALSE(overlay_agent()->layer_for_highlighting()->parent());
    EXPECT_FALSE(overlay_agent()->layer_for_highlighting()->visible());
  }
}

#if defined(USE_AURA)
TEST_F(OverlayAgentTest, HighlightWindow) {
  std::unique_ptr<protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);

  std::unique_ptr<aura::Window> window =
      CreateWindowElement(gfx::Rect(0, 0, 20, 20));
  int window_id =
      dom_agent()
          ->element_root()
          ->FindUIElementIdForBackendElement<aura::Window>(window.get());
  DCHECK_NE(window_id, 0);

  overlay_agent()->highlightNode(nullptr, window_id);
  ui::Layer* highlightingLayer = overlay_agent()->layer_for_highlighting();
  DCHECK(highlightingLayer);

  EXPECT_EQ(highlightingLayer->parent(), GetContext()->layer());
  EXPECT_TRUE(highlightingLayer->visible());

  overlay_agent()->hideHighlight();
  EXPECT_FALSE(highlightingLayer->visible());
}

TEST_F(OverlayAgentTest, HighlightEmptyOrInvisibleWindow) {
  std::unique_ptr<protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);

  std::unique_ptr<aura::Window> window = CreateWindowElement(gfx::Rect());
  int window_id =
      dom_agent()
          ->element_root()
          ->FindUIElementIdForBackendElement<aura::Window>(window.get());
  DCHECK_NE(window_id, 0);

  overlay_agent()->highlightNode(nullptr, window_id);
  ui::Layer* highlightingLayer = overlay_agent()->layer_for_highlighting();
  DCHECK(highlightingLayer);

  // Highlight doesn't show for empty element.
  EXPECT_FALSE(highlightingLayer->parent());
  EXPECT_FALSE(highlightingLayer->visible());

  // Make the window non-empty, the highlight shows up.
  window->SetBounds(gfx::Rect(10, 10, 50, 50));
  overlay_agent()->highlightNode(nullptr, window_id);
  EXPECT_EQ(highlightingLayer->parent(), GetContext()->layer());
  EXPECT_TRUE(highlightingLayer->visible());

  // Make the window invisible, the highlight still shows.
  window->Hide();
  overlay_agent()->highlightNode(nullptr, window_id);
  EXPECT_EQ(highlightingLayer->parent(), GetContext()->layer());
  EXPECT_TRUE(highlightingLayer->visible());
}
#endif

TEST_F(OverlayAgentTest, HighlightWidget) {
  std::unique_ptr<views::Widget> widget = CreateWidget();

  std::unique_ptr<protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);

  int widget_id =
      dom_agent()
          ->element_root()
          ->FindUIElementIdForBackendElement<views::Widget>(widget.get());
  DCHECK_NE(widget_id, 0);

  overlay_agent()->highlightNode(nullptr, widget_id);
  ui::Layer* highlightingLayer = overlay_agent()->layer_for_highlighting();
  DCHECK(highlightingLayer);

#if defined(USE_AURA)
  EXPECT_EQ(highlightingLayer->parent(), GetContext()->layer());
#else
// TODO(https://crbug.com/898280): Fix this for Mac.
#endif
  EXPECT_TRUE(highlightingLayer->visible());

  overlay_agent()->hideHighlight();
  EXPECT_FALSE(highlightingLayer->visible());
}

}  // namespace ui_devtools
