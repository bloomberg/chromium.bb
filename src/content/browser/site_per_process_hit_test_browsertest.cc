// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_per_process_browsertest.h"

#include <tuple>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/cursor_manager.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/input/touch_emulator.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/screen_info.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/mock_overscroll_observer.h"
#include "ui/display/display_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/gfx/geometry/quad_f.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/test/mock_overscroll_controller_delegate_aura.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_rewriter.h"
#endif

#if defined(OS_MACOSX)
#include "ui/base/test/scoped_preferred_scroller_style_mac.h"
#endif

#if defined(OS_ANDROID)
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/test/mock_overscroll_refresh_handler_android.h"
#endif

#if defined(OS_WIN)
#include "base/debug/stack_trace.h"
#endif

namespace content {

namespace {

class RenderWidgetHostMouseEventMonitor {
 public:
  explicit RenderWidgetHostMouseEventMonitor(RenderWidgetHost* host)
      : host_(host), event_received_(false) {
    mouse_callback_ =
        base::Bind(&RenderWidgetHostMouseEventMonitor::MouseEventCallback,
                   base::Unretained(this));
    host_->AddMouseEventCallback(mouse_callback_);
  }
  ~RenderWidgetHostMouseEventMonitor() {
    host_->RemoveMouseEventCallback(mouse_callback_);
  }
  bool EventWasReceived() const { return event_received_; }
  void ResetEventReceived() { event_received_ = false; }
  const blink::WebMouseEvent& event() const { return event_; }

 private:
  bool MouseEventCallback(const blink::WebMouseEvent& event) {
    event_received_ = true;
    event_ = event;
    return false;
  }
  RenderWidgetHost::MouseEventCallback mouse_callback_;
  RenderWidgetHost* host_;
  bool event_received_;
  blink::WebMouseEvent event_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostMouseEventMonitor);
};

class TestInputEventObserver : public RenderWidgetHost::InputEventObserver {
 public:
  explicit TestInputEventObserver(RenderWidgetHost* host) : host_(host) {
    host_->AddInputEventObserver(this);
  }

  ~TestInputEventObserver() override { host_->RemoveInputEventObserver(this); }

  bool EventWasReceived() const { return !events_received_.empty(); }
  void ResetEventsReceived() { events_received_.clear(); }
  blink::WebInputEvent::Type EventType() const {
    DCHECK(EventWasReceived());
    return events_received_.front();
  }
  const std::vector<blink::WebInputEvent::Type>& events_received() {
    return events_received_;
  }

  const blink::WebInputEvent& event() const { return *event_; }

  void OnInputEvent(const blink::WebInputEvent& event) override {
    events_received_.push_back(event.GetType());
    event_ = ui::WebInputEventTraits::Clone(event);
  };

  const std::vector<InputEventAckSource>& events_acked() {
    return events_acked_;
  }

  void OnInputEventAck(InputEventAckSource source,
                       InputEventAckState state,
                       const blink::WebInputEvent&) override {
    events_acked_.push_back(source);
  }

 private:
  RenderWidgetHost* host_;
  std::vector<blink::WebInputEvent::Type> events_received_;
  std::vector<InputEventAckSource> events_acked_;
  ui::WebScopedInputEvent event_;

  DISALLOW_COPY_AND_ASSIGN(TestInputEventObserver);
};

// |position_in_widget| is in the coord space of |rwhv|.
template <typename PointType>
void SetWebEventPositions(blink::WebPointerProperties* event,
                          const PointType& position_in_widget,
                          RenderWidgetHostViewBase* rwhv,
                          RenderWidgetHostViewBase* rwhv_root) {
  event->SetPositionInWidget(gfx::PointF(position_in_widget));
  const gfx::PointF position_in_root =
      rwhv->TransformPointToRootCoordSpaceF(event->PositionInWidget());
  const gfx::PointF point_in_screen =
      position_in_root + rwhv_root->GetViewBounds().OffsetFromOrigin();
  event->SetPositionInScreen(point_in_screen.x(), point_in_screen.y());
}

// For convenience when setting the position in the space of the root RWHV.
template <typename PointType>
void SetWebEventPositions(blink::WebPointerProperties* event,
                          const PointType& position_in_widget,
                          RenderWidgetHostViewBase* rwhv_root) {
  DCHECK(!rwhv_root->IsRenderWidgetHostViewChildFrame());
  SetWebEventPositions(event, position_in_widget, rwhv_root, rwhv_root);
}

#if defined(USE_AURA)
// |event->location()| is in the coord space of |rwhv|.
void UpdateEventRootLocation(ui::LocatedEvent* event,
                             RenderWidgetHostViewBase* rwhv,
                             RenderWidgetHostViewBase* rwhv_root) {
  const gfx::Point position_in_root =
      rwhv->TransformPointToRootCoordSpace(event->location());

  gfx::Point root_location = position_in_root;
  aura::Window::ConvertPointToTarget(
      rwhv_root->GetNativeView(), rwhv_root->GetNativeView()->GetRootWindow(),
      &root_location);

  event->set_root_location(root_location);
}

// For convenience when setting the position in the space of the root RWHV.
void UpdateEventRootLocation(ui::LocatedEvent* event,
                             RenderWidgetHostViewBase* rwhv_root) {
  DCHECK(!rwhv_root->IsRenderWidgetHostViewChildFrame());
  UpdateEventRootLocation(event, rwhv_root, rwhv_root);
}
#endif  // defined(USE_AURA)

void RouteMouseEventAndWaitUntilDispatch(
    RenderWidgetHostInputEventRouter* router,
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* expected_target,
    blink::WebMouseEvent* event) {
  InputEventAckWaiter waiter(expected_target->GetRenderWidgetHost(),
                             event->GetType());
  router->RouteMouseEvent(root_view, event, ui::LatencyInfo());
  waiter.Wait();
}

// Dispatch |event| to the specified view using browser process hit testing.
void DispatchMouseEventAndWaitUntilDispatch(
    WebContentsImpl* web_contents,
    blink::WebMouseEvent& event,
    RenderWidgetHostViewBase* location_view,
    const gfx::PointF& location,
    RenderWidgetHostViewBase* expected_target,
    const gfx::PointF& expected_location) {
  auto* router = web_contents->GetInputEventRouter();

  RenderWidgetHostMouseEventMonitor monitor(
      expected_target->GetRenderWidgetHost());
  gfx::PointF root_location =
      location_view->TransformPointToRootCoordSpaceF(location);
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  auto* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  SetWebEventPositions(&event, root_location, root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, expected_target,
                                      &event);
  EXPECT_TRUE(monitor.EventWasReceived());
  EXPECT_NEAR(expected_location.x(), monitor.event().PositionInWidget().x, 2)
      << " & original location was " << location.x() << ", " << location.y()
      << " & root_location was " << root_location.x() << ", "
      << root_location.y();
  EXPECT_NEAR(expected_location.y(), monitor.event().PositionInWidget().y, 2);
}

// Wrapper for the above method that creates a MouseDown to send.
void DispatchMouseEventAndWaitUntilDispatch(
    WebContentsImpl* web_contents,
    RenderWidgetHostViewBase* location_view,
    const gfx::PointF& location,
    RenderWidgetHostViewBase* expected_target,
    const gfx::PointF& expected_location) {
  blink::WebMouseEvent down_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  down_event.button = blink::WebPointerProperties::Button::kLeft;
  down_event.click_count = 1;
  DispatchMouseEventAndWaitUntilDispatch(web_contents, down_event,
                                         location_view, location,
                                         expected_target, expected_location);
}

// Helper function that performs a surface hittest.
void SurfaceHitTestTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  DispatchMouseEventAndWaitUntilDispatch(web_contents, rwhv_child,
                                         gfx::PointF(5, 5), rwhv_child,
                                         gfx::PointF(5, 5));

  DispatchMouseEventAndWaitUntilDispatch(
      web_contents, rwhv_root, gfx::PointF(2, 2), rwhv_root, gfx::PointF(2, 2));
}

void OverlapSurfaceHitTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_content_overlap_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  gfx::PointF parent_location = gfx::PointF(5, 5);
  parent_location =
      rwhv_child->TransformPointToRootCoordSpaceF(parent_location);
  DispatchMouseEventAndWaitUntilDispatch(
      web_contents, rwhv_child, gfx::PointF(5, 5), rwhv_root, parent_location);

  DispatchMouseEventAndWaitUntilDispatch(web_contents, rwhv_child,
                                         gfx::PointF(95, 95), rwhv_child,
                                         gfx::PointF(95, 95));
}

void NonFlatTransformedSurfaceHitTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_non_flat_transformed_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  DispatchMouseEventAndWaitUntilDispatch(web_contents, rwhv_child,
                                         gfx::PointF(5, 5), rwhv_child,
                                         gfx::PointF(5, 5));
}

void PerspectiveTransformedSurfaceHitTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_perspective_transformed_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  RenderFrameSubmissionObserver render_frame_submission_observer(web_contents);

  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  // (90, 75) hit tests into the child frame that is positioned at (50, 50).
  // Without other transformations this should result in a translated point
  // of (40, 25), but the 45 degree 3-dimensional rotation of the frame about
  // a vertical axis skews it.
  // We can't allow DispatchMouseEventAndWaitUntilDispatch to compute the
  // coordinates in the root space unless browser conversions with
  // perspective transforms are first fixed. See https://crbug.com/854257.
  DispatchMouseEventAndWaitUntilDispatch(web_contents, rwhv_root,
                                         gfx::PointF(90, 75), rwhv_child,
                                         gfx::PointF(33, 23));
}

// Helper function that performs a surface hittest in nested frame.
void NestedSurfaceHitTestTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL(
      "a.com", "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_EQ(site_url, parent_iframe_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            parent_iframe_node->current_frame_host()->GetSiteInstance());

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  GURL nested_site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(nested_site_url, nested_iframe_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            nested_iframe_node->current_frame_host()->GetSiteInstance());
  EXPECT_NE(parent_iframe_node->current_frame_host()->GetSiteInstance(),
            nested_iframe_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_nested =
      static_cast<RenderWidgetHostViewBase*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  WaitForHitTestDataOrChildSurfaceReady(
      nested_iframe_node->current_frame_host());

  DispatchMouseEventAndWaitUntilDispatch(web_contents, rwhv_nested,
                                         gfx::PointF(10, 10), rwhv_nested,
                                         gfx::PointF(10, 10));
}

void HitTestLayerSquashing(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/oopif_hit_test_layer_squashing.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  gfx::Vector2dF child_offset = rwhv_child->GetViewBounds().origin() -
                                rwhv_root->GetViewBounds().origin();
  // Send a mouse-down on #B. The main-frame should receive it.
  DispatchMouseEventAndWaitUntilDispatch(web_contents, rwhv_root,
                                         gfx::PointF(195, 11), rwhv_root,
                                         gfx::PointF(195, 11));
  // Send another event just below. The child-frame should receive it.
  DispatchMouseEventAndWaitUntilDispatch(web_contents, rwhv_root,
                                         gfx::PointF(195, 30), rwhv_child,
                                         gfx::PointF(195, 30) - child_offset);
  // Send a mouse-down on #C.
  DispatchMouseEventAndWaitUntilDispatch(web_contents, rwhv_root,
                                         gfx::PointF(35, 195), rwhv_root,
                                         gfx::PointF(35, 195));
  // Send a mouse-down to the right of #C so that it goes to the child frame.
  DispatchMouseEventAndWaitUntilDispatch(web_contents, rwhv_root,
                                         gfx::PointF(55, 195), rwhv_child,
                                         gfx::PointF(55, 195) - child_offset);
  // Send a mouse-down to the right-bottom edge of the iframe.
  DispatchMouseEventAndWaitUntilDispatch(web_contents, rwhv_root,
                                         gfx::PointF(195, 235), rwhv_child,
                                         gfx::PointF(195, 235) - child_offset);
}

void HitTestWatermark(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/oopif_hit_test_watermark.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  gfx::Vector2dF child_offset = rwhv_child->GetViewBounds().origin() -
                                rwhv_root->GetViewBounds().origin();
  const gfx::PointF child_location(100, 120);
  // Send a mouse-down at the center of the iframe. This should go to the
  // main-frame (since there's a translucent div on top of it).
  DispatchMouseEventAndWaitUntilDispatch(web_contents, rwhv_child,
                                         child_location, rwhv_root,
                                         child_location + child_offset);

  // Set 'pointer-events: none' on the div.
  EXPECT_TRUE(ExecuteScript(web_contents, "W.style.pointerEvents = 'none';"));

  // TODO(sunxd): Re-enable this test when surface layer hit test is able to
  // handle pointer-events none. See https://crbug.com/841358.
  // Dispatch another event at the same location. It should reach the oopif this
  // time.
  if (!features::IsVizHitTestingSurfaceLayerEnabled()) {
    DispatchMouseEventAndWaitUntilDispatch(
        web_contents, rwhv_child, child_location, rwhv_child, child_location);
  }
}

#if defined(USE_AURA)
void HitTestRootWindowTransform(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  // Apply transform to root window to test that we respect root window
  // transform when transforming event location.
  gfx::Transform transform;
  transform.RotateAboutXAxis(180.f);
  transform.Translate(0.f,
                      -shell->window()->GetHost()->window()->bounds().height());
  shell->window()->GetHost()->SetRootTransform(transform);

  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));
  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  DispatchMouseEventAndWaitUntilDispatch(web_contents, rwhv_child,
                                         gfx::PointF(5, 5), rwhv_child,
                                         gfx::PointF(5, 5));

  DispatchMouseEventAndWaitUntilDispatch(
      web_contents, rwhv_root, gfx::PointF(2, 2), rwhv_root, gfx::PointF(2, 2));
}
#endif  // defined(USE_AURA)

#if defined(USE_AURA)
bool ConvertJSONToPoint(const std::string& str, gfx::PointF* point) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(str);
  if (!value)
    return false;
  base::DictionaryValue* root;
  if (!value->GetAsDictionary(&root))
    return false;
  double x, y;
  if (!root->GetDouble("x", &x))
    return false;
  if (!root->GetDouble("y", &y))
    return false;
  point->set_x(x);
  point->set_y(y);
  return true;
}

bool ConvertJSONToRect(const std::string& str, gfx::Rect* rect) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(str);
  if (!value)
    return false;
  base::DictionaryValue* root;
  if (!value->GetAsDictionary(&root))
    return false;
  int x, y, width, height;
  if (!root->GetInteger("x", &x))
    return false;
  if (!root->GetInteger("y", &y))
    return false;
  if (!root->GetInteger("width", &width))
    return false;
  if (!root->GetInteger("height", &height))
    return false;
  rect->set_x(x);
  rect->set_y(y);
  rect->set_width(width);
  rect->set_height(height);
  return true;
}
#endif  // defined(USE_AURA)

// Class for intercepting SetMouseCapture messages being sent to a
// RenderWidgetHost. Note that this only works for RenderWidgetHosts that
// are attached to RenderFrameHosts, and not those for page popups, which
// use different bindings.
class SetMouseCaptureInterceptor
    : public base::RefCountedThreadSafe<SetMouseCaptureInterceptor>,
      public mojom::WidgetInputHandlerHostInterceptorForTesting {
 public:
  SetMouseCaptureInterceptor(RenderWidgetHostImpl* host)
      : msg_received_(false),
        capturing_(false),
        host_(host),
        impl_(binding().SwapImplForTesting(this)) {}

  bool Capturing() const { return capturing_; }

  void Wait() {
    DCHECK(!run_loop_);
    if (msg_received_) {
      msg_received_ = false;
      return;
    }
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
    run_loop_.reset();
    msg_received_ = false;
  }

 protected:
  // mojom::WidgetInputHandlerHostInterceptorForTesting:
  mojom::WidgetInputHandlerHost* GetForwardingInterface() override {
    return impl_;
  }
  void SetMouseCapture(bool capturing) override {
    capturing_ = capturing;
    msg_received_ = true;
    if (run_loop_)
      run_loop_->Quit();
    GetForwardingInterface()->SetMouseCapture(capturing);
  }

 private:
  friend class base::RefCountedThreadSafe<SetMouseCaptureInterceptor>;

  ~SetMouseCaptureInterceptor() override {
    binding().SwapImplForTesting(impl_);
  }

  mojo::Binding<mojom::WidgetInputHandlerHost>& binding() {
    return static_cast<InputRouterImpl*>(host_->input_router())
        ->frame_host_binding_for_testing();
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  bool msg_received_;
  bool capturing_;
  RenderWidgetHostImpl* host_;
  mojom::WidgetInputHandlerHost* impl_;

  DISALLOW_COPY_AND_ASSIGN(SetMouseCaptureInterceptor);
};

#if defined(USE_AURA)
// A class to allow intercepting and discarding of system-level mouse events
// that might otherwise cause unpredictable behaviour in tests.
class MouseEventRewriter : public ui::EventRewriter {
 public:
  MouseEventRewriter() = default;
  ~MouseEventRewriter() override {}

 private:
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* new_event) override {
    if (event.IsMouseEvent())
      return ui::EVENT_REWRITE_DISCARD;
    return ui::EVENT_REWRITE_CONTINUE;
  }

  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* new_event) override {
    NOTREACHED();
    return ui::EVENT_REWRITE_CONTINUE;
  }

  DISALLOW_COPY_AND_ASSIGN(MouseEventRewriter);
};
#endif

}  // namespace

class SitePerProcessHitTestBrowserTest
    : public testing::WithParamInterface<std::tuple<int, float>>,
      public SitePerProcessBrowserTest {
 public:
  SitePerProcessHitTestBrowserTest() {}

#if defined(USE_AURA)
  void PreRunTestOnMainThread() override {
    SitePerProcessBrowserTest::PreRunTestOnMainThread();
    // Disable system mouse events, which can interfere with tests.
    shell()->window()->GetHost()->AddEventRewriter(&event_rewriter);
  }

  void PostRunTestOnMainThread() override {
    shell()->window()->GetHost()->RemoveEventRewriter(&event_rewriter);
    SitePerProcessBrowserTest::PostRunTestOnMainThread();
  }
#endif

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTest::SetUpCommandLine(command_line);
    if (std::get<0>(GetParam()) == 1) {
      feature_list_.InitAndEnableFeature(features::kEnableVizHitTestDrawQuad);
    } else if (std::get<0>(GetParam()) == 2) {
      feature_list_.InitAndEnableFeature(
          features::kEnableVizHitTestSurfaceLayer);
    } else {
      feature_list_.InitWithFeatures({}, {features::kEnableVizHitTestDrawQuad,
                                          features::kVizDisplayCompositor});
    }
  }

  base::test::ScopedFeatureList feature_list_;
#if defined(USE_AURA)
  MouseEventRewriter event_rewriter;
#endif
};

//
// SitePerProcessHighDPIHitTestBrowserTest
//

class SitePerProcessHighDPIHitTestBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  const double kDeviceScaleFactor = 2.0;

  SitePerProcessHighDPIHitTestBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessHitTestBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", kDeviceScaleFactor));
  }
};

//
// SitePerProcessNonIntegerScaleFactorHitTestBrowserTest
//

class SitePerProcessNonIntegerScaleFactorHitTestBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  const double kDeviceScaleFactor = 1.5;

  SitePerProcessNonIntegerScaleFactorHitTestBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessHitTestBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", kDeviceScaleFactor));
  }
};

// Restrict to Aura to we can use routable MouseWheel event via
// RenderWidgetHostViewAura::OnScrollEvent().
#if defined(USE_AURA)
class SitePerProcessInternalsHitTestBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  SitePerProcessInternalsHitTestBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessHitTestBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
    // Needed to guarantee the scrollable div we're testing with is not given
    // its own compositing layer.
    command_line->AppendSwitch(switches::kDisablePreferCompositingToLCDText);
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", std::get<1>(GetParam())));
  }
};

IN_PROC_BROWSER_TEST_P(SitePerProcessInternalsHitTestBrowserTest,
                       ScrollNestedLocalNonFastScrollableDiv) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);

  GURL site_url(embedded_test_server()->GetURL(
      "b.com", "/tall_page_with_local_iframe.html"));
  NavigateFrameToURL(parent_iframe_node, site_url);

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  WaitForHitTestDataOrChildSurfaceReady(
      nested_iframe_node->current_frame_host());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  const char* get_element_location_script_fmt =
      "var rect = "
      "document.getElementById('%s').getBoundingClientRect();\n"
      "var point = {\n"
      "  x: rect.left,\n"
      "  y: rect.top\n"
      "};\n"
      "window.domAutomationController.send(JSON.stringify(point));";

  // Since the nested local b-frame shares the RenderWidgetHostViewChildFrame
  // with the parent frame, we need to query element offsets in both documents
  // before converting to root space coordinates for the wheel event.
  std::string str;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      nested_iframe_node->current_frame_host(),
      base::StringPrintf(get_element_location_script_fmt, "scrollable_div"),
      &str));
  gfx::PointF nested_point_f;
  ConvertJSONToPoint(str, &nested_point_f);

  EXPECT_TRUE(ExecuteScriptAndExtractString(
      parent_iframe_node->current_frame_host(),
      base::StringPrintf(get_element_location_script_fmt, "nested_frame"),
      &str));
  gfx::PointF parent_offset_f;
  ConvertJSONToPoint(str, &parent_offset_f);

  // Compute location for wheel event.
  gfx::PointF point_f(parent_offset_f.x() + nested_point_f.x() + 5.f,
                      parent_offset_f.y() + nested_point_f.y() + 5.f);

  RenderWidgetHostViewChildFrame* rwhv_nested =
      static_cast<RenderWidgetHostViewChildFrame*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());
  point_f = rwhv_nested->TransformPointToRootCoordSpaceF(point_f);

  RenderWidgetHostViewAura* rwhv_root = static_cast<RenderWidgetHostViewAura*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  gfx::PointF nested_in_parent;
  rwhv_root->TransformPointToCoordSpaceForView(
      point_f,
      parent_iframe_node->current_frame_host()
          ->GetRenderWidgetHost()
          ->GetView(),
      &nested_in_parent, viz::EventSource::MOUSE);

  // Get original scroll position.
  double div_scroll_top_start;
  EXPECT_TRUE(ExecuteScriptAndExtractDouble(
      nested_iframe_node->current_frame_host(),
      "window.domAutomationController.send("
      "document.getElementById('scrollable_div').scrollTop);",
      &div_scroll_top_start));
  EXPECT_EQ(0.0, div_scroll_top_start);

  // Wait until renderer's compositor thread is synced. Otherwise the non fast
  // scrollable regions won't be set when the event arrives.
  MainThreadFrameObserver observer(rwhv_nested->GetRenderWidgetHost());
  observer.Wait();

  // Send a wheel to scroll the div.
  gfx::Point location(point_f.x(), point_f.y());
  ui::ScrollEvent scroll_event(ui::ET_SCROLL, location, ui::EventTimeForNow(),
                               0, 0, -ui::MouseWheelEvent::kWheelDelta, 0,
                               ui::MouseWheelEvent::kWheelDelta,
                               2);  // This must be '2' or it gets silently
                                    // dropped.
  UpdateEventRootLocation(&scroll_event, rwhv_root);

  InputEventAckWaiter ack_observer(
      parent_iframe_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::kGestureScrollUpdate);
  rwhv_root->OnScrollEvent(&scroll_event);
  ack_observer.Wait();

  // Check compositor layers.
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      nested_iframe_node->current_frame_host(),
      "window.domAutomationController.send("
      "window.internals.layerTreeAsText(document));",
      &str));
  // We expect the nested OOPIF to not have any compositor layers.
  EXPECT_EQ(std::string(), str);

  // Verify the div scrolled.
  double div_scroll_top = div_scroll_top_start;
  EXPECT_TRUE(ExecuteScriptAndExtractDouble(
      nested_iframe_node->current_frame_host(),
      "window.domAutomationController.send("
      "document.getElementById('scrollable_div').scrollTop);",
      &div_scroll_top));
  EXPECT_NE(div_scroll_top_start, div_scroll_top);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessInternalsHitTestBrowserTest,
                       NestedLocalNonFastScrollableDivCoordsAreLocal) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);

  GURL site_url(embedded_test_server()->GetURL(
      "b.com", "/tall_page_with_local_iframe.html"));
  NavigateFrameToURL(parent_iframe_node, site_url);

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  WaitForHitTestDataOrChildSurfaceReady(
      nested_iframe_node->current_frame_host());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  const char* get_element_location_script_fmt =
      "var rect = "
      "document.getElementById('%s').getBoundingClientRect();\n"
      "var point = {\n"
      "  x: rect.left,\n"
      "  y: rect.top\n"
      "};\n"
      "window.domAutomationController.send(JSON.stringify(point));";

  // Since the nested local b-frame shares the RenderWidgetHostViewChildFrame
  // with the parent frame, we need to query element offsets in both documents
  // before converting to root space coordinates for the wheel event.
  std::string str;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      nested_iframe_node->current_frame_host(),
      base::StringPrintf(get_element_location_script_fmt, "scrollable_div"),
      &str));
  gfx::PointF nested_point_f;
  ConvertJSONToPoint(str, &nested_point_f);

  int num_non_fast_region_rects;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      parent_iframe_node->current_frame_host(),
      "window.internals.markGestureScrollRegionDirty(document);\n"
      "window.internals.forceCompositingUpdate(document);\n"
      "var rects = window.internals.nonFastScrollableRects(document);\n"
      "window.domAutomationController.send(rects.length);",
      &num_non_fast_region_rects));
  EXPECT_EQ(1, num_non_fast_region_rects);
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      parent_iframe_node->current_frame_host(),
      "var rect = {\n"
      "  x: rects[0].left,\n"
      "  y: rects[0].top,\n"
      "  width: rects[0].width,\n"
      "  height: rects[0].height\n"
      "};\n"
      "window.domAutomationController.send(JSON.stringify(rect));",
      &str));
  gfx::Rect non_fast_scrollable_rect_before_scroll;
  ConvertJSONToRect(str, &non_fast_scrollable_rect_before_scroll);

  EXPECT_TRUE(ExecuteScriptAndExtractString(
      parent_iframe_node->current_frame_host(),
      base::StringPrintf(get_element_location_script_fmt, "nested_frame"),
      &str));
  gfx::PointF parent_offset_f;
  ConvertJSONToPoint(str, &parent_offset_f);

  // Compute location for wheel event to scroll the parent with respect to the
  // mainframe.
  gfx::PointF point_f(parent_offset_f.x() + 1.f, parent_offset_f.y() + 1.f);

  RenderWidgetHostViewChildFrame* rwhv_parent =
      static_cast<RenderWidgetHostViewChildFrame*>(
          parent_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());
  point_f = rwhv_parent->TransformPointToRootCoordSpaceF(point_f);

  RenderWidgetHostViewAura* rwhv_root = static_cast<RenderWidgetHostViewAura*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  gfx::PointF nested_in_parent;
  rwhv_root->TransformPointToCoordSpaceForView(
      point_f,
      parent_iframe_node->current_frame_host()
          ->GetRenderWidgetHost()
          ->GetView(),
      &nested_in_parent);

  // Get original scroll position.
  double div_scroll_top_start;
  EXPECT_TRUE(
      ExecuteScriptAndExtractDouble(parent_iframe_node->current_frame_host(),
                                    "window.domAutomationController.send("
                                    "document.body.scrollTop);",
                                    &div_scroll_top_start));
  EXPECT_EQ(0.0, div_scroll_top_start);

  // Send a wheel to scroll the parent containing the div.
  gfx::Point location(point_f.x(), point_f.y());
  ui::ScrollEvent scroll_event(ui::ET_SCROLL, location, ui::EventTimeForNow(),
                               0, 0, -ui::MouseWheelEvent::kWheelDelta, 0,
                               ui::MouseWheelEvent::kWheelDelta,
                               2);  // This must be '2' or it gets silently
                                    // dropped.
  UpdateEventRootLocation(&scroll_event, rwhv_root);

  InputEventAckWaiter ack_observer(
      parent_iframe_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::kGestureScrollUpdate);
  rwhv_root->OnScrollEvent(&scroll_event);
  ack_observer.Wait();

  MainThreadFrameObserver thread_observer(rwhv_parent->GetRenderWidgetHost());
  thread_observer.Wait();

  // Check compositor layers.
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      nested_iframe_node->current_frame_host(),
      "window.domAutomationController.send("
      "window.internals.layerTreeAsText(document));",
      &str));
  // We expect the nested OOPIF to not have any compositor layers.
  EXPECT_EQ(std::string(), str);

  // Verify the div scrolled.
  double div_scroll_top = div_scroll_top_start;
  EXPECT_TRUE(
      ExecuteScriptAndExtractDouble(parent_iframe_node->current_frame_host(),
                                    "window.domAutomationController.send("
                                    "document.body.scrollTop);",
                                    &div_scroll_top));
  EXPECT_NE(div_scroll_top_start, div_scroll_top);

  // Verify the non-fast scrollable region rect is the same, even though the
  // parent scroll isn't.
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      parent_iframe_node->current_frame_host(),
      "window.internals.markGestureScrollRegionDirty(document);\n"
      "window.internals.forceCompositingUpdate(document);\n"
      "var rects = window.internals.nonFastScrollableRects(document);\n"
      "window.domAutomationController.send(rects.length);",
      &num_non_fast_region_rects));
  EXPECT_EQ(1, num_non_fast_region_rects);
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      parent_iframe_node->current_frame_host(),
      "var rect = {\n"
      "  x: rects[0].left,\n"
      "  y: rects[0].top,\n"
      "  width: rects[0].width,\n"
      "  height: rects[0].height\n"
      "};\n"
      "window.domAutomationController.send(JSON.stringify(rect));",
      &str));
  gfx::Rect non_fast_scrollable_rect_after_scroll;
  ConvertJSONToRect(str, &non_fast_scrollable_rect_after_scroll);
  EXPECT_EQ(non_fast_scrollable_rect_before_scroll,
            non_fast_scrollable_rect_after_scroll);
}
#endif  // defined(USE_AURA)

#if defined(OS_CHROMEOS)
// Times out flakily on Chrome OS. crbug.com/833380
#define MAYBE_CancelWheelScrollBubblingOnWheelTargetDeletion \
  DISABLED_CancelWheelScrollBubblingOnWheelTargetDeletion
#else
#define MAYBE_CancelWheelScrollBubblingOnWheelTargetDeletion \
  CancelWheelScrollBubblingOnWheelTargetDeletion
#endif

// Tests that wheel scroll bubbling gets cancelled when the wheel target view
// gets destroyed in the middle of a wheel scroll seqeunce. This happens in
// cases like overscroll navigation from inside an oopif.
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_CancelWheelScrollBubblingOnWheelTargetDeletion) {
  ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
      0);
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* iframe_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, iframe_node->current_url());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  RenderWidgetHostViewBase* child_rwhv = static_cast<RenderWidgetHostViewBase*>(
      iframe_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetInputEventRouter();

  WaitForHitTestDataOrChildSurfaceReady(iframe_node->current_frame_host());

  InputEventAckWaiter scroll_begin_observer(
      root->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::kGestureScrollBegin);
  InputEventAckWaiter scroll_end_observer(
      root->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::kGestureScrollEnd);

  // Scroll the iframe upward, scroll events get bubbled up to the root.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gfx::Rect bounds = child_rwhv->GetViewBounds();
  float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;
  gfx::Point position_in_widget(
      gfx::ToCeiledInt((bounds.x() - root_view->GetViewBounds().x() + 5) *
                       scale_factor),
      gfx::ToCeiledInt((bounds.y() - root_view->GetViewBounds().y() + 5) *
                       scale_factor));
  SetWebEventPositions(&scroll_event, position_in_widget, root_view);
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = 5.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  scroll_event.has_precise_scrolling_deltas = true;
  router->RouteMouseWheelEvent(root_view, &scroll_event, ui::LatencyInfo());
  scroll_begin_observer.Wait();

  // Now destroy the child_rwhv, scroll bubbling stops and a GSE gets sent to
  // the root_view.
  RenderProcessHost* rph =
      iframe_node->current_frame_host()->GetSiteInstance()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      rph, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(rph->Shutdown(0));
  crash_observer.Wait();
  scroll_event.delta_y = 0.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  scroll_event.dispatch_type =
      blink::WebInputEvent::DispatchType::kEventNonBlocking;
  router->RouteMouseWheelEvent(root_view, &scroll_event, ui::LatencyInfo());
  scroll_end_observer.Wait();
}

// When a scroll event is bubbled, ensure that the bubbled event's coordinates
// are correctly updated to the ancestor's coordinate space. In particular,
// ensure that the transformation considers CSS scaling of the child where
// simply applying the ancestor's offset does not produce the correct
// coordinates in the ancestor's coordinate space.
// See https://crbug.com/817392
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       BubbledScrollEventsTransformedCorrectly) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_scaled_frame.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* iframe_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, iframe_node->current_url());

  RenderWidgetHostViewBase* root_rwhv = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetInputEventRouter();

  WaitForHitTestDataOrChildSurfaceReady(iframe_node->current_frame_host());

  const float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;
  // Due to the CSS scaling of the iframe, the position in the child view's
  // coordinates is (96, 96) and not (48, 48) (or approximately these values
  // if there's rounding due to the scale factor).
  const gfx::Point position_in_root(gfx::ToCeiledInt(150 * scale_factor),
                                    gfx::ToCeiledInt(150 * scale_factor));

  auto expect_gsb_with_position = base::BindRepeating(
      [](const gfx::Point& expected_position, content::InputEventAckSource,
         content::InputEventAckState, const blink::WebInputEvent& event) {
        if (event.GetType() != blink::WebInputEvent::kGestureScrollBegin)
          return false;

        const blink::WebGestureEvent& gesture_event =
            static_cast<const blink::WebGestureEvent&>(event);
        EXPECT_NEAR(expected_position.x(), gesture_event.PositionInWidget().x,
                    1);
        EXPECT_NEAR(expected_position.y(), gesture_event.PositionInWidget().y,
                    1);
        return true;
      });

  InputEventAckWaiter root_scroll_begin_observer(
      root_rwhv->GetRenderWidgetHost(),
      base::BindRepeating(expect_gsb_with_position, position_in_root));

  // Scroll the iframe upward, scroll events get bubbled up to the root.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&scroll_event, position_in_root, root_rwhv);
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = 5.0f;
  scroll_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  scroll_event.has_precise_scrolling_deltas = true;

  router->RouteMouseWheelEvent(root_rwhv, &scroll_event, ui::LatencyInfo());

  root_scroll_begin_observer.Wait();
}

#if defined(OS_WIN)
// Add temporary TouchMove event observer to detect spurious TouchMove events
// leading to test flake.
// https://crbug.com/833380.
class EmulatedTouchTouchMoveInputObserver
    : public RenderWidgetHost::InputEventObserver {
 public:
  explicit EmulatedTouchTouchMoveInputObserver(RenderWidgetHost* host)
      : host_(host), first_touch_move_seen_(false) {
    host_->AddInputEventObserver(this);
  }
  ~EmulatedTouchTouchMoveInputObserver() override {
    host_->RemoveInputEventObserver(this);
  }

  void OnInputEvent(const blink::WebInputEvent& event) override {
    if (event.GetType() != blink::WebInputEvent::kTouchMove)
      return;

    const blink::WebTouchEvent& touch_event =
        static_cast<const blink::WebTouchEvent&>(event);
    blink::WebFloatPoint pos_in_widget =
        touch_event.touches[0].PositionInWidget();
    blink::WebFloatPoint pos_in_screen =
        touch_event.touches[0].PositionInScreen();
    LOG(ERROR) << "TouchMove seen: widget @ (" << pos_in_widget.x << ","
               << pos_in_widget.y << "), screen @ (" << pos_in_screen.x << ","
               << pos_in_screen.y << ")";

    if (first_touch_move_seen_)
      return;

    first_touch_move_seen_ = true;
    base::debug::StackTrace().Print();
  }

 private:
  RenderWidgetHost* host_;
  bool first_touch_move_seen_;
};
#endif

class SitePerProcessEmulatedTouchBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  enum TestType {
    ScrollBubbling,
    PinchGoesToMainFrame,
    TouchActionBubbling,
    ShowPressHasTouchID
  };

  ~SitePerProcessEmulatedTouchBrowserTest() override {}

  void RunTest(TestType test_type) {
    std::string url;
    if (test_type == TouchActionBubbling)
      url = "/frame_tree/page_with_pany_frame.html";
    else
      url = "/frame_tree/page_with_positioned_frame.html";
    GURL main_url(embedded_test_server()->GetURL(url));
    ASSERT_TRUE(NavigateToURL(shell(), main_url));

    // It is safe to obtain the root frame tree node here, as it doesn't change.
    FrameTreeNode* root = web_contents()->GetFrameTree()->root();
    ASSERT_EQ(1U, root->child_count());

    FrameTreeNode* iframe_node = root->child_at(0);
    GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
    EXPECT_EQ(site_url, iframe_node->current_url());

    RenderWidgetHostViewBase* root_rwhv =
        static_cast<RenderWidgetHostViewBase*>(
            root->current_frame_host()->GetRenderWidgetHost()->GetView());
    RenderWidgetHostViewBase* child_rwhv =
        static_cast<RenderWidgetHostViewBase*>(iframe_node->current_frame_host()
                                                   ->GetRenderWidgetHost()
                                                   ->GetView());

    RenderWidgetHostInputEventRouter* router =
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetInputEventRouter();

    WaitForHitTestDataOrChildSurfaceReady(iframe_node->current_frame_host());

    auto expect_gesture_with_position = base::BindRepeating(
        [](blink::WebInputEvent::Type expected_type,
           const gfx::Point& expected_position, content::InputEventAckSource,
           content::InputEventAckState, const blink::WebInputEvent& event) {
#if defined(OS_WIN)
          // Add some logging to diagnose a potential source of flake:
          // the hypothesis is that something is causing the gesture
          // stream to cancel before kGestureShowPress is generated, so
          // we'll dump the event stream that we actually see in this case.
          // https://crbug.com/833380.
          if (expected_type == blink::WebInputEvent::kGestureShowPress) {
            LOG(ERROR) << "Waiting for: kGestureShowPress: ack seen for "
                       << blink::WebInputEvent::GetName(event.GetType());
          }
#endif
          if (event.GetType() != expected_type)
            return false;

          const blink::WebGestureEvent& gesture_event =
              static_cast<const blink::WebGestureEvent&>(event);
          EXPECT_NEAR(expected_position.x(), gesture_event.PositionInWidget().x,
                      1);
          EXPECT_NEAR(expected_position.y(), gesture_event.PositionInWidget().y,
                      1);
          EXPECT_EQ(blink::kWebGestureDeviceTouchscreen,
                    gesture_event.SourceDevice());
          // We expect all gesture events to have non-zero ids otherwise they
          // can force hit-testing in RenderWidgetHostInputEventRouter even
          // when it's unnecessary.
          EXPECT_NE(0U, gesture_event.unique_touch_event_id);
          return true;
        });

    blink::WebInputEvent::Type expected_gesture_type;
    switch (test_type) {
      case ScrollBubbling:
      case TouchActionBubbling:
        expected_gesture_type = blink::WebInputEvent::kGestureScrollBegin;
        break;
      case PinchGoesToMainFrame:
        expected_gesture_type = blink::WebInputEvent::kGesturePinchBegin;
        break;
      case ShowPressHasTouchID:
        expected_gesture_type = blink::WebInputEvent::kGestureShowPress;
        break;
      default:
        ASSERT_TRUE(false);
    }

#if defined(OS_WIN)
    {
      gfx::Rect view_bounds = root_rwhv->GetViewBounds();
      LOG(ERROR) << "Root view bounds = (" << view_bounds.x() << ","
                 << view_bounds.y() << ") " << view_bounds.width() << " x "
                 << view_bounds.height();
    }
#endif

    gfx::Point position_in_child(5, 5);
    InputEventAckWaiter child_gesture_event_observer(
        child_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(expect_gesture_with_position, expected_gesture_type,
                            position_in_child));

    gfx::Point position_in_root =
        child_rwhv->TransformPointToRootCoordSpace(position_in_child);
    InputEventAckWaiter root_gesture_event_observer(
        root_rwhv->GetRenderWidgetHost(),
        base::BindRepeating(expect_gesture_with_position, expected_gesture_type,
                            position_in_root));

    // Enable touch emulation.
    auto* touch_emulator = router->GetTouchEmulator();
    ASSERT_TRUE(touch_emulator);
    touch_emulator->Enable(TouchEmulator::Mode::kEmulatingTouchFromMouse,
                           ui::GestureProviderConfigType::CURRENT_PLATFORM);

    // Create mouse events to emulate touch scroll. Since the page has no touch
    // handlers, these events will be converted into a gesture scroll sequence.
    base::TimeTicks simulated_event_time = ui::EventTimeForNow();
    base::TimeDelta simulated_event_time_delta =
        base::TimeDelta::FromMilliseconds(100);
    blink::WebMouseEvent mouse_move_event =
        SyntheticWebMouseEventBuilder::Build(blink::WebInputEvent::kMouseMove,
                                             position_in_root.x(),
                                             position_in_root.y(), 0);
    mouse_move_event.SetTimeStamp(simulated_event_time);

    int mouse_modifier = (test_type == PinchGoesToMainFrame)
                             ? blink::WebInputEvent::kShiftKey
                             : 0;
    mouse_modifier |= blink::WebInputEvent::kLeftButtonDown;
    blink::WebMouseEvent mouse_down_event =
        SyntheticWebMouseEventBuilder::Build(
            blink::WebInputEvent::kMouseDown, position_in_root.x(),
            position_in_root.y(), mouse_modifier);
    mouse_down_event.button = blink::WebMouseEvent::Button::kLeft;
    simulated_event_time += simulated_event_time_delta;
    mouse_down_event.SetTimeStamp(simulated_event_time);

    blink::WebMouseEvent mouse_drag_event =
        SyntheticWebMouseEventBuilder::Build(
            blink::WebInputEvent::kMouseMove, position_in_root.x(),
            position_in_root.y() + 20, mouse_modifier);
    simulated_event_time += simulated_event_time_delta;
    mouse_drag_event.SetTimeStamp(simulated_event_time);
    mouse_drag_event.button = blink::WebMouseEvent::Button::kLeft;

    blink::WebMouseEvent mouse_up_event = SyntheticWebMouseEventBuilder::Build(
        blink::WebInputEvent::kMouseUp, position_in_root.x(),
        position_in_root.y() + 20, mouse_modifier);
    mouse_up_event.button = blink::WebMouseEvent::Button::kLeft;
    simulated_event_time += simulated_event_time_delta;
    mouse_up_event.SetTimeStamp(simulated_event_time);

#if defined(OS_WIN)
    // Add temporary TouchMove event observer to detect spurious TouchMove
    // events leading to test flake.
    // https://crbug.com/833380.
    std::unique_ptr<EmulatedTouchTouchMoveInputObserver> touch_move_observer;
    if (test_type == ShowPressHasTouchID) {
      touch_move_observer.reset(new EmulatedTouchTouchMoveInputObserver(
          child_rwhv->GetRenderWidgetHost()));
    }
#endif

    // Send mouse events and wait for GesturePinchBegin.
    router->RouteMouseEvent(root_rwhv, &mouse_move_event, ui::LatencyInfo());
    router->RouteMouseEvent(root_rwhv, &mouse_down_event, ui::LatencyInfo());
    if (test_type == ShowPressHasTouchID) {
      // Wait for child to receive GestureShowPress. If this test fails, it
      // will either DCHECK or time out.
      child_gesture_event_observer.Wait();
#if defined(OS_WIN)
      touch_move_observer.reset();
#endif
      return;
    }
    router->RouteMouseEvent(root_rwhv, &mouse_drag_event, ui::LatencyInfo());
    router->RouteMouseEvent(root_rwhv, &mouse_up_event, ui::LatencyInfo());

    if (test_type == ScrollBubbling || test_type == TouchActionBubbling) {
      // Verify child receives GestureScrollBegin.
      child_gesture_event_observer.Wait();
    }

    // Verify the root receives the GesturePinchBegin or GestureScrollBegin,
    // depending on |test_type|.
    root_gesture_event_observer.Wait();

    // Shut down.
    touch_emulator->Disable();
  }
};

IN_PROC_BROWSER_TEST_P(SitePerProcessEmulatedTouchBrowserTest,
                       EmulatedTouchShowPressHasTouchID) {
  RunTest(ShowPressHasTouchID);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessEmulatedTouchBrowserTest,
                       EmulatedTouchScrollBubbles) {
  RunTest(ScrollBubbling);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessEmulatedTouchBrowserTest,
                       EmulatedTouchPinchGoesToMainFrame) {
  RunTest(PinchGoesToMainFrame);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessEmulatedTouchBrowserTest,
                       EmulatedGestureScrollBubbles) {
  RunTest(TouchActionBubbling);
}

#if defined(OS_ANDROID) || defined(USE_AURA)
namespace {
// This function is used in TouchActionAckTimeout and
// SubframeGestureEventRouting, which is defined either under Android or Aura.
void OnSyntheticGestureCompleted(scoped_refptr<MessageLoopRunner> runner,
                                 SyntheticGesture::Result result) {
  EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
  runner->Quit();
}

#if defined(OS_ANDROID)
void GiveItSomeTime(int t) {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromMilliseconds(t));
  run_loop.Run();
}
#endif  // defined(OS_ANDROID)

}  // namespace
#endif  // defined(OS_ANDROID) || defined(USE_AURA)

// Regression test for https://crbug.com/851644. The test passes as long as it
// doesn't crash.
// Touch action ack timeout is enabled on Android only.
// Flaky, see https://crbug.com/871062.
#if defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       DISABLED_TouchActionAckTimeout) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_janky_frame.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());
  GURL frame_url(embedded_test_server()->GetURL(
      "baz.com", "/page_with_touch_start_janking_main_thread.html"));
  auto* child_frame_host = root->child_at(0)->current_frame_host();

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewChildFrame* rwhv_child =
      static_cast<RenderWidgetHostViewChildFrame*>(
          child_frame_host->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_frame_host);

  // Compute the point so that the gesture event can target the child frame.
  const gfx::Rect root_bounds = rwhv_root->GetViewBounds();
  const gfx::Rect child_bounds = rwhv_child->GetViewBounds();
  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());
  const float page_scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;
  const gfx::PointF point_in_child(
      (child_bounds.x() - root_bounds.x() + 25) * page_scale_factor,
      (child_bounds.y() - root_bounds.y() + 25) * page_scale_factor);

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor = gfx::PointF(point_in_child.x(), point_in_child.y());
  params.distances.push_back(gfx::Vector2dF(0, -10));
  // Make this scroll slow so that the second scroll will be queued even before
  // this one ends.
  params.speed_in_pixels_s = 1000;
  std::unique_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));

  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner();
  RenderWidgetHostImpl* render_widget_host =
      root->current_frame_host()->GetRenderWidgetHost();
  render_widget_host->QueueSyntheticGesture(
      std::move(gesture), base::BindOnce(OnSyntheticGestureCompleted, runner));
  // The first gesture takes 100ms, so wait for 120ms to ensure that it has
  // finished.
  runner->Run();
  GiveItSomeTime(120);

  SyntheticSmoothScrollGestureParams params2;
  params2.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params2.anchor = gfx::PointF(point_in_child.x(), point_in_child.y());
  params2.distances.push_back(gfx::Vector2dF(0, -10));
  params2.speed_in_pixels_s = 100000;
  std::unique_ptr<SyntheticSmoothScrollGesture> gesture2(
      new SyntheticSmoothScrollGesture(params2));
  render_widget_host->QueueSyntheticGesture(
      std::move(gesture2), base::BindOnce(OnSyntheticGestureCompleted, runner));

  runner->Run();
  runner = nullptr;

  // Give enough time to make sure all gesture are flushed and handled.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(2500));
  run_loop.Run();
}
#endif  // defined(OS_ANDROID)

#if defined(USE_AURA) || defined(OS_ANDROID)

// When unconsumed scrolls in a child bubble to the root and start an
// overscroll gesture, the subsequent gesture scroll update events should be
// consumed by the root. The child should not be able to scroll during the
// overscroll gesture.
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       RootConsumesScrollDuringOverscrollGesture) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);

#if defined(USE_AURA)
  // The child must be horizontally scrollable.
  GURL child_url(embedded_test_server()->GetURL("b.com", "/wide_page.html"));
#elif defined(OS_ANDROID)
  // The child must be vertically scrollable.
  GURL child_url(embedded_test_server()->GetURL("b.com", "/tall_page.html"));
#endif
  NavigateFrameToURL(child_node, child_url);

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  RenderWidgetHostViewChildFrame* rwhv_child =
      static_cast<RenderWidgetHostViewChildFrame*>(
          child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  ASSERT_TRUE(rwhv_root->IsScrollOffsetAtTop());
  ASSERT_TRUE(rwhv_child->IsScrollOffsetAtTop());

  RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetInputEventRouter();

  {
    // Set up the RenderWidgetHostInputEventRouter to send the gesture stream
    // to the child.
    const gfx::Rect root_bounds = rwhv_root->GetViewBounds();
    const gfx::Rect child_bounds = rwhv_child->GetViewBounds();
    const float page_scale_factor =
        render_frame_submission_observer.LastRenderFrameMetadata()
            .page_scale_factor;
    const gfx::PointF point_in_child(
        (child_bounds.x() - root_bounds.x() + 10) * page_scale_factor,
        (child_bounds.y() - root_bounds.y() + 10) * page_scale_factor);
    gfx::PointF dont_care;
    ASSERT_EQ(rwhv_child->GetRenderWidgetHost(),
              router->GetRenderWidgetHostAtPoint(rwhv_root, point_in_child,
                                                 &dont_care));

    blink::WebTouchEvent touch_event(
        blink::WebInputEvent::kTouchStart, blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    touch_event.touches_length = 1;
    touch_event.touches[0].state = blink::WebTouchPoint::kStatePressed;
    SetWebEventPositions(&touch_event.touches[0], point_in_child, rwhv_root);
    touch_event.unique_touch_event_id = 1;
    InputEventAckWaiter waiter(rwhv_child->GetRenderWidgetHost(),
                               blink::WebInputEvent::kTouchStart);
    router->RouteTouchEvent(rwhv_root, &touch_event,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
    // With async hit testing, make sure the target for the initial TouchStart
    // is resolved before sending the rest of the stream.
    waiter.Wait();

    blink::WebGestureEvent gesture_event(
        blink::WebInputEvent::kGestureTapDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::kWebGestureDeviceTouchscreen);
    gesture_event.unique_touch_event_id = touch_event.unique_touch_event_id;
    router->RouteGestureEvent(rwhv_root, &gesture_event,
                              ui::LatencyInfo(ui::SourceEventType::TOUCH));
  }

#if defined(USE_AURA)
  RenderWidgetHostViewAura* rwhva =
      static_cast<RenderWidgetHostViewAura*>(rwhv_root);
  std::unique_ptr<MockOverscrollControllerDelegateAura>
      mock_overscroll_delegate =
          std::make_unique<MockOverscrollControllerDelegateAura>(rwhva);
  rwhva->overscroll_controller()->set_delegate(mock_overscroll_delegate.get());
  MockOverscrollObserver* mock_overscroll_observer =
      mock_overscroll_delegate.get();
#elif defined(OS_ANDROID)
  RenderWidgetHostViewAndroid* rwhv_android =
      static_cast<RenderWidgetHostViewAndroid*>(rwhv_root);
  std::unique_ptr<MockOverscrollRefreshHandlerAndroid> mock_overscroll_handler =
      std::make_unique<MockOverscrollRefreshHandlerAndroid>();
  rwhv_android->SetOverscrollControllerForTesting(
      mock_overscroll_handler.get());
  MockOverscrollObserver* mock_overscroll_observer =
      mock_overscroll_handler.get();
#endif  // defined(USE_AURA)

  InputEventAckWaiter gesture_begin_observer_child(
      child_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::kGestureScrollBegin);
  InputEventAckWaiter gesture_end_observer_child(
      child_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::kGestureScrollEnd);

#if defined(USE_AURA)
  const float overscroll_threshold = OverscrollConfig::GetThreshold(
      OverscrollConfig::Threshold::kStartTouchscreen);
#elif defined(OS_ANDROID)
  const float overscroll_threshold = 0.f;
#endif

  // First we need our scroll to initiate an overscroll gesture in the root
  // via unconsumed scrolls in the child.
  blink::WebGestureEvent gesture_scroll_begin(
      blink::WebGestureEvent::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::kWebGestureDeviceTouchscreen);
  gesture_scroll_begin.unique_touch_event_id = 1;
  gesture_scroll_begin.data.scroll_begin.delta_hint_units =
      blink::WebGestureEvent::ScrollUnits::kPrecisePixels;
  gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0.f;
  gesture_scroll_begin.data.scroll_begin.delta_y_hint = 0.f;
#if defined(USE_AURA)
  // For aura, we scroll horizontally to activate an overscroll navigation.
  gesture_scroll_begin.data.scroll_begin.delta_x_hint =
      overscroll_threshold + 1;
#elif defined(OS_ANDROID)
  // For android, we scroll vertically to activate pull-to-refresh.
  gesture_scroll_begin.data.scroll_begin.delta_y_hint =
      overscroll_threshold + 1;
#endif
  router->RouteGestureEvent(rwhv_root, &gesture_scroll_begin,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));

  // Make sure the child is indeed receiving the gesture stream.
  gesture_begin_observer_child.Wait();

  blink::WebGestureEvent gesture_scroll_update(
      blink::WebGestureEvent::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::kWebGestureDeviceTouchscreen);
  gesture_scroll_update.unique_touch_event_id = 1;
  gesture_scroll_update.data.scroll_update.delta_units =
      blink::WebGestureEvent::ScrollUnits::kPrecisePixels;
  gesture_scroll_update.data.scroll_update.delta_x = 0.f;
  gesture_scroll_update.data.scroll_update.delta_y = 0.f;
#if defined(USE_AURA)
  float* delta = &gesture_scroll_update.data.scroll_update.delta_x;
#elif defined(OS_ANDROID)
  float* delta = &gesture_scroll_update.data.scroll_update.delta_y;
#endif
  *delta = overscroll_threshold + 1;
  mock_overscroll_observer->Reset();
  // This will bring us into an overscroll gesture.
  router->RouteGestureEvent(rwhv_root, &gesture_scroll_update,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  // Note that in addition to verifying that we get the overscroll update, it
  // is necessary to wait before sending the next event to prevent our multiple
  // GestureScrollUpdates from being coalesced.
  mock_overscroll_observer->WaitForUpdate();

  // This scroll is in the same direction and so it will contribute to the
  // overscroll.
  *delta = 10.0f;
  mock_overscroll_observer->Reset();
  router->RouteGestureEvent(rwhv_root, &gesture_scroll_update,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  mock_overscroll_observer->WaitForUpdate();

  // Now we reverse direction. The child could scroll in this direction, but
  // since we're in an overscroll gesture, the root should consume it.
  *delta = -5.0f;
  mock_overscroll_observer->Reset();
  router->RouteGestureEvent(rwhv_root, &gesture_scroll_update,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  mock_overscroll_observer->WaitForUpdate();

  blink::WebGestureEvent gesture_scroll_end(
      blink::WebGestureEvent::kGestureScrollEnd,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::kWebGestureDeviceTouchscreen);
  gesture_scroll_end.unique_touch_event_id = 1;
  gesture_scroll_end.data.scroll_end.delta_units =
      blink::WebGestureEvent::ScrollUnits::kPrecisePixels;
  mock_overscroll_observer->Reset();
  router->RouteGestureEvent(rwhv_root, &gesture_scroll_end,
                            ui::LatencyInfo(ui::SourceEventType::TOUCH));
  mock_overscroll_observer->WaitForEnd();

  // Ensure that the method of providing the child's scroll events to the root
  // does not leave the child in an invalid state.
  gesture_end_observer_child.Wait();
}
#endif  // defined(USE_AURA) || defined(OS_ANDROID)

// Test that an ET_SCROLL event sent to an out-of-process iframe correctly
// results in a scroll. This is only handled by RenderWidgetHostViewAura
// and is needed for trackpad scrolling on Chromebooks.
#if defined(USE_AURA)

#if defined(THREAD_SANITIZER) || defined(OS_CHROMEOS)
// Flaky: https://crbug.com/833380
#define MAYBE_ScrollEventToOOPIF DISABLED_ScrollEventToOOPIF
#else
#define MAYBE_ScrollEventToOOPIF ScrollEventToOOPIF
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_ScrollEventToOOPIF) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewAura* rwhv_parent =
      static_cast<RenderWidgetHostViewAura*>(
          root->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  // Create listener for input events.
  TestInputEventObserver child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  // Send a ui::ScrollEvent that will hit test to the child frame.
  InputEventAckWaiter waiter(
      child_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::kMouseWheel);
  ui::ScrollEvent scroll_event(ui::ET_SCROLL, gfx::Point(75, 75),
                               ui::EventTimeForNow(), ui::EF_NONE, 0,
                               10,     // Offsets
                               0, 10,  // Offset ordinals
                               2);
  UpdateEventRootLocation(&scroll_event, rwhv_parent);
  rwhv_parent->OnScrollEvent(&scroll_event);
  waiter.Wait();

  // Verify that this a mouse wheel event was sent to the child frame renderer.
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  const auto& child_events = child_frame_monitor.events_received();
  EXPECT_NE(child_events.end(),
            std::find(child_events.begin(), child_events.end(),
                      blink::WebInputEvent::kMouseWheel));
}

#if defined(THREAD_SANITIZER)
// Flaky: https://crbug.com/833380
#define MAYBE_InputEventRouterWheelCoalesceTest \
  DISABLED_InputEventRouterWheelCoalesceTest
#else
#define MAYBE_InputEventRouterWheelCoalesceTest \
  InputEventRouterWheelCoalesceTest
#endif

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_InputEventRouterWheelCoalesceTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewAura* rwhv_parent =
      static_cast<RenderWidgetHostViewAura*>(
          root->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  // Create listener for input events.
  TestInputEventObserver child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());
  InputEventAckWaiter waiter(
      child_node->current_frame_host()->GetRenderWidgetHost(),
      blink::WebInputEvent::kMouseWheel);

  // Send a mouse wheel event to child.
  blink::WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&wheel_event, gfx::Point(75, 75), rwhv_parent);
  wheel_event.delta_x = 10;
  wheel_event.delta_y = 20;
  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  router->RouteMouseWheelEvent(rwhv_parent, &wheel_event, ui::LatencyInfo());

  // Send more mouse wheel events to the child. Since we are waiting for the
  // async targeting on the first event, these new mouse wheel events should
  // be coalesced properly.
  blink::WebMouseWheelEvent wheel_event1(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&wheel_event1, gfx::Point(70, 70), rwhv_parent);
  wheel_event1.delta_x = 12;
  wheel_event1.delta_y = 22;
  wheel_event1.phase = blink::WebMouseWheelEvent::kPhaseChanged;
  router->RouteMouseWheelEvent(rwhv_parent, &wheel_event1, ui::LatencyInfo());

  blink::WebMouseWheelEvent wheel_event2(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&wheel_event2, gfx::Point(65, 65), rwhv_parent);
  wheel_event2.delta_x = 14;
  wheel_event2.delta_y = 24;
  wheel_event2.phase = blink::WebMouseWheelEvent::kPhaseChanged;
  router->RouteMouseWheelEvent(rwhv_parent, &wheel_event2, ui::LatencyInfo());

  // Since we are targeting child, event dispatch should not happen
  // synchronously. Validate that the expected target does not receive the
  // event immediately.
  // When V2 surface layer hit testing is enabled, we expect to do synchronous
  // event targeting on a child under some circumstances, so we expect the event
  // immediately dispatched to the child.
  if (!features::IsVizHitTestingSurfaceLayerEnabled())
    EXPECT_FALSE(child_frame_monitor.EventWasReceived());

  waiter.Wait();
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  EXPECT_EQ(child_frame_monitor.EventType(), blink::WebInputEvent::kMouseWheel);

  // Check if the two mouse-wheel update events are coalesced correctly.
  const auto& gesture_event =
      static_cast<const blink::WebGestureEvent&>(child_frame_monitor.event());
  EXPECT_EQ(26 /* wheel_event1.delta_x + wheel_event2.delta_x */,
            gesture_event.data.scroll_update.delta_x);
  EXPECT_EQ(46 /* wheel_event1.delta_y + wheel_event2.delta_y */,
            gesture_event.data.scroll_update.delta_y);
}
#endif  // defined(USE_AURA)

// Test that mouse events are being routed to the correct RenderWidgetHostView
// based on coordinates.
#if defined(THREAD_SANITIZER) || defined(OS_CHROMEOS)
// The test times out often on TSAN bot.
// https://crbug.com/591170.
// Also times out flakily on Chrome OS. crbug.com/833380
#define MAYBE_SurfaceHitTestTest DISABLED_SurfaceHitTestTest
#else
#define MAYBE_SurfaceHitTestTest SurfaceHitTestTest
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_SurfaceHitTestTest) {
  SurfaceHitTestTestHelper(shell(), embedded_test_server());
}

// Same test as above, but runs in high-dpi mode.
// NOTE: This has to be renamed from SurfaceHitTestTest to
// HighDPISurfaceHitTestTest. Otherwise MAYBE_SurfaceHitTestTest gets #defined
// twice.
#if defined(OS_ANDROID) || defined(OS_WIN)
// High DPI browser tests are not needed on Android, and confuse some of the
// coordinate calculations. Android uses fixed device scale factor.
// Windows is disabled because of https://crbug.com/545547.
#define MAYBE_HighDPISurfaceHitTestTest DISABLED_HighDPISurfaceHitTestTest
#elif defined(THREAD_SANITIZER)
// Flaky: https://crbug.com/833380
#define MAYBE_HighDPISurfaceHitTestTest DISABLED_HighDPISurfaceHitTestTest
#else
#define MAYBE_HighDPISurfaceHitTestTest HighDPISurfaceHitTestTest
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessHighDPIHitTestBrowserTest,
                       MAYBE_HighDPISurfaceHitTestTest) {
  SurfaceHitTestTestHelper(shell(), embedded_test_server());
}

// Test that mouse events are being routed to the correct RenderWidgetHostView
// when there are nested out-of-process iframes.
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       NestedSurfaceHitTestTest) {
  NestedSurfaceHitTestTestHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHighDPIHitTestBrowserTest,
                       NestedSurfaceHitTestTest) {
  NestedSurfaceHitTestTestHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       NonFlatTransformedSurfaceHitTestTest) {
  NonFlatTransformedSurfaceHitTestHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHighDPIHitTestBrowserTest,
                       NonFlatTransformedSurfaceHitTestTest) {
  NonFlatTransformedSurfaceHitTestHelper(shell(), embedded_test_server());
}

// TODO(kenrb): Running this test on Android bots has slight discrepancies in
// transformed event coordinates when we do manual calculation of expected
// values. We can't rely on browser side transformation because it is broken
// for perspective transforms. See https://crbug.com/854247.
#if defined(OS_ANDROID)
#define MAYBE_PerspectiveTransformedSurfaceHitTestTest \
  DISABLED_PerspectiveTransformedSurfaceHitTestTest
#else
#define MAYBE_PerspectiveTransformedSurfaceHitTestTest \
  PerspectiveTransformedSurfaceHitTestTest
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_PerspectiveTransformedSurfaceHitTestTest) {
  PerspectiveTransformedSurfaceHitTestHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHighDPIHitTestBrowserTest,
                       MAYBE_PerspectiveTransformedSurfaceHitTestTest) {
  PerspectiveTransformedSurfaceHitTestHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHighDPIHitTestBrowserTest,
                       OverlapSurfaceHitTestTest) {
  OverlapSurfaceHitTestHelper(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       OverlapSurfaceHitTestTest) {
  OverlapSurfaceHitTestHelper(shell(), embedded_test_server());
}

#if defined(OS_LINUX)
// Flaky timeouts and failures: https://crbug.com/833380
#define MAYBE_HitTestLayerSquashing DISABLED_HitTestLayerSquashing
#else
#define MAYBE_HitTestLayerSquashing HitTestLayerSquashing
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_HitTestLayerSquashing) {
  HitTestLayerSquashing(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHighDPIHitTestBrowserTest,
                       MAYBE_HitTestLayerSquashing) {
  HitTestLayerSquashing(shell(), embedded_test_server());
}

#if defined(OS_LINUX)
// Flaky timeouts and failures: https://crbug.com/833380
#define MAYBE_HitTestWatermark DISABLED_HitTestWatermark
#else
#define MAYBE_HitTestWatermark HitTestWatermark
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_HitTestWatermark) {
  HitTestWatermark(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHighDPIHitTestBrowserTest,
                       MAYBE_HitTestWatermark) {
  HitTestWatermark(shell(), embedded_test_server());
}

#if defined(USE_AURA)
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       DISABLED_RootWindowTransform) {
  HitTestRootWindowTransform(shell(), embedded_test_server());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHighDPIHitTestBrowserTest,
                       DISABLED_RootWindowTransform) {
  HitTestRootWindowTransform(shell(), embedded_test_server());
}
#endif  // defined(USE_AURA)

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       HitTestStaleDataDeletedView) {
  // TODO(sunxd): Hit test regions are not submitted for overlapping surfaces,
  // causing /2 to fail outside of Viz. https::/crbug.com/846798
  if (base::FeatureList::IsEnabled(features::kEnableVizHitTestSurfaceLayer) &&
      !base::FeatureList::IsEnabled(features::kVizDisplayCompositor)) {
    return;
  }

  // Have two iframes to avoid going to short circuit path during the second
  // targeting.
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_iframes.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ASSERT_EQ(2U, root->child_count());

  FrameTreeNode* child_node1 = root->child_at(0);
  GURL site_url1(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  EXPECT_EQ(site_url1, child_node1->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node1->current_frame_host()->GetSiteInstance());

  FrameTreeNode* child_node2 = root->child_at(1);
  GURL site_url2(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url2, child_node2->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node2->current_frame_host()->GetSiteInstance());

  RenderWidgetHostImpl* root_rwh = static_cast<RenderWidgetHostImpl*>(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostViewBase* rwhv_parent =
      static_cast<RenderWidgetHostViewBase*>(root_rwh->GetView());
  RenderWidgetHostViewBase* rwhv_child2 =
      static_cast<RenderWidgetHostViewBase*>(
          child_node2->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node1->current_frame_host());
  WaitForHitTestDataOrChildSurfaceReady(child_node2->current_frame_host());

  const gfx::PointF child_location(50, 50);
  gfx::PointF parent_location =
      rwhv_child2->TransformPointToRootCoordSpaceF(child_location);
  // Send a mouse-down at the center of the child2. This should go to the
  // child2.
  DispatchMouseEventAndWaitUntilDispatch(
      web_contents, rwhv_parent, parent_location, rwhv_child2, child_location);

  // Remove the iframe from the page. Add an infinite loop at the end so that
  // renderer wouldn't submit updated hit-test data.
  FrameDeletedObserver delete_observer(child_node2->current_frame_host());
  ExecuteScriptAsync(
      root,
      "document.body.removeChild(document.getElementsByName('frame2')[0]);"
      "while(true) {}");
  delete_observer.Wait();
  EXPECT_EQ(1U, root->child_count());

  // The synchronous targeting for the same location should now find the
  // root-view as the target (and require async-targeting), since child2 has
  // been removed. We cannot actually attempt to dispatch the event though,
  // since it would try to do asynchronous targeting by asking the root-view,
  // whose main-thread is blocked because of the infinite-loop in the injected
  // javascript above.
  blink::WebMouseEvent down_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  down_event.button = blink::WebPointerProperties::Button::kLeft;
  down_event.click_count = 1;
  SetWebEventPositions(&down_event, parent_location, rwhv_parent);
  auto result = web_contents->GetInputEventRouter()->FindTargetSynchronously(
      rwhv_parent, down_event);
  EXPECT_EQ(result.view, rwhv_parent);
  EXPECT_TRUE(result.should_query_view);
  EXPECT_EQ(result.target_location.value(), parent_location);
}

#if defined(THREAD_SANITIZER) || defined(OS_LINUX)
// Flaky: https://crbug.com/833380
#define MAYBE_SurfaceHitTestPointerEventsNone \
  DISABLED_SurfaceHitTestPointerEventsNone
#else
#define MAYBE_SurfaceHitTestPointerEventsNone SurfaceHitTestPointerEventsNone
#endif

// This test tests that browser process hittesting ignores frames with
// pointer-events: none.
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_SurfaceHitTestPointerEventsNone) {
  // TODO(sunxd): Fix pointer-events none for surface layer viz hit testing. See
  // https://crbug.com/841358.
  if (features::IsVizHitTestingSurfaceLayerEnabled()) {
    LOG(INFO) << "Skipping test due to https://crbug.com/841358";
    return;
  }
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame_pointer-events_none.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  // Target input event to child frame.
  blink::WebMouseEvent child_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  child_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&child_event, gfx::Point(75, 75), root_view);
  child_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  InputEventAckWaiter waiter(root->current_frame_host()->GetRenderWidgetHost(),
                             blink::WebInputEvent::kMouseDown);
  router->RouteMouseEvent(root_view, &child_event, ui::LatencyInfo());
  waiter.Wait();

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_NEAR(75, main_frame_monitor.event().PositionInWidget().x, 2);
  EXPECT_NEAR(75, main_frame_monitor.event().PositionInWidget().y, 2);
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());
}

#if defined(OS_CHROMEOS)
// Flaky on Chrome OS. crbug.com/833380
#define MAYBE_AsynchronousHitTestChildTimeout \
  DISABLED_AsynchronousHitTestChildTimeout
#else
#define MAYBE_AsynchronousHitTestChildTimeout AsynchronousHitTestChildTimeout
#endif

// Verify that an event is properly retargeted to the main frame when an
// asynchronous hit test to the child frame times out.
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_AsynchronousHitTestChildTimeout) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_busy_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  // Shorten the timeout for purposes of this test.
  router->GetRenderWidgetTargeterForTests()
      ->set_async_hit_test_timeout_delay_for_testing(
          TestTimeouts::tiny_timeout());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  // Target input event to child frame. It should get delivered to the main
  // frame instead because the child frame main thread is non-responsive.
  blink::WebMouseEvent child_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  child_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&child_event, gfx::Point(75, 75), root_view);
  child_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &child_event);

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_NEAR(75, main_frame_monitor.event().PositionInWidget().x, 2);
  EXPECT_NEAR(75, main_frame_monitor.event().PositionInWidget().y, 2);
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());
}

// Tooltips aren't used on Android, so no need to compile/run this test in that
// case.
#if !defined(OS_ANDROID)
class TooltipMonitor : public CursorManager::TooltipObserver {
 public:
  TooltipMonitor(CursorManager* cursor_manager) : run_loop_(new base::RunLoop) {
    DCHECK(cursor_manager);
    cursor_manager->SetTooltipObserverForTesting(this);
  }
  ~TooltipMonitor() override {}

  void Reset() {
    run_loop_.reset(new base::RunLoop);
    tooltips_received_.clear();
  }

  void OnSetTooltipTextForView(const RenderWidgetHostViewBase* view,
                               const base::string16& tooltip_text) override {
    tooltips_received_.push_back(tooltip_text);
    if (tooltip_text == tooltip_text_wanted_ && run_loop_->running())
      run_loop_->Quit();
  }

  void WaitUntil(const base::string16& tooltip_text) {
    tooltip_text_wanted_ = tooltip_text;
    auto it = std::find(tooltips_received_.begin(), tooltips_received_.end(),
                        tooltip_text);
    if (it != tooltips_received_.end())
      return;
    run_loop_->Run();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  base::string16 tooltip_text_wanted_;
  std::vector<base::string16> tooltips_received_;

  DISALLOW_COPY_AND_ASSIGN(TooltipMonitor);
};  // class TooltipMonitor

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       CrossProcessTooltipTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* b_node = root->child_at(0);

  RenderWidgetHostViewBase* rwhv_a = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_b = static_cast<RenderWidgetHostViewBase*>(
      b_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  DCHECK(rwhv_a->GetCursorManager());

  TooltipMonitor tooltip_monitor(rwhv_a->GetCursorManager());

  WaitForHitTestDataOrChildSurfaceReady(b_node->current_frame_host());

  // Make sure the point_in_a_frame value is outside the default 8px margin
  // for the body element.
  gfx::Point point_in_a_frame(10, 10);
  gfx::Point point_in_b_frame =
      rwhv_b->TransformPointToRootCoordSpace(gfx::Point(25, 25));

  // Create listeners for mouse events. These are used to verify that the
  // RenderWidgetHostInputEventRouter is generating MouseLeave, etc for
  // the right renderers.
  RenderWidgetHostMouseEventMonitor a_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor b_frame_monitor(
      b_node->current_frame_host()->GetRenderWidgetHost());

  // Add tooltip text to both the body and the iframe in A.
  std::string script =
      "body = document.body.setAttribute('title', 'body_tooltip');\n"
      "iframe = document.getElementsByTagName('iframe')[0];\n"
      "iframe.setAttribute('title','iframe_for_b');";
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(), script));

  // Send mouse events to both A and B.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseMove, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  auto* router = web_contents()->GetInputEventRouter();

  // Alternate mouse moves between main frame and the cross-process iframe to
  // test that the tool tip in the iframe can override the one set by the main
  // frame renderer, even on a second entry into the iframe.
  gfx::Point current_point;
  for (int iteration = 0; iteration < 2; ++iteration) {
    // The following is a bit of a hack to prevent hitting the same
    // position/node check in ChromeClient::SetToolTip().
    current_point = point_in_a_frame;
    current_point.Offset(iteration, iteration);
    SetWebEventPositions(&mouse_event, current_point, rwhv_a);
    RouteMouseEventAndWaitUntilDispatch(router, rwhv_a, rwhv_a, &mouse_event);
    EXPECT_TRUE(a_frame_monitor.EventWasReceived());
    a_frame_monitor.ResetEventReceived();
    // B will receive a mouseLeave on all but the first iteration.
    EXPECT_EQ(iteration != 0, b_frame_monitor.EventWasReceived());
    b_frame_monitor.ResetEventReceived();

    tooltip_monitor.WaitUntil(base::UTF8ToUTF16("body_tooltip"));
    tooltip_monitor.Reset();

    // Next send a MouseMove to B frame, and A should receive a MouseMove event.
    current_point = point_in_b_frame;
    current_point.Offset(iteration, iteration);
    SetWebEventPositions(&mouse_event, current_point, rwhv_a);
    RouteMouseEventAndWaitUntilDispatch(router, rwhv_a, rwhv_b, &mouse_event);
    EXPECT_TRUE(a_frame_monitor.EventWasReceived());
    EXPECT_EQ(a_frame_monitor.event().GetType(),
              blink::WebInputEvent::kMouseMove);
    a_frame_monitor.ResetEventReceived();
    EXPECT_TRUE(b_frame_monitor.EventWasReceived());
    b_frame_monitor.ResetEventReceived();
    tooltip_monitor.WaitUntil(base::string16());
    tooltip_monitor.Reset();
  }

  rwhv_a->GetCursorManager()->SetTooltipObserverForTesting(nullptr);
}
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
// The following test ensures that we don't get a crash if a tooltip is
// triggered on Android. This test is nearly identical to
// SitePerProcessHitTestBrowserTest.CrossProcessTooltipTestAndroid, except
// it omits the tooltip monitor, and all dereferences of GetCursorManager().
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       CrossProcessTooltipTestAndroid) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* b_node = root->child_at(0);

  RenderWidgetHostViewBase* rwhv_a = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_b = static_cast<RenderWidgetHostViewBase*>(
      b_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  // On Android we don't expect GetCursorManager() to return anything other
  // than nullptr. If it did, this test would be unnecessary.
  DCHECK(!rwhv_a->GetCursorManager());

  WaitForHitTestDataOrChildSurfaceReady(b_node->current_frame_host());

  // Make sure the point_in_a_frame value is outside the default 8px margin
  // for the body element.
  gfx::Point point_in_a_frame(10, 10);
  gfx::Point point_in_b_frame =
      rwhv_b->TransformPointToRootCoordSpace(gfx::Point(25, 25));

  // Create listeners for mouse events. These are used to verify that the
  // RenderWidgetHostInputEventRouter is generating MouseLeave, etc for
  // the right renderers.
  RenderWidgetHostMouseEventMonitor a_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor b_frame_monitor(
      b_node->current_frame_host()->GetRenderWidgetHost());

  // Add tooltip text to both the body and the iframe in A.
  std::string script_a =
      "body = document.body.setAttribute('title', 'body_a_tooltip');\n"
      "iframe = document.getElementsByTagName('iframe')[0];\n"
      "iframe.setAttribute('title','iframe_for_b');";
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(), script_a));
  std::string script_b =
      "body = document.body.setAttribute('title', 'body_b_tooltip');";
  EXPECT_TRUE(ExecuteScript(b_node->current_frame_host(), script_b));

  // Send mouse events to both A and B.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseMove, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  auto* router = web_contents()->GetInputEventRouter();

  // Alternate mouse moves between main frame and the cross-process iframe to
  // test that the tool tip in the iframe can override the one set by the main
  // frame renderer, even on a second entry into the iframe.
  gfx::Point current_point;
  for (int iteration = 0; iteration < 2; ++iteration) {
    // The following is a bit of a hack to prevent hitting the same
    // position/node check in ChromeClient::SetToolTip().
    current_point = point_in_a_frame;
    current_point.Offset(iteration, iteration);
    SetWebEventPositions(&mouse_event, current_point, rwhv_a);
    RouteMouseEventAndWaitUntilDispatch(router, rwhv_a, rwhv_a, &mouse_event);
    EXPECT_TRUE(a_frame_monitor.EventWasReceived());
    a_frame_monitor.ResetEventReceived();
    // B will receive a mouseLeave on all but the first iteration.
    EXPECT_EQ(iteration != 0, b_frame_monitor.EventWasReceived());
    b_frame_monitor.ResetEventReceived();

    // Next send a MouseMove to B frame, and A should receive a MouseMove event.
    current_point = point_in_b_frame;
    current_point.Offset(iteration, iteration);
    SetWebEventPositions(&mouse_event, current_point, rwhv_a);
    RouteMouseEventAndWaitUntilDispatch(router, rwhv_a, rwhv_b, &mouse_event);
    EXPECT_TRUE(a_frame_monitor.EventWasReceived());
    EXPECT_EQ(a_frame_monitor.event().GetType(),
              blink::WebInputEvent::kMouseMove);
    a_frame_monitor.ResetEventReceived();
    EXPECT_TRUE(b_frame_monitor.EventWasReceived());
    b_frame_monitor.ResetEventReceived();
  }

  // This is an (arbitrary) delay to allow the test to crash if it's going to.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_max_timeout());
  run_loop.Run();
}
#endif  // defined(OS_ANDROID)

// This test verifies that MouseEnter and MouseLeave events fire correctly
// when the mouse cursor moves between processes.
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       CrossProcessMouseEnterAndLeaveTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c(d))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   |--Site B ------- proxies for A C D\n"
      "   +--Site C ------- proxies for A B D\n"
      "        +--Site D -- proxies for A B C\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/\n"
      "      D = http://d.com/",
      DepictFrameTree(root));

  FrameTreeNode* b_node = root->child_at(0);
  FrameTreeNode* c_node = root->child_at(1);
  FrameTreeNode* d_node = c_node->child_at(0);

  RenderWidgetHostViewBase* rwhv_a = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_b = static_cast<RenderWidgetHostViewBase*>(
      b_node->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_d = static_cast<RenderWidgetHostViewBase*>(
      d_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  // Verifying surfaces are ready in B and D are sufficient, since other
  // surfaces contain at least one of them.
  WaitForHitTestDataOrChildSurfaceReady(b_node->current_frame_host());
  WaitForHitTestDataOrChildSurfaceReady(d_node->current_frame_host());

  // Create listeners for mouse events. These are used to verify that the
  // RenderWidgetHostInputEventRouter is generating MouseLeave, etc for
  // the right renderers.
  RenderWidgetHostMouseEventMonitor root_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor a_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor b_frame_monitor(
      b_node->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor c_frame_monitor(
      c_node->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor d_frame_monitor(
      d_node->current_frame_host()->GetRenderWidgetHost());

  float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;

  // Get the view bounds of the child iframe, which should account for the
  // relative offset of its direct parent within the root frame, for use in
  // targeting the input event.
  gfx::Rect a_bounds = rwhv_a->GetViewBounds();
  gfx::Rect b_bounds = rwhv_b->GetViewBounds();
  gfx::Rect d_bounds = rwhv_d->GetViewBounds();

  gfx::Point point_in_a_frame(2, 2);
  gfx::Point point_in_b_frame(
      gfx::ToCeiledInt((b_bounds.x() - a_bounds.x() + 25) * scale_factor),
      gfx::ToCeiledInt((b_bounds.y() - a_bounds.y() + 25) * scale_factor));
  gfx::Point point_in_d_frame(
      gfx::ToCeiledInt((d_bounds.x() - a_bounds.x() + 25) * scale_factor),
      gfx::ToCeiledInt((d_bounds.y() - a_bounds.y() + 25) * scale_factor));

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseMove, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&mouse_event, point_in_a_frame, rwhv_a);

  // Send an initial MouseMove to the root view, which shouldn't affect the
  // other renderers.
  web_contents()->GetInputEventRouter()->RouteMouseEvent(rwhv_a, &mouse_event,
                                                         ui::LatencyInfo());
  EXPECT_TRUE(a_frame_monitor.EventWasReceived());
  a_frame_monitor.ResetEventReceived();
  EXPECT_FALSE(b_frame_monitor.EventWasReceived());
  EXPECT_FALSE(c_frame_monitor.EventWasReceived());
  EXPECT_FALSE(d_frame_monitor.EventWasReceived());

  // Next send a MouseMove to B frame, which shouldn't affect C or D but
  // A should receive a MouseMove event.
  SetWebEventPositions(&mouse_event, point_in_b_frame, rwhv_a);
  auto* router = web_contents()->GetInputEventRouter();
  RouteMouseEventAndWaitUntilDispatch(router, rwhv_a, rwhv_b, &mouse_event);
  EXPECT_TRUE(a_frame_monitor.EventWasReceived());
  EXPECT_EQ(a_frame_monitor.event().GetType(),
            blink::WebInputEvent::kMouseMove);
  a_frame_monitor.ResetEventReceived();
  EXPECT_TRUE(b_frame_monitor.EventWasReceived());
  b_frame_monitor.ResetEventReceived();
  EXPECT_FALSE(c_frame_monitor.EventWasReceived());
  EXPECT_FALSE(d_frame_monitor.EventWasReceived());

  // Next send a MouseMove to D frame, which should have side effects in every
  // other RenderWidgetHostView.
  SetWebEventPositions(&mouse_event, point_in_d_frame, rwhv_a);
  RouteMouseEventAndWaitUntilDispatch(router, rwhv_a, rwhv_d, &mouse_event);
  EXPECT_TRUE(a_frame_monitor.EventWasReceived());
  EXPECT_EQ(a_frame_monitor.event().GetType(),
            blink::WebInputEvent::kMouseMove);
  EXPECT_TRUE(b_frame_monitor.EventWasReceived());
  EXPECT_EQ(b_frame_monitor.event().GetType(),
            blink::WebInputEvent::kMouseLeave);
  EXPECT_TRUE(c_frame_monitor.EventWasReceived());
  EXPECT_EQ(c_frame_monitor.event().GetType(),
            blink::WebInputEvent::kMouseMove);
  EXPECT_TRUE(d_frame_monitor.EventWasReceived());
}

// Verify that mouse capture works on a RenderWidgetHostView level, so that
// dragging scroll bars and selecting text continues even when the mouse
// cursor crosses over cross-process frame boundaries.
// TODO(kenrb): This currently only works for scrollbar dragging.
// Other reasons for a node to capture mouse input need to be addressed. See
// https://crbug.com/647378.
#if defined(OS_CHROMEOS)
// TODO: Flaky on Chrome OS. crbug.com/868409
#define MAYBE_CrossProcessMouseCapture DISABLED_CrossProcessMouseCapture
#else
#define MAYBE_CrossProcessMouseCapture CrossProcessMouseCapture
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_CrossProcessMouseCapture) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_large_scrollable_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  ASSERT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;

  // Get the view bounds of the child iframe, which should account for the
  // relative offset of its direct parent within the root frame, for use in
  // targeting the input event.
  gfx::Rect bounds = rwhv_child->GetViewBounds();
  int child_frame_target_x = gfx::ToCeiledInt(
      (bounds.x() - root_view->GetViewBounds().x() + 5) * scale_factor);
  int child_frame_target_y = gfx::ToCeiledInt(
      (bounds.y() - root_view->GetViewBounds().y() + 5) * scale_factor);

  // Target MouseDown to child frame.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&mouse_event,
                       gfx::Point(child_frame_target_x, child_frame_target_y),
                       root_view);
  mouse_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, root_view, rwhv_child,
                                      &mouse_event);

  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());

  // Target MouseMove to main frame. This should be routed to the main frame
  // because the child frame is not capturing input.
  mouse_event.SetType(blink::WebInputEvent::kMouseMove);
  mouse_event.SetModifiers(blink::WebInputEvent::kLeftButtonDown);
  SetWebEventPositions(&mouse_event, gfx::Point(1, 1), root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &mouse_event);

  // Dispatch twice because the router generates an extra MouseLeave for the
  // child frame.
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &mouse_event);

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());

// Targeting a scrollbar with a click doesn't work on Mac or Android.
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  scoped_refptr<SetMouseCaptureInterceptor> interceptor =
      new SetMouseCaptureInterceptor(static_cast<RenderWidgetHostImpl*>(
          root->current_frame_host()->GetRenderWidgetHost()));

  // Now send a MouseDown to target the thumb part of the scroll bar, which
  // should initiate mouse capture for the main frame.
  mouse_event.SetType(blink::WebInputEvent::kMouseDown);
  SetWebEventPositions(&mouse_event, gfx::Point(100, 25), root_view);
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());

  // Wait for the mouse capture message.
  interceptor->Wait();
  EXPECT_TRUE(interceptor->Capturing());

  // Yield the thread, in order to let the capture message be processed by its
  // actual handler.
  {
    base::RunLoop loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  loop.QuitClosure());
    loop.Run();
  }

  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();

  // Now that the main frame is capturing, a MouseMove targeted to the child
  // frame should be received by the main frame.
  mouse_event.SetType(blink::WebInputEvent::kMouseMove);
  SetWebEventPositions(&mouse_event,
                       gfx::Point(child_frame_target_x, child_frame_target_y),
                       root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &mouse_event);
  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();

  // A MouseUp sent anywhere should cancel the mouse capture.
  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  SetWebEventPositions(&mouse_event,
                       gfx::Point(child_frame_target_x, child_frame_target_y),
                       root_view);
  RouteMouseEventAndWaitUntilDispatch(router, root_view, root_view,
                                      &mouse_event);

  interceptor->Wait();
  EXPECT_FALSE(interceptor->Capturing());
#endif  // !defined(OS_MACOSX) && !defined(OS_ANDROID)
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MouseCaptureOnDragSelection) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  ASSERT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  // Target MouseDown to child frame.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event,
                                         rwhv_child, gfx::PointF(15.0, 5.0),
                                         rwhv_child, gfx::PointF(15.0, 5.0));

  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();

  scoped_refptr<SetMouseCaptureInterceptor> interceptor =
      new SetMouseCaptureInterceptor(static_cast<RenderWidgetHostImpl*>(
          child_node->current_frame_host()->GetRenderWidgetHost()));

  // Target MouseMove to child frame to start drag. This should cause the
  // child to start capturing mouse input.
  mouse_event.SetType(blink::WebInputEvent::kMouseMove);
  mouse_event.SetModifiers(blink::WebInputEvent::kLeftButtonDown);
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event,
                                         rwhv_child, gfx::PointF(5.0, 5.0),
                                         rwhv_child, gfx::PointF(5.0, 5.0));

  // Dispatch twice because the router generates an extra MouseLeave for the
  // main frame.
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event,
                                         rwhv_child, gfx::PointF(5.0, 5.0),
                                         rwhv_child, gfx::PointF(5.0, 5.0));

  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();

  // Wait for the mouse capture message.
  interceptor->Wait();
  EXPECT_TRUE(interceptor->Capturing());
  // Yield the thread, in order to let the capture message be processed by its
  // actual handler.
  {
    base::RunLoop loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  loop.QuitClosure());
    loop.Run();
  }

  // Now that the child frame is capturing, a MouseMove targeted to the main
  // frame should be received by the child frame.
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event,
                                         rwhv_child, gfx::PointF(-25.0, -25.0),
                                         rwhv_child, gfx::PointF(-25.0, -25.0));
  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();

  // A MouseUp sent anywhere should cancel the mouse capture.
  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  mouse_event.SetModifiers(0);
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), mouse_event,
                                         rwhv_child, gfx::PointF(-25.0, -25.0),
                                         rwhv_child, gfx::PointF(-25.0, -25.0));

  interceptor->Wait();
  EXPECT_FALSE(interceptor->Capturing());
}

// There are no cursors on Android.
#if !defined(OS_ANDROID)
class CursorMessageFilter : public content::BrowserMessageFilter {
 public:
  CursorMessageFilter()
      : content::BrowserMessageFilter(ViewMsgStart),
        message_loop_runner_(new content::MessageLoopRunner),
        last_set_cursor_routing_id_(MSG_ROUTING_NONE) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    if (message.type() == ViewHostMsg_SetCursor::ID) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::BindOnce(&CursorMessageFilter::OnSetCursor, this,
                         message.routing_id()));
    }
    return false;
  }

  void OnSetCursor(int routing_id) {
    last_set_cursor_routing_id_ = routing_id;
    message_loop_runner_->Quit();
  }

  int last_set_cursor_routing_id() const { return last_set_cursor_routing_id_; }

  void Wait() {
    // Do not reset the cursor, as the cursor may already have been set (and
    // Quit() already called on |message_loop_runner_|).
    message_loop_runner_->Run();
  }

 private:
  ~CursorMessageFilter() override {}

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  int last_set_cursor_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(CursorMessageFilter);
};

namespace {

// Verify that we receive a mouse cursor update message when we mouse over
// a text field contained in an out-of-process iframe.
void CursorUpdateReceivedFromCrossSiteIframeHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));

  auto* web_contents = static_cast<WebContentsImpl*>(shell->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();

  FrameTreeNode* child_node = root->child_at(0);
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  scoped_refptr<CursorMessageFilter> filter = new CursorMessageFilter();
  child_node->current_frame_host()->GetProcess()->AddFilter(filter.get());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHost* rwh_child =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
  RenderWidgetHostViewBase* child_view =
      static_cast<RenderWidgetHostViewBase*>(rwh_child->GetView());

  // This should only return nullptr on Android.
  EXPECT_TRUE(root_view->GetCursorManager());

  WebCursor cursor;
  EXPECT_FALSE(
      root_view->GetCursorManager()->GetCursorForTesting(root_view, cursor));
  EXPECT_FALSE(
      root_view->GetCursorManager()->GetCursorForTesting(child_view, cursor));

  // Send a MouseMove to the subframe. The frame contains text, and moving the
  // mouse over it should cause the renderer to send a mouse cursor update.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseMove, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  SetWebEventPositions(&mouse_event, gfx::Point(60, 60), root_view);
  auto* router = web_contents->GetInputEventRouter();
  RenderWidgetHostMouseEventMonitor child_monitor(
      child_view->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor root_monitor(
      root_view->GetRenderWidgetHost());
  RouteMouseEventAndWaitUntilDispatch(router, root_view, child_view,
                                      &mouse_event);
  // The child_view should receive a mouse-move event.
  EXPECT_TRUE(child_monitor.EventWasReceived());
  EXPECT_EQ(blink::WebInputEvent::kMouseMove, child_monitor.event().GetType());
  EXPECT_NEAR(10, child_monitor.event().PositionInWidget().x, 2);
  EXPECT_NEAR(10, child_monitor.event().PositionInWidget().y, 2);

  // The root_view should also receive a mouse-move event.
  EXPECT_TRUE(root_monitor.EventWasReceived());
  EXPECT_EQ(blink::WebInputEvent::kMouseMove, root_monitor.event().GetType());
  EXPECT_EQ(60, root_monitor.event().PositionInWidget().x);
  EXPECT_EQ(60, root_monitor.event().PositionInWidget().y);

  // CursorMessageFilter::Wait() implicitly tests whether we receive a
  // ViewHostMsg_SetCursor message from the renderer process, because it does
  // does not return otherwise.
  filter->Wait();
  EXPECT_EQ(filter->last_set_cursor_routing_id(), rwh_child->GetRoutingID());

  // Yield to ensure that the SetCursor message is processed by its real
  // handler.
  {
    base::RunLoop loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  loop.QuitClosure());
    loop.Run();
  }

  // The root_view receives a mouse-move event on top of the iframe, which does
  // not send a cursor update.
  EXPECT_FALSE(
      root_view->GetCursorManager()->GetCursorForTesting(root_view, cursor));
  EXPECT_TRUE(
      root_view->GetCursorManager()->GetCursorForTesting(child_view, cursor));
  // Since this moused over a text box, this should not be the default cursor.
  CursorInfo cursor_info;
  cursor.GetCursorInfo(&cursor_info);
  EXPECT_EQ(cursor_info.type, blink::WebCursorInfo::kTypeIBeam);
}

}  // namespace

// Flaky: https://crbug.com/833380
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       DISABLED_CursorUpdateReceivedFromCrossSiteIframe) {
  CursorUpdateReceivedFromCrossSiteIframeHelper(shell(),
                                                embedded_test_server());
}

// Flaky: https://crbug.com/833380
IN_PROC_BROWSER_TEST_P(SitePerProcessHighDPIHitTestBrowserTest,
                       DISABLED_CursorUpdateReceivedFromCrossSiteIframe) {
  CursorUpdateReceivedFromCrossSiteIframeHelper(shell(),
                                                embedded_test_server());
}
#endif  // !defined(OS_ANDROID)

#if defined(USE_AURA)
// Browser process hit testing is not implemented on Android, and these tests
// require Aura for RenderWidgetHostViewAura::OnTouchEvent().
// https://crbug.com/491334

// Ensure that scroll events can be cancelled with a wheel handler.
// https://crbug.com/698195

class SitePerProcessMouseWheelHitTestBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  SitePerProcessMouseWheelHitTestBrowserTest() : rwhv_root_(nullptr) {}

  void SetupWheelAndScrollHandlers(content::RenderFrameHostImpl* rfh) {
    // Set up event handlers. The wheel event handler calls prevent default on
    // alternate events, so only every other wheel generates a scroll. The fact
    // that any scroll events fire is dependent on the event going to the main
    // thread, which requires the nonFastScrollableRegion be set correctly
    // on the compositor.
    std::string script =
        "wheel_count = 0;"
        "function wheel_handler(e) {"
        "  wheel_count++;"
        "  if (wheel_count % 2 == 0)"
        "    e.preventDefault();\n"
        "  domAutomationController.send('wheel: ' + wheel_count);"
        "}"
        "function scroll_handler(e) {"
        "  domAutomationController.send('scroll: ' + wheel_count);"
        "}"
        "scroll_div = document.getElementById('scrollable_div');"
        "scroll_div.addEventListener('wheel', wheel_handler);"
        "scroll_div.addEventListener('scroll', scroll_handler);"
        "document.body.style.background = 'black';";

    content::DOMMessageQueue msg_queue;
    std::string reply;
    EXPECT_TRUE(ExecuteScript(rfh, script));

    // Wait until renderer's compositor thread is synced. Otherwise the event
    // handler won't be installed when the event arrives.
    {
      MainThreadFrameObserver observer(rfh->GetRenderWidgetHost());
      observer.Wait();
    }
  }

  void SendMouseWheel(gfx::Point location) {
    DCHECK(rwhv_root_);
    ui::ScrollEvent scroll_event(ui::ET_SCROLL, location, ui::EventTimeForNow(),
                                 0, 0, -ui::MouseWheelEvent::kWheelDelta, 0,
                                 ui::MouseWheelEvent::kWheelDelta,
                                 2);  // This must be '2' or it gets silently
                                      // dropped.
    UpdateEventRootLocation(&scroll_event, rwhv_root_);
    rwhv_root_->OnScrollEvent(&scroll_event);
  }

  void set_rwhv_root(RenderWidgetHostViewAura* rwhv_root) {
    rwhv_root_ = rwhv_root;
  }

  void RunTest(gfx::Point pos, RenderWidgetHostViewBase* expected_target) {
    content::DOMMessageQueue msg_queue;
    std::string reply;

    auto* rwhv_root = static_cast<RenderWidgetHostViewAura*>(
        web_contents()->GetRenderWidgetHostView());
    set_rwhv_root(rwhv_root);

    // Set the wheel scroll latching timeout to a large value to make sure
    // that the timer doesn't expire for the duration of the test.
    rwhv_root->event_handler()->set_mouse_wheel_wheel_phase_handler_timeout(
        TestTimeouts::action_max_timeout());

    InputEventAckWaiter waiter(expected_target->GetRenderWidgetHost(),
                               blink::WebInputEvent::kMouseWheel);
    SendMouseWheel(pos);
    waiter.Wait();

    // Expect both wheel and scroll handlers to fire.
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"wheel: 1\"", reply);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"scroll: 1\"", reply);

    SendMouseWheel(pos);

    // Even though even number events are prevented by default since the first
    // wheel event is not prevented by default, the rest of the wheel events
    // will be handled nonblocking and the scroll will happen.
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"wheel: 2\"", reply);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"scroll: 2\"", reply);

    SendMouseWheel(pos);

    // Odd number of wheels, expect both wheel and scroll handlers to fire
    // again.
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"wheel: 3\"", reply);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"scroll: 3\"", reply);
  }

 private:
  RenderWidgetHostViewAura* rwhv_root_;
};

// Fails on Windows official build, see // https://crbug.com/800822
#if defined(OS_WIN)
#define MAYBE_MultipleSubframeWheelEventsOnMainThread \
  DISABLED_MultipleSubframeWheelEventsOnMainThread
#else
#define MAYBE_MultipleSubframeWheelEventsOnMainThread \
  MultipleSubframeWheelEventsOnMainThread
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessMouseWheelHitTestBrowserTest,
                       MAYBE_MultipleSubframeWheelEventsOnMainThread) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_two_positioned_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(2U, root->child_count());

  GURL frame_url(embedded_test_server()->GetURL(
      "b.com", "/page_with_scrollable_div.html"));
  // To test for https://bugs.chromium.org/p/chromium/issues/detail?id=820232
  // it's important that both subframes are in the same renderer process, so
  // we load the same URL in each case.
  NavigateFrameToURL(root->child_at(0), frame_url);
  NavigateFrameToURL(root->child_at(1), frame_url);

  for (int frame_index = 0; frame_index < 2; frame_index++) {
    // Synchronize with the child and parent renderers to guarantee that the
    // surface information required for event hit testing is ready.
    RenderWidgetHostViewBase* child_rwhv =
        static_cast<RenderWidgetHostViewBase*>(
            root->child_at(frame_index)->current_frame_host()->GetView());

    WaitForHitTestDataOrChildSurfaceReady(
        root->child_at(frame_index)->current_frame_host());

    content::RenderFrameHostImpl* child =
        root->child_at(frame_index)->current_frame_host();
    SetupWheelAndScrollHandlers(child);

    gfx::Rect bounds = child_rwhv->GetViewBounds();
    gfx::Point pos(bounds.x() + 10, bounds.y() + 10);

    RunTest(pos, child_rwhv);
  }
}

// Verifies that test in SubframeWheelEventsOnMainThread also makes sense for
// the same page loaded in the mainframe.
// Fails on Windows official build, see // https://crbug.com/800822
#if defined(OS_WIN)
#define MAYBE_MainframeWheelEventsOnMainThread \
  DISABLED_MainframeWheelEventsOnMainThread
#else
#define MAYBE_MainframeWheelEventsOnMainThread MainframeWheelEventsOnMainThread
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessMouseWheelHitTestBrowserTest,
                       MAYBE_MainframeWheelEventsOnMainThread) {
  GURL main_url(
      embedded_test_server()->GetURL("/page_with_scrollable_div.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  content::RenderFrameHostImpl* rfhi = root->current_frame_host();
  SetupWheelAndScrollHandlers(rfhi);

  gfx::Point pos(10, 10);

  RunTest(pos, rfhi->GetRenderWidgetHost()->GetView());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessMouseWheelHitTestBrowserTest,
                       InputEventRouterWheelTargetTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  auto* rwhv_root = static_cast<RenderWidgetHostViewAura*>(
      web_contents()->GetRenderWidgetHostView());
  set_rwhv_root(rwhv_root);

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(embedded_test_server()->GetURL(
      "b.com", "/page_with_scrollable_div.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  RenderWidgetHostViewBase* child_rwhv = static_cast<RenderWidgetHostViewBase*>(
      root->child_at(0)->current_frame_host()->GetView());
  WaitForHitTestDataOrChildSurfaceReady(
      root->child_at(0)->current_frame_host());

  RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  // Send a mouse wheel event to child.
  gfx::Rect bounds = child_rwhv->GetViewBounds();
  gfx::Point pos(bounds.x() + 10, bounds.y() + 10);
  InputEventAckWaiter waiter(child_rwhv->GetRenderWidgetHost(),
                             blink::WebInputEvent::kMouseWheel);
  SendMouseWheel(pos);
  waiter.Wait();

  EXPECT_EQ(child_rwhv, router->wheel_target_.target);

  // Send a mouse wheel event to the main frame. It will be still routed to
  // child till the end of current scrolling sequence. Since wheel scroll
  // latching is enabled by default, we always do sync targeting so
  // InputEventAckWaiter is not needed here.
  TestInputEventObserver child_frame_monitor(child_rwhv->GetRenderWidgetHost());
  SendMouseWheel(pos);
  EXPECT_EQ(child_rwhv, router->wheel_target_.target);

  // Verify that this a mouse wheel event was sent to the child frame renderer.
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  const auto& child_events = child_frame_monitor.events_received();
  EXPECT_NE(child_events.end(),
            std::find(child_events.begin(), child_events.end(),
                      blink::WebInputEvent::kMouseWheel));

  // Kill the wheel target view process. This must reset the wheel_target_.
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  crash_observer.Wait();
  EXPECT_EQ(nullptr, router->wheel_target_.target);
}

// Ensure that a cross-process subframe with a touch-handler can receive touch
// events.
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       SubframeTouchEventRouting) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_touch_handler.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  WaitForHitTestDataOrChildSurfaceReady(
      root->child_at(0)->current_frame_host());

  // There's no intrinsic reason the following values can't be equal, but they
  // aren't at present, and if they become the same this test will need to be
  // updated to accommodate.
  EXPECT_NE(cc::kTouchActionAuto, cc::kTouchActionNone);

  // Verify the child's input router is initially not set. The TouchStart event
  // will trigger kTouchActionNone being sent back to the browser.
  RenderWidgetHostImpl* child_render_widget_host =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
  EXPECT_FALSE(child_render_widget_host->input_router()
                   ->AllowedTouchAction()
                   .has_value());

  InputEventAckWaiter waiter(child_render_widget_host,
                             blink::WebInputEvent::kTouchStart);

  // Simulate touch event to sub-frame.
  gfx::Point child_center(150, 150);
  auto* rwhv = static_cast<RenderWidgetHostViewAura*>(
      contents->GetRenderWidgetHostView());

  // Wait until renderer's compositor thread is synced.
  {
    MainThreadFrameObserver observer(child_render_widget_host);
    observer.Wait();
  }

  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, child_center, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                         /* pointer_id*/ 0,
                         /* radius_x */ 30.0f,
                         /* radius_y */ 30.0f,
                         /* force */ 0.0f));
  UpdateEventRootLocation(&touch_event, rwhv);
  rwhv->OnTouchEvent(&touch_event);
  waiter.Wait();
  {
    MainThreadFrameObserver observer(child_render_widget_host);
    observer.Wait();
  }

  // Verify touch handler in subframe was invoked.
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root->child_at(0),
      "window.domAutomationController.send(getLastTouchEvent());", &result));
  EXPECT_EQ("touchstart", result);

  // Verify the presence of the touch handler in the child frame correctly
  // propagates touch-action:none information back to the child's input router.
  EXPECT_EQ(cc::kTouchActionNone,
            child_render_widget_host->input_router()->AllowedTouchAction());
}

// This test verifies that the test in
// SitePerProcessHitTestBrowserTest.SubframeTouchEventRouting also works
// properly for the main frame. Prior to the CL in which this test is
// introduced, use of MainThreadFrameObserver in SubframeTouchEventRouting was
// not necessary since the touch events were handled on the main thread. Now
// they are handled on the compositor thread, hence the need to synchronize.
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MainframeTouchEventRouting) {
  GURL main_url(
      embedded_test_server()->GetURL("/page_with_touch_handler.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();

  // Synchronize with the renderers to guarantee that the
  // surface information required for event hit testing is ready.
  auto* rwhv = static_cast<RenderWidgetHostViewAura*>(
      contents->GetRenderWidgetHostView());

  // There's no intrinsic reason the following values can't be equal, but they
  // aren't at present, and if they become the same this test will need to be
  // updated to accommodate.
  EXPECT_NE(cc::kTouchActionAuto, cc::kTouchActionNone);

  // Verify the main frame's input router is initially not set. The
  // TouchStart event will trigger kTouchActionNone being sent back to the
  // browser.
  RenderWidgetHostImpl* render_widget_host =
      root->current_frame_host()->GetRenderWidgetHost();
  EXPECT_FALSE(
      render_widget_host->input_router()->AllowedTouchAction().has_value());

  // Simulate touch event to sub-frame.
  gfx::Point frame_center(150, 150);

  // Wait until renderer's compositor thread is synced.
  {
    auto observer =
        std::make_unique<MainThreadFrameObserver>(render_widget_host);
    observer->Wait();
  }

  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, frame_center, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                         /* pointer_id*/ 0,
                         /* radius_x */ 30.0f,
                         /* radius_y */ 30.0f,
                         /* force */ 0.0f));
  UpdateEventRootLocation(&touch_event, rwhv);
  rwhv->OnTouchEvent(&touch_event);
  {
    auto observer =
        std::make_unique<MainThreadFrameObserver>(render_widget_host);
    observer->Wait();
  }

  // Verify touch handler in subframe was invoked.
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root, "window.domAutomationController.send(getLastTouchEvent());",
      &result));
  EXPECT_EQ("touchstart", result);

  // Verify the presence of the touch handler in the child frame correctly
  // propagates touch-action:none information back to the child's input router.
  EXPECT_EQ(cc::kTouchActionNone,
            render_widget_host->input_router()->AllowedTouchAction());
}

// https://crbug.com/592320
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       DISABLED_SubframeGestureEventRouting) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_click_handler.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  auto* child_frame_host = root->child_at(0)->current_frame_host();

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  WaitForHitTestDataOrChildSurfaceReady(child_frame_host);

  // There have been no GestureTaps sent yet.
  {
    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        child_frame_host,
        "window.domAutomationController.send(getClickStatus());", &result));
    EXPECT_EQ("0 clicks received", result);
  }

  // Simulate touch sequence to send GestureTap to sub-frame.
  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  gfx::Point center(150, 150);
  params.position = gfx::PointF(center.x(), center.y());
  params.duration_ms = 100;
  std::unique_ptr<SyntheticTapGesture> gesture(new SyntheticTapGesture(params));

  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner();

  RenderWidgetHostImpl* render_widget_host =
      root->current_frame_host()->GetRenderWidgetHost();
  // TODO(wjmaclean): Convert the call to base::Bind() to a lambda someday.
  render_widget_host->QueueSyntheticGesture(
      std::move(gesture), base::BindOnce(OnSyntheticGestureCompleted, runner));

  // We need to run the message loop while we wait for the synthetic gesture
  // to be processed; the callback registered above will get us out of the
  // message loop when that happens.
  runner->Run();
  runner = nullptr;

  // Verify click handler in subframe was invoked
  {
    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        child_frame_host,
        "window.domAutomationController.send(getClickStatus());", &result));
    EXPECT_EQ("1 click received", result);
  }
}

namespace {

// Defined here to be close to
// SitePerProcessHitTestBrowserTest.InputEventRouterGestureTargetQueueTest.
// Will wait for RenderWidgetHost's compositor thread to sync if one is given.
// Returns the unique_touch_id of the TouchStart.
uint32_t SendTouchTapWithExpectedTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& touch_point,
    RenderWidgetHostViewBase*& router_touch_target,
    const RenderWidgetHostViewBase* expected_target,
    RenderWidgetHostImpl* child_render_widget_host) {
  auto* root_view_aura = static_cast<RenderWidgetHostViewAura*>(root_view);
  if (child_render_widget_host != nullptr) {
    MainThreadFrameObserver observer(child_render_widget_host);
    observer.Wait();
  }
  ui::TouchEvent touch_event_pressed(
      ui::ET_TOUCH_PRESSED, touch_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                         /* pointer_id*/ 0,
                         /* radius_x */ 30.0f,
                         /* radius_y */ 30.0f,
                         /* force */ 0.0f));
  UpdateEventRootLocation(&touch_event_pressed, root_view_aura);
  InputEventAckWaiter waiter(expected_target->GetRenderWidgetHost(),
                             blink::WebInputEvent::kTouchStart);
  root_view_aura->OnTouchEvent(&touch_event_pressed);
  if (child_render_widget_host != nullptr) {
    MainThreadFrameObserver observer(child_render_widget_host);
    observer.Wait();
  }
  waiter.Wait();
  EXPECT_EQ(expected_target, router_touch_target);
  ui::TouchEvent touch_event_released(
      ui::ET_TOUCH_RELEASED, touch_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                         /* pointer_id*/ 0,
                         /* radius_x */ 30.0f,
                         /* radius_y */ 30.0f,
                         /* force */ 0.0f));
  UpdateEventRootLocation(&touch_event_released, root_view_aura);
  root_view_aura->OnTouchEvent(&touch_event_released);
  if (child_render_widget_host != nullptr) {
    MainThreadFrameObserver observer(child_render_widget_host);
    observer.Wait();
  }
  EXPECT_EQ(nullptr, router_touch_target);
  return touch_event_pressed.unique_event_id();
}

void SendGestureTapSequenceWithExpectedTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& gesture_point,
    RenderWidgetHostViewBase*& router_gesture_target,
    const RenderWidgetHostViewBase* old_expected_target,
    const RenderWidgetHostViewBase* expected_target,
    const uint32_t unique_touch_event_id) {
  auto* root_view_aura = static_cast<RenderWidgetHostViewAura*>(root_view);

  ui::GestureEventDetails gesture_begin_details(ui::ET_GESTURE_BEGIN);
  gesture_begin_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_begin_event(
      gesture_point.x(), gesture_point.y(), 0, ui::EventTimeForNow(),
      gesture_begin_details, unique_touch_event_id);
  UpdateEventRootLocation(&gesture_begin_event, root_view_aura);
  root_view_aura->OnGestureEvent(&gesture_begin_event);
  // We expect to still have the old gesture target in place for the
  // GestureFlingCancel that will be inserted before GestureTapDown.
  // Note: the GestureFlingCancel is inserted by RenderWidgetHostViewAura::
  // OnGestureEvent() when it sees ui::ET_GESTURE_TAP_DOWN, so we don't
  // explicitly add it here.
  EXPECT_EQ(old_expected_target, router_gesture_target);

  ui::GestureEventDetails gesture_tap_down_details(ui::ET_GESTURE_TAP_DOWN);
  gesture_tap_down_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_tap_down_event(
      gesture_point.x(), gesture_point.y(), 0, ui::EventTimeForNow(),
      gesture_tap_down_details, unique_touch_event_id);
  UpdateEventRootLocation(&gesture_tap_down_event, root_view_aura);
  root_view_aura->OnGestureEvent(&gesture_tap_down_event);
  EXPECT_EQ(expected_target, router_gesture_target);

  ui::GestureEventDetails gesture_show_press_details(ui::ET_GESTURE_SHOW_PRESS);
  gesture_show_press_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_show_press_event(
      gesture_point.x(), gesture_point.y(), 0, ui::EventTimeForNow(),
      gesture_show_press_details, unique_touch_event_id);
  UpdateEventRootLocation(&gesture_show_press_event, root_view_aura);
  root_view_aura->OnGestureEvent(&gesture_show_press_event);
  EXPECT_EQ(expected_target, router_gesture_target);

  ui::GestureEventDetails gesture_tap_details(ui::ET_GESTURE_TAP);
  gesture_tap_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  gesture_tap_details.set_tap_count(1);
  ui::GestureEvent gesture_tap_event(gesture_point.x(), gesture_point.y(), 0,
                                     ui::EventTimeForNow(), gesture_tap_details,
                                     unique_touch_event_id);
  UpdateEventRootLocation(&gesture_tap_event, root_view_aura);
  root_view_aura->OnGestureEvent(&gesture_tap_event);
  EXPECT_EQ(expected_target, router_gesture_target);

  ui::GestureEventDetails gesture_end_details(ui::ET_GESTURE_END);
  gesture_end_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_end_event(gesture_point.x(), gesture_point.y(), 0,
                                     ui::EventTimeForNow(), gesture_end_details,
                                     unique_touch_event_id);
  UpdateEventRootLocation(&gesture_end_event, root_view_aura);
  root_view_aura->OnGestureEvent(&gesture_end_event);
  EXPECT_EQ(expected_target, router_gesture_target);
}

void SendTouchpadPinchSequenceWithExpectedTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& gesture_point,
    RenderWidgetHostViewBase*& router_touchpad_gesture_target,
    RenderWidgetHostViewBase* expected_target) {
  auto* root_view_aura = static_cast<RenderWidgetHostViewAura*>(root_view);

  ui::GestureEventDetails pinch_begin_details(ui::ET_GESTURE_PINCH_BEGIN);
  pinch_begin_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  ui::GestureEvent pinch_begin(gesture_point.x(), gesture_point.y(), 0,
                               ui::EventTimeForNow(), pinch_begin_details);
  UpdateEventRootLocation(&pinch_begin, root_view_aura);
  TestInputEventObserver target_monitor(expected_target->GetRenderWidgetHost());
  InputEventAckWaiter waiter(expected_target->GetRenderWidgetHost(),
                             blink::WebInputEvent::kGesturePinchBegin);
  root_view_aura->OnGestureEvent(&pinch_begin);
  // If the expected target is not the root, then we should be doing async
  // targeting first. So event dispatch should not happen synchronously.
  // Validate that the expected target does not receive the event immediately in
  // such cases.
  // V2 surface layer hit testing cannot handle pointer-events: none elements
  // yet, see https://crbug.com/841358.
  if (root_view != expected_target &&
      !features::IsVizHitTestingSurfaceLayerEnabled()) {
    EXPECT_FALSE(target_monitor.EventWasReceived());
  }
  waiter.Wait();
  EXPECT_TRUE(target_monitor.EventWasReceived());
  EXPECT_EQ(expected_target, router_touchpad_gesture_target);
  target_monitor.ResetEventsReceived();

  ui::GestureEventDetails pinch_update_details(ui::ET_GESTURE_PINCH_UPDATE);
  pinch_update_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  pinch_update_details.set_scale(1.23);
  ui::GestureEvent pinch_update(gesture_point.x(), gesture_point.y(), 0,
                                ui::EventTimeForNow(), pinch_update_details);
  UpdateEventRootLocation(&pinch_update, root_view_aura);
  root_view_aura->OnGestureEvent(&pinch_update);
  EXPECT_EQ(expected_target, router_touchpad_gesture_target);
  EXPECT_TRUE(target_monitor.EventWasReceived());
  EXPECT_EQ(target_monitor.EventType(),
            blink::WebInputEvent::kGesturePinchUpdate);
  target_monitor.ResetEventsReceived();

  ui::GestureEventDetails pinch_end_details(ui::ET_GESTURE_PINCH_END);
  pinch_end_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  ui::GestureEvent pinch_end(gesture_point.x(), gesture_point.y(), 0,
                             ui::EventTimeForNow(), pinch_end_details);
  UpdateEventRootLocation(&pinch_end, root_view_aura);
  root_view_aura->OnGestureEvent(&pinch_end);
  EXPECT_TRUE(target_monitor.EventWasReceived());
  EXPECT_EQ(target_monitor.EventType(), blink::WebInputEvent::kGesturePinchEnd);
  EXPECT_EQ(nullptr, router_touchpad_gesture_target);
}

#if !defined(OS_WIN)
// Sending touchpad fling events is not supported on Windows.
void SendTouchpadFlingSequenceWithExpectedTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& gesture_point,
    RenderWidgetHostViewBase*& router_wheel_target,
    RenderWidgetHostViewBase* expected_target) {
  auto* root_view_aura = static_cast<RenderWidgetHostViewAura*>(root_view);

  ui::ScrollEvent scroll_begin(ui::ET_SCROLL, gesture_point,
                               ui::EventTimeForNow(), 0, 1, 0, 1, 0, 2);
  UpdateEventRootLocation(&scroll_begin, root_view_aura);
  root_view_aura->OnScrollEvent(&scroll_begin);

  ui::ScrollEvent fling_start(ui::ET_SCROLL_FLING_START, gesture_point,
                              ui::EventTimeForNow(), 0, 1, 0, 1, 0, 1);
  UpdateEventRootLocation(&fling_start, root_view_aura);
  TestInputEventObserver target_monitor(expected_target->GetRenderWidgetHost());
  InputEventAckWaiter fling_start_waiter(
      expected_target->GetRenderWidgetHost(),
      blink::WebInputEvent::kGestureFlingStart);
  InputMsgWatcher gestrue_scroll_end_waiter(
      expected_target->GetRenderWidgetHost(),
      blink::WebInputEvent::kGestureScrollEnd);
  root_view_aura->OnScrollEvent(&fling_start);
  // If the expected target is not the root, then we should be doing async
  // targeting first. So event dispatch should not happen synchronously.
  // Validate that the expected target does not receive the event immediately in
  // such cases.
  // When V2 surface layer hit testing is enabled, we should synchronously
  // target the event to the child.
  if (root_view != expected_target &&
      !features::IsVizHitTestingSurfaceLayerEnabled()) {
    EXPECT_FALSE(target_monitor.EventWasReceived());
  }
  fling_start_waiter.Wait();
  EXPECT_TRUE(target_monitor.EventWasReceived());
  EXPECT_EQ(expected_target, router_wheel_target);
  target_monitor.ResetEventsReceived();

  // Send a GFC event, the fling_controller will process the GFC and stop the
  // fling by generating a wheel event with phaseEnded. The
  // mouse_wheel_event_queue will process the wheel event and generate a GSE.
  InputEventAckWaiter fling_cancel_waiter(
      expected_target->GetRenderWidgetHost(),
      blink::WebInputEvent::kGestureFlingCancel);
  ui::ScrollEvent fling_cancel(ui::ET_SCROLL_FLING_CANCEL, gesture_point,
                               ui::EventTimeForNow(), 0, 1, 0, 1, 0, 1);
  UpdateEventRootLocation(&fling_cancel, root_view_aura);
  root_view_aura->OnScrollEvent(&fling_cancel);
  // Since the fling velocity is small, sometimes the fling is over before
  // sending the GFC event.
  gestrue_scroll_end_waiter.GetAckStateWaitIfNecessary();
  fling_cancel_waiter.Wait();
}
#endif  // !defined(OS_WIN)

}  // anonymous namespace

// Flaky, see https://crbug.com/823578
#if defined(OS_WIN)
#define MAYBE_InputEventRouterGestureTargetMapTest \
  DISABLED_InputEventRouterGestureTargetMapTest
#else
#define MAYBE_InputEventRouterGestureTargetMapTest \
  InputEventRouterGestureTargetMapTest
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_InputEventRouterGestureTargetMapTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_click_handler.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  auto* child_frame_host = root->child_at(0)->current_frame_host();
  auto* rwhv_child =
      static_cast<RenderWidgetHostViewBase*>(child_frame_host->GetView());

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  WaitForHitTestDataOrChildSurfaceReady(child_frame_host);

  // All touches & gestures are sent to the main frame's view, and should be
  // routed appropriately from there.
  auto* rwhv_parent = static_cast<RenderWidgetHostViewBase*>(
      contents->GetRenderWidgetHostView());

  RenderWidgetHostInputEventRouter* router = contents->GetInputEventRouter();
  EXPECT_TRUE(router->touchscreen_gesture_target_map_.empty());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_.target);

  // Send touch sequence to main-frame.
  gfx::Point main_frame_point(25, 25);
  uint32_t firstId = SendTouchTapWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touch_target_.target, rwhv_parent,
      nullptr);
  EXPECT_EQ(1u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_.target);

  // Send touch sequence to child.
  gfx::Point child_center(150, 150);
  uint32_t secondId = SendTouchTapWithExpectedTarget(
      rwhv_parent, child_center, router->touch_target_.target, rwhv_child,
      nullptr);
  EXPECT_EQ(2u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_.target);

  // Send another touch sequence to main frame.
  uint32_t thirdId = SendTouchTapWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touch_target_.target, rwhv_parent,
      nullptr);
  EXPECT_EQ(3u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_.target);

  // Send Gestures to clear GestureTargetQueue.

  // The first touch sequence should generate a GestureTapDown, sent to the
  // main frame.
  SendGestureTapSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touchscreen_gesture_target_.target,
      nullptr, rwhv_parent, firstId);
  EXPECT_EQ(2u, router->touchscreen_gesture_target_map_.size());
  // Note: rwhv_parent is the target used for GestureFlingCancel sent by
  // RenderWidgetHostViewAura::OnGestureEvent() at the start of the next gesture
  // sequence; the sequence itself goes to rwhv_child.
  EXPECT_EQ(rwhv_parent, router->touchscreen_gesture_target_.target);

  // The second touch sequence should generate a GestureTapDown, sent to the
  // child frame.
  SendGestureTapSequenceWithExpectedTarget(
      rwhv_parent, child_center, router->touchscreen_gesture_target_.target,
      rwhv_parent, rwhv_child, secondId);
  EXPECT_EQ(1u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(rwhv_child, router->touchscreen_gesture_target_.target);

  // The third touch sequence should generate a GestureTapDown, sent to the
  // main frame.
  SendGestureTapSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touchscreen_gesture_target_.target,
      rwhv_child, rwhv_parent, thirdId);
  EXPECT_EQ(0u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(rwhv_parent, router->touchscreen_gesture_target_.target);
}

// TODO: Flaking test crbug.com/802827
#if defined(OS_WIN)
#define MAYBE_InputEventRouterGesturePreventDefaultTargetMapTest \
  DISABLED_InputEventRouterGesturePreventDefaultTargetMapTest
#else
#define MAYBE_InputEventRouterGesturePreventDefaultTargetMapTest \
  InputEventRouterGesturePreventDefaultTargetMapTest
#endif
#if defined(USE_AURA) || defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_P(
    SitePerProcessHitTestBrowserTest,
    MAYBE_InputEventRouterGesturePreventDefaultTargetMapTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(embedded_test_server()->GetURL(
      "b.com", "/page_with_touch_start_default_prevented.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  auto* child_frame_host = root->child_at(0)->current_frame_host();
  RenderWidgetHostImpl* child_render_widget_host =
      child_frame_host->GetRenderWidgetHost();
  auto* rwhv_child =
      static_cast<RenderWidgetHostViewBase*>(child_frame_host->GetView());

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  WaitForHitTestDataOrChildSurfaceReady(child_frame_host);

  // All touches & gestures are sent to the main frame's view, and should be
  // routed appropriately from there.
  auto* rwhv_parent = static_cast<RenderWidgetHostViewBase*>(
      contents->GetRenderWidgetHostView());

  RenderWidgetHostInputEventRouter* router = contents->GetInputEventRouter();
  EXPECT_TRUE(router->touchscreen_gesture_target_map_.empty());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_.target);

  // Send touch sequence to main-frame.
  gfx::Point main_frame_point(25, 25);
  uint32_t firstId = SendTouchTapWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touch_target_.target, rwhv_parent,
      child_render_widget_host);
  EXPECT_EQ(1u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_.target);

  // Send touch sequence to child.
  gfx::Point child_center(150, 150);
  SendTouchTapWithExpectedTarget(rwhv_parent, child_center,
                                 router->touch_target_.target, rwhv_child,
                                 child_render_widget_host);
  EXPECT_EQ(1u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_.target);

  // Send another touch sequence to main frame.
  uint32_t thirdId = SendTouchTapWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touch_target_.target, rwhv_parent,
      child_render_widget_host);
  EXPECT_EQ(2u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_.target);

  // Send Gestures to clear GestureTargetQueue.

  // The first touch sequence should generate a GestureTapDown, sent to the
  // main frame.
  SendGestureTapSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touchscreen_gesture_target_.target,
      nullptr, rwhv_parent, firstId);
  EXPECT_EQ(1u, router->touchscreen_gesture_target_map_.size());
  // Note: rwhv_parent is the target used for GestureFlingCancel sent by
  // RenderWidgetHostViewAura::OnGestureEvent() at the start of the next gesture
  // sequence; the sequence itself goes to rwhv_child.
  EXPECT_EQ(rwhv_parent, router->touchscreen_gesture_target_.target);

  // The third touch sequence should generate a GestureTapDown, sent to the
  // main frame.
  SendGestureTapSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touchscreen_gesture_target_.target,
      rwhv_parent, rwhv_parent, thirdId);
  EXPECT_EQ(0u, router->touchscreen_gesture_target_map_.size());
  EXPECT_EQ(rwhv_parent, router->touchscreen_gesture_target_.target);
}
#endif  // defined(USE_AURA) || defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       InputEventRouterTouchpadGestureTargetTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_click_handler.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  auto* child_frame_host = root->child_at(0)->current_frame_host();

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  auto* rwhv_child =
      static_cast<RenderWidgetHostViewBase*>(child_frame_host->GetView());
  WaitForHitTestDataOrChildSurfaceReady(child_frame_host);

  // All touches & gestures are sent to the main frame's view, and should be
  // routed appropriately from there.
  auto* rwhv_parent = static_cast<RenderWidgetHostViewBase*>(
      contents->GetRenderWidgetHostView());

  RenderWidgetHostInputEventRouter* router = contents->GetInputEventRouter();
  EXPECT_EQ(nullptr, router->touchpad_gesture_target_.target);

  // TODO(848050): If we send multiple touchpad pinch sequences to separate
  // views and the timing of the acks are such that the begin ack of the second
  // sequence arrives in the root before the end ack of the first sequence, we
  // would produce an invalid gesture event sequence. For now, we wait for the
  // root to receive the end ack before sending a pinch sequence to a different
  // view. The root view should preserve validity of input event sequences
  // when processing acks from multiple views, so that waiting here is not
  // necessary.
  auto wait_for_pinch_sequence_end = base::BindRepeating(
      [](RenderWidgetHost* rwh) {
        InputEventAckWaiter pinch_end_observer(
            rwh, base::BindRepeating([](content::InputEventAckSource,
                                        content::InputEventAckState,
                                        const blink::WebInputEvent& event) {
              return event.GetType() ==
                         blink::WebGestureEvent::kGesturePinchEnd &&
                     !static_cast<const blink::WebGestureEvent&>(event)
                          .NeedsWheelEvent();
            }));
        pinch_end_observer.Wait();
      },
      rwhv_parent->GetRenderWidgetHost());

  gfx::Point main_frame_point(25, 25);
  gfx::Point child_center(150, 150);

  // Send touchpad pinch sequence to main-frame.
  SendTouchpadPinchSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touchpad_gesture_target_.target,
      rwhv_parent);

  wait_for_pinch_sequence_end.Run();

  // Send touchpad pinch sequence to child.
  SendTouchpadPinchSequenceWithExpectedTarget(
      rwhv_parent, child_center, router->touchpad_gesture_target_.target,
      rwhv_child);

  wait_for_pinch_sequence_end.Run();

  // Send another touchpad pinch sequence to main frame.
  SendTouchpadPinchSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touchpad_gesture_target_.target,
      rwhv_parent);

#if !defined(OS_WIN)
  // Sending touchpad fling events is not supported on Windows.

  // Send touchpad fling sequence to main-frame.
  SendTouchpadFlingSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->wheel_target_.target, rwhv_parent);

  // Send touchpad fling sequence to child.
  SendTouchpadFlingSequenceWithExpectedTarget(
      rwhv_parent, child_center, router->wheel_target_.target, rwhv_child);

  // Send another touchpad fling sequence to main frame.
  SendTouchpadFlingSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->wheel_target_.target, rwhv_parent);
#endif
}

// Test that performing a touchpad pinch over an OOPIF offers the synthetic
// wheel events to the child and causes the page scale factor to change for
// the main frame (given that the child did not consume the wheel).
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_TouchpadPinchOverOOPIF) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_wheel_handler.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  auto* child_frame_host = root->child_at(0)->current_frame_host();

  auto* rwhv_child =
      static_cast<RenderWidgetHostViewBase*>(child_frame_host->GetView());
  WaitForHitTestDataOrChildSurfaceReady(child_frame_host);

  auto* rwhv_parent = static_cast<RenderWidgetHostViewBase*>(
      contents->GetRenderWidgetHostView());

  RenderWidgetHostInputEventRouter* router = contents->GetInputEventRouter();
  EXPECT_EQ(nullptr, router->touchpad_gesture_target_.target);

  const float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;
  const gfx::Point point_in_child(gfx::ToCeiledInt(100 * scale_factor),
                                  gfx::ToCeiledInt(100 * scale_factor));

  content::TestPageScaleObserver scale_observer(shell()->web_contents());
  SendTouchpadPinchSequenceWithExpectedTarget(
      rwhv_parent, point_in_child, router->touchpad_gesture_target_.target,
      rwhv_child);

  // Ensure the child frame saw the wheel event.
  bool default_prevented = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      child_frame_host,
      "handlerPromise.then(function(e) {"
      "  window.domAutomationController.send(e.defaultPrevented);"
      "});",
      &default_prevented));
  EXPECT_FALSE(default_prevented);

  scale_observer.WaitForPageScaleUpdate();
}

#endif  // defined(USE_AURA)

// A WebContentsDelegate to capture ContextMenu creation events.
class ContextMenuObserverDelegate : public WebContentsDelegate {
 public:
  ContextMenuObserverDelegate()
      : context_menu_created_(false),
        message_loop_runner_(new MessageLoopRunner) {}

  ~ContextMenuObserverDelegate() override {}

  bool HandleContextMenu(const content::ContextMenuParams& params) override {
    context_menu_created_ = true;
    menu_params_ = params;
    message_loop_runner_->Quit();
    return true;
  }

  ContextMenuParams getParams() { return menu_params_; }

  void Wait() {
    if (!context_menu_created_)
      message_loop_runner_->Run();
    context_menu_created_ = false;
  }

 private:
  bool context_menu_created_;
  ContextMenuParams menu_params_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuObserverDelegate);
};

// Helper function to run the CreateContextMenuTest in either normal
// or high DPI mode.
void CreateContextMenuTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));

  RenderFrameSubmissionObserver render_frame_submission_observer(
      shell->web_contents());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  // Ensure that the child process renderer is ready to have input events
  // routed to it. This happens when the browser process has received
  // updated compositor surfaces from both renderer processes.
  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  // A WebContentsDelegate to listen for the ShowContextMenu message.
  ContextMenuObserverDelegate context_menu_delegate;
  shell->web_contents()->SetDelegate(&context_menu_delegate);

  RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell->web_contents())
          ->GetInputEventRouter();

  float scale_factor =
      render_frame_submission_observer.LastRenderFrameMetadata()
          .page_scale_factor;

  gfx::Rect root_bounds = root_view->GetViewBounds();
  gfx::Rect bounds = rwhv_child->GetViewBounds();

  gfx::Point point(
      gfx::ToCeiledInt((bounds.x() - root_bounds.x() + 5) * scale_factor),
      gfx::ToCeiledInt((bounds.y() - root_bounds.y() + 5) * scale_factor));

  // Target right-click event to child frame.
  blink::WebMouseEvent click_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  click_event.button = blink::WebPointerProperties::Button::kRight;
  SetWebEventPositions(&click_event, point, root_view);
  click_event.click_count = 1;
  router->RouteMouseEvent(root_view, &click_event, ui::LatencyInfo());

  // We also need a MouseUp event, needed by Windows.
  click_event.SetType(blink::WebInputEvent::kMouseUp);
  SetWebEventPositions(&click_event, point, root_view);
  router->RouteMouseEvent(root_view, &click_event, ui::LatencyInfo());

  context_menu_delegate.Wait();

  ContextMenuParams params = context_menu_delegate.getParams();

  EXPECT_NEAR(point.x(), params.x, 2);
  EXPECT_NEAR(point.y(), params.y, 2);
}

#if defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_LINUX)
// High DPI tests don't work properly on Android, which has fixed scale factor.
// Windows is disabled because of https://crbug.com/545547.
// The test is flaky on Linux:  https://crbug.com/833380.
#define MAYBE_CreateContextMenuTest DISABLED_CreateContextMenuTest
#elif defined(THREAD_SANITIZER)
// TSAN is flaky on both standard and High DPI: https://crbug.com/833380
#define MAYBE_CreateContextMenuTest DISABLED_CreateContextMenuTest
#else
#define MAYBE_CreateContextMenuTest CreateContextMenuTest
#endif

// Test that a mouse right-click to an out-of-process iframe causes a context
// menu to be generated with the correct screen position.
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_CreateContextMenuTest) {
  CreateContextMenuTestHelper(shell(), embedded_test_server());
}

// Test that a mouse right-click to an out-of-process iframe causes a context
// menu to be generated with the correct screen position on a screen with
// non-default scale factor.
IN_PROC_BROWSER_TEST_P(SitePerProcessHighDPIHitTestBrowserTest,
                       MAYBE_CreateContextMenuTest) {
  CreateContextMenuTestHelper(shell(), embedded_test_server());
}

// Test that clicking a select element in an out-of-process iframe creates
// a popup menu in the correct position.
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest, PopupMenuTest) {
  GURL main_url(
      embedded_test_server()->GetURL("/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "baz.com", "/site_isolation/page-with-select.html"));
  NavigateFrameToURL(child_node, site_url);

  web_contents()->SendScreenRects();

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  scoped_refptr<ShowWidgetMessageFilter> filter = new ShowWidgetMessageFilter();
  child_node->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Target left-click event to child frame.
  blink::WebMouseEvent click_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  click_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&click_event, gfx::Point(15, 15), rwhv_root);
  click_event.click_count = 1;
  rwhv_child->ProcessMouseEvent(click_event, ui::LatencyInfo());

  // Dismiss the popup.
  SetWebEventPositions(&click_event, gfx::Point(1, 1), rwhv_root);
  rwhv_child->ProcessMouseEvent(click_event, ui::LatencyInfo());

  filter->Wait();
  gfx::Rect popup_rect = filter->last_initial_rect();
  if (IsUseZoomForDSFEnabled()) {
    ScreenInfo screen_info;
    shell()->web_contents()->GetRenderWidgetHostView()->GetScreenInfo(
        &screen_info);
    popup_rect = gfx::ScaleToRoundedRect(popup_rect,
                                         1 / screen_info.device_scale_factor);
  }
#if defined(OS_MACOSX) || defined(OS_ANDROID)
  // On Mac and Android we receive the coordinates before they are transformed,
  // so they are still relative to the out-of-process iframe origin.
  EXPECT_EQ(popup_rect.x(), 9);
  EXPECT_EQ(popup_rect.y(), 9);
#else
  EXPECT_EQ(popup_rect.x() - rwhv_root->GetViewBounds().x(), 354);
  EXPECT_EQ(popup_rect.y() - rwhv_root->GetViewBounds().y(), 94);
#endif

#if defined(OS_LINUX)
  // Verify click-and-drag selection of popups still works on Linux with
  // OOPIFs enabled. This is only necessary to test on Aura because Mac and
  // Android use native widgets. Windows does not support this as UI
  // convention (it requires separate clicks to open the menu and select an
  // option). See https://crbug.com/703191.
  int process_id = child_node->current_frame_host()->GetProcess()->GetID();
  filter->Reset();
  RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetInputEventRouter();
  // Re-open the select element.
  SetWebEventPositions(&click_event, gfx::Point(360, 90), rwhv_root);
  click_event.click_count = 1;
  router->RouteMouseEvent(rwhv_root, &click_event, ui::LatencyInfo());

  filter->Wait();

  RenderWidgetHostViewAura* popup_view = static_cast<RenderWidgetHostViewAura*>(
      RenderWidgetHost::FromID(process_id, filter->last_routing_id())
          ->GetView());
  // The IO thread posts to ViewMsg_ShowWidget handlers in both the message
  // filter above and the WebContents, which initializes the popup's view.
  // It is possible for this code to execute before the WebContents handler,
  // in which case OnMouseEvent would be called on an uninitialized RWHVA.
  // This loop ensures that the initialization completes before proceeding.
  while (!popup_view->window()) {
    base::RunLoop loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  loop.QuitClosure());
    loop.Run();
  }

  RenderWidgetHostMouseEventMonitor popup_monitor(
      popup_view->GetRenderWidgetHost());

  // Next send a mouse up directly targeting the first option, simulating a
  // drag. This requires a ui::MouseEvent because it tests behavior that is
  // above RWH input event routing.
  ui::MouseEvent mouse_up_event(ui::ET_MOUSE_RELEASED, gfx::Point(10, 5),
                                gfx::Point(10, 5), ui::EventTimeForNow(),
                                ui::EF_LEFT_MOUSE_BUTTON,
                                ui::EF_LEFT_MOUSE_BUTTON);
  UpdateEventRootLocation(&mouse_up_event, rwhv_root);
  popup_view->OnMouseEvent(&mouse_up_event);

  // This verifies that the popup actually received the event, and it wasn't
  // diverted to a different RenderWidgetHostView due to mouse capture.
  EXPECT_TRUE(popup_monitor.EventWasReceived());
#endif  // defined(OS_LINUX)
}

// Test that clicking a select element in a nested out-of-process iframe creates
// a popup menu in the correct position, even if the top-level page repositions
// its out-of-process iframe. This verifies that screen positioning information
// is propagating down the frame tree correctly.
#if defined(OS_ANDROID)
// Surface-based hit testing and coordinate translation is not yet avaiable on
// Android.
#define MAYBE_NestedPopupMenuTest DISABLED_NestedPopupMenuTest
#else
// Times out frequently. https://crbug.com/599730.
#define MAYBE_NestedPopupMenuTest DISABLED_NestedPopupMenuTest
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       MAYBE_NestedPopupMenuTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  web_contents()->SendScreenRects();

  // For clarity, we are labeling the frame tree nodes as:
  //  - root_node
  //   \-> b_node (out-of-process from root and c_node)
  //     \-> c_node (out-of-process from root and b_node)

  content::TestNavigationObserver navigation_observer(shell()->web_contents());
  FrameTreeNode* b_node = root->child_at(0);
  FrameTreeNode* c_node = b_node->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "baz.com", "/site_isolation/page-with-select.html"));
  NavigateFrameToURL(c_node, site_url);

  RenderWidgetHostViewBase* rwhv_c_node =
      static_cast<RenderWidgetHostViewBase*>(
          c_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            c_node->current_frame_host()->GetSiteInstance());

  scoped_refptr<ShowWidgetMessageFilter> filter = new ShowWidgetMessageFilter();
  c_node->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Target left-click event to child frame.
  blink::WebMouseEvent click_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  click_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&click_event, gfx::Point(15, 15), rwhv_root);
  click_event.click_count = 1;
  rwhv_c_node->ProcessMouseEvent(click_event, ui::LatencyInfo());

  // Prompt the WebContents to dismiss the popup by clicking elsewhere.
  SetWebEventPositions(&click_event, gfx::Point(1, 1), rwhv_root);
  rwhv_c_node->ProcessMouseEvent(click_event, ui::LatencyInfo());

  filter->Wait();

  gfx::Rect popup_rect = filter->last_initial_rect();

#if defined(OS_MACOSX)
  EXPECT_EQ(popup_rect.x(), 9);
  EXPECT_EQ(popup_rect.y(), 9);
#else
  EXPECT_EQ(popup_rect.x() - rwhv_root->GetViewBounds().x(), 354);
  EXPECT_EQ(popup_rect.y() - rwhv_root->GetViewBounds().y(), 154);
#endif

  // Save the screen rect for b_node. Since it updates asynchronously from
  // the script command that changes it, we need to wait for it to change
  // before attempting to create the popup widget again.
  gfx::Rect last_b_node_bounds_rect =
      b_node->current_frame_host()->GetView()->GetViewBounds();

  std::string script =
      "var iframe = document.querySelector('iframe');"
      "iframe.style.position = 'absolute';"
      "iframe.style.left = 150;"
      "iframe.style.top = 150;";
  EXPECT_TRUE(ExecuteScript(root, script));

  filter->Reset();

  // Busy loop to wait for b_node's screen rect to get updated. There
  // doesn't seem to be any better way to find out when this happens.
  while (last_b_node_bounds_rect.x() ==
             b_node->current_frame_host()->GetView()->GetViewBounds().x() &&
         last_b_node_bounds_rect.y() ==
             b_node->current_frame_host()->GetView()->GetViewBounds().y()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  click_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&click_event, gfx::Point(15, 15), rwhv_root);
  click_event.click_count = 1;
  rwhv_c_node->ProcessMouseEvent(click_event, ui::LatencyInfo());

  SetWebEventPositions(&click_event, gfx::Point(1, 1), rwhv_root);
  rwhv_c_node->ProcessMouseEvent(click_event, ui::LatencyInfo());

  filter->Wait();

  popup_rect = filter->last_initial_rect();

#if defined(OS_MACOSX)
  EXPECT_EQ(popup_rect.x(), 9);
  EXPECT_EQ(popup_rect.y(), 9);
#else
  EXPECT_EQ(popup_rect.x() - rwhv_root->GetViewBounds().x(), 203);
  EXPECT_EQ(popup_rect.y() - rwhv_root->GetViewBounds().y(), 248);
#endif
}

// Verify that scrolling the main frame correctly updates the position to
// a nested child frame. See issue https://crbug.com/878703 for more
// information.
// On Mac and Android, the reported menu coordinates are relative to the
// OOPIF, and its screen position is computed later, so this test isn't
// relevant on those platforms.
#if !defined(OS_ANDROID) && !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest,
                       ScrolledNestedPopupMenuTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_tall_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child_node = root->child_at(0);

  GURL child_url(embedded_test_server()->GetURL(
      "b.com", "/frame_tree/page_with_positioned_frame.html"));
  NavigateFrameToURL(child_node, child_url);

  FrameTreeNode* grandchild_node = child_node->child_at(0);

  GURL grandchild_url(embedded_test_server()->GetURL(
      "c.com", "/site_isolation/page-with-select.html"));
  NavigateFrameToURL(grandchild_node, grandchild_url);

  WaitForHitTestDataOrChildSurfaceReady(grandchild_node->current_frame_host());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/",
      DepictFrameTree(root));

  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_grandchild =
      static_cast<RenderWidgetHostViewBase*>(
          grandchild_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  scoped_refptr<ShowWidgetMessageFilter> filter = new ShowWidgetMessageFilter();
  grandchild_node->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Target left-click event to the select element in the innermost frame.
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), rwhv_grandchild,
                                         gfx::PointF(15, 15), rwhv_grandchild,
                                         gfx::PointF(15, 15));

  // Prompt the WebContents to dismiss the popup by clicking elsewhere.
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), rwhv_grandchild,
                                         gfx::PointF(2, 2), rwhv_grandchild,
                                         gfx::PointF(2, 2));
  filter->Wait();

  // This test isn't verifying correctness of these coordinates, this is just
  // to ensure that they change after scroll.
  gfx::Rect unscrolled_popup_rect = filter->last_initial_rect();
  gfx::Rect initial_grandchild_view_bounds = rwhv_grandchild->GetViewBounds();

  // Scroll the main frame.
  EXPECT_TRUE(ExecuteScript(root, "window.scrollTo(0, 20);"));

  // Wait until the OOPIF positions have been updated in the browser process.
  while (true) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
    if (initial_grandchild_view_bounds.y() ==
        rwhv_grandchild->GetViewBounds().y() + 20)
      break;
  }

  filter->Reset();
  // This sends the message directly to the rwhv_grandchild, avoiding using
  // the helper methods, to avert a race condition with the surfaces or
  // HitTestRegions needing to update post-scroll. The event won't hit test
  // correctly if it gets sent before a fresh compositor frame is received.
  blink::WebMouseEvent down_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  down_event.button = blink::WebPointerProperties::Button::kLeft;
  down_event.click_count = 1;
  down_event.SetPositionInWidget(15, 15);
  rwhv_grandchild->ProcessMouseEvent(down_event, ui::LatencyInfo());

  // Dismiss the popup again. This time there is no need to worry about
  // compositor frame updates because it is sufficient to send the click to
  // the root frame.
  DispatchMouseEventAndWaitUntilDispatch(web_contents(), rwhv_root,
                                         gfx::PointF(1, 1), rwhv_root,
                                         gfx::PointF(1, 1));
  filter->Wait();
  EXPECT_EQ(unscrolled_popup_rect.y(), filter->last_initial_rect().y() + 20);
}
#endif  // !defined(OS_ANDROID)

#if defined(USE_AURA)
class SitePerProcessGestureHitTestBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  SitePerProcessGestureHitTestBrowserTest() {}

  // This functions simulates a sequence of events that are typical of a
  // gesture pinch at |position|. We need this since machinery in the event
  // codepath will require GesturePinch* to be enclosed in
  // GestureScrollBegin/End, and since RenderWidgetHostInputEventRouter needs
  // both the preceding touch events, as well as GestureTapDown, in order to
  // correctly target the subsequent gesture event stream. The minimum stream
  // required to trigger the correct behaviours is represented here, but could
  // be expanded to include additional events such as one or more
  // GestureScrollUpdate and GesturePinchUpdate events.
  void SendPinchBeginEndSequence(RenderWidgetHostViewAura* rwhva,
                                 const gfx::Point& position,
                                 RenderWidgetHost* expected_target_rwh) {
    DCHECK(rwhva);
    // Use full version of constructor with radius, angle and force since it
    // will crash in the renderer otherwise.
    ui::TouchEvent touch_pressed(
        ui::ET_TOUCH_PRESSED, position, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           /* pointer_id*/ 0,
                           /* radius_x */ 1.0f,
                           /* radius_y */ 1.0f,
                           /* force */ 1.0f));
    UpdateEventRootLocation(&touch_pressed, rwhva);
    InputEventAckWaiter waiter(expected_target_rwh,
                               blink::WebInputEvent::kTouchStart);
    rwhva->OnTouchEvent(&touch_pressed);
    waiter.Wait();

    ui::GestureEventDetails gesture_tap_down_details(ui::ET_GESTURE_TAP_DOWN);
    gesture_tap_down_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_tap_down(
        position.x(), position.y(), 0, ui::EventTimeForNow(),
        gesture_tap_down_details, touch_pressed.unique_event_id());
    UpdateEventRootLocation(&gesture_tap_down, rwhva);
    rwhva->OnGestureEvent(&gesture_tap_down);

    ui::GestureEventDetails gesture_scroll_begin_details(
        ui::ET_GESTURE_SCROLL_BEGIN);
    gesture_scroll_begin_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    gesture_scroll_begin_details.set_touch_points(2);
    ui::GestureEvent gesture_scroll_begin(
        position.x(), position.y(), 0, ui::EventTimeForNow(),
        gesture_scroll_begin_details, touch_pressed.unique_event_id());
    UpdateEventRootLocation(&gesture_scroll_begin, rwhva);
    rwhva->OnGestureEvent(&gesture_scroll_begin);

    ui::GestureEventDetails gesture_pinch_begin_details(
        ui::ET_GESTURE_PINCH_BEGIN);
    gesture_pinch_begin_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_pinch_begin(
        position.x(), position.y(), 0, ui::EventTimeForNow(),
        gesture_pinch_begin_details, touch_pressed.unique_event_id());
    UpdateEventRootLocation(&gesture_pinch_begin, rwhva);
    rwhva->OnGestureEvent(&gesture_pinch_begin);

    ui::GestureEventDetails gesture_pinch_end_details(ui::ET_GESTURE_PINCH_END);
    gesture_pinch_end_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_pinch_end(
        position.x(), position.y(), 0, ui::EventTimeForNow(),
        gesture_pinch_end_details, touch_pressed.unique_event_id());
    UpdateEventRootLocation(&gesture_pinch_end, rwhva);
    rwhva->OnGestureEvent(&gesture_pinch_end);

    ui::GestureEventDetails gesture_scroll_end_details(
        ui::ET_GESTURE_SCROLL_END);
    gesture_scroll_end_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_scroll_end(
        position.x(), position.y(), 0, ui::EventTimeForNow(),
        gesture_scroll_end_details, touch_pressed.unique_event_id());
    UpdateEventRootLocation(&gesture_scroll_end, rwhva);
    rwhva->OnGestureEvent(&gesture_scroll_end);

    // TouchActionFilter is reset when a touch event sequence ends, so in order
    // to preserve the touch action set by TouchStart, we end release touch
    // after pinch gestures.
    ui::TouchEvent touch_released(
        ui::ET_TOUCH_RELEASED, position, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           /* pointer_id*/ 0,
                           /* radius_x */ 1.0f,
                           /* radius_y */ 1.0f,
                           /* force */ 1.0f));
    InputEventAckWaiter touch_released_waiter(expected_target_rwh,
                                              blink::WebInputEvent::kTouchEnd);
    rwhva->OnTouchEvent(&touch_released);
    touch_released_waiter.Wait();
  }

  void SetupRootAndChild() {
    GURL main_url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(b)"));
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    FrameTreeNode* root_node =
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetFrameTree()
            ->root();
    FrameTreeNode* child_node = root_node->child_at(0);

    rwhv_child_ = static_cast<RenderWidgetHostViewBase*>(
        child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

    rwhva_root_ = static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());

    WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

    rwhi_child_ = child_node->current_frame_host()->GetRenderWidgetHost();
    rwhi_root_ = root_node->current_frame_host()->GetRenderWidgetHost();
  }

  void SubframeGesturePinchTestHelper(const std::string& url,
                                      bool reset_root_touch_action) {
    GURL main_url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(b)"));

    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    // It is safe to obtain the root frame tree node here, as it doesn't change.
    FrameTreeNode* root_node =
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetFrameTree()
            ->root();
    ASSERT_EQ(1U, root_node->child_count());

    FrameTreeNode* child_node = root_node->child_at(0);
    GURL b_url(embedded_test_server()->GetURL("b.com", url));
    NavigateFrameToURL(child_node, b_url);

    ASSERT_EQ(
        " Site A ------------ proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://b.com/",
        DepictFrameTree(root_node));

    rwhv_child_ = static_cast<RenderWidgetHostViewBase*>(
        child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

    rwhva_root_ = static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());

    WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());

    MainThreadFrameObserver observer(rwhv_child_->GetRenderWidgetHost());
    observer.Wait();

    rwhi_child_ = child_node->current_frame_host()->GetRenderWidgetHost();
    rwhi_root_ = root_node->current_frame_host()->GetRenderWidgetHost();

    TestInputEventObserver root_frame_monitor(rwhi_root_);
    TestInputEventObserver child_frame_monitor(rwhi_child_);

    gfx::Rect bounds = rwhv_child_->GetViewBounds();
    bounds.Offset(gfx::Point() - rwhva_root_->GetViewBounds().origin());

    // The pinch gesture will always sent to the root frame even if the fingers
    // are targeting the iframe. In this case, the test should not crash.
    if (reset_root_touch_action) {
      static_cast<InputRouterImpl*>(
          static_cast<RenderWidgetHostImpl*>(rwhva_root_->GetRenderWidgetHost())
              ->input_router())
          ->OnHasTouchEventHandlersForTest(true);
    }
    SendPinchBeginEndSequence(rwhva_root_, bounds.CenterPoint(), rwhi_child_);

    if (reset_root_touch_action)
      return;

    // Verify that root-RWHI gets nothing.
    EXPECT_FALSE(root_frame_monitor.EventWasReceived());
    // Verify that child-RWHI gets TS/GTD/GSB/GPB/GPE/GSE/TE.
    EXPECT_EQ(blink::WebInputEvent::kTouchStart,
              child_frame_monitor.events_received()[0]);
    EXPECT_EQ(blink::WebInputEvent::kGestureTapDown,
              child_frame_monitor.events_received()[1]);
    EXPECT_EQ(blink::WebInputEvent::kGestureScrollBegin,
              child_frame_monitor.events_received()[2]);
    EXPECT_EQ(blink::WebInputEvent::kGesturePinchBegin,
              child_frame_monitor.events_received()[3]);
    EXPECT_EQ(blink::WebInputEvent::kGesturePinchEnd,
              child_frame_monitor.events_received()[4]);
    EXPECT_EQ(blink::WebInputEvent::kGestureScrollEnd,
              child_frame_monitor.events_received()[5]);
    EXPECT_EQ(blink::WebInputEvent::kTouchEnd,
              child_frame_monitor.events_received()[6]);

    // Verify that the pinch gestures are consumed by browser.
    EXPECT_EQ(InputEventAckSource::BROWSER,
              child_frame_monitor.events_acked()[3]);
    EXPECT_EQ(InputEventAckSource::BROWSER,
              child_frame_monitor.events_acked()[4]);
  }

 protected:
  RenderWidgetHostViewBase* rwhv_child_;
  RenderWidgetHostViewAura* rwhva_root_;
  RenderWidgetHostImpl* rwhi_child_;
  RenderWidgetHostImpl* rwhi_root_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SitePerProcessGestureHitTestBrowserTest);
};

IN_PROC_BROWSER_TEST_P(SitePerProcessGestureHitTestBrowserTest,
                       SubframeGesturePinchGoesToMainFrame) {
  SetupRootAndChild();

  TestInputEventObserver root_frame_monitor(rwhi_root_);
  TestInputEventObserver child_frame_monitor(rwhi_child_);

  // Need child rect in main frame coords.
  gfx::Rect bounds = rwhv_child_->GetViewBounds();
  bounds.Offset(gfx::Point() - rwhva_root_->GetViewBounds().origin());
  SendPinchBeginEndSequence(rwhva_root_, bounds.CenterPoint(), rwhi_child_);

  // Verify root-RWHI gets GSB/GPB/GPE/GSE.
  EXPECT_TRUE(root_frame_monitor.EventWasReceived());
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollBegin,
            root_frame_monitor.events_received()[0]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchBegin,
            root_frame_monitor.events_received()[1]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchEnd,
            root_frame_monitor.events_received()[2]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollEnd,
            root_frame_monitor.events_received()[3]);

  // Verify child-RWHI gets TS/TE, GTD/GSB/GSE.
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  EXPECT_EQ(blink::WebInputEvent::kTouchStart,
            child_frame_monitor.events_received()[0]);
  EXPECT_EQ(blink::WebInputEvent::kGestureTapDown,
            child_frame_monitor.events_received()[1]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollBegin,
            child_frame_monitor.events_received()[2]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollEnd,
            child_frame_monitor.events_received()[3]);
  EXPECT_EQ(blink::WebInputEvent::kTouchEnd,
            child_frame_monitor.events_received()[4]);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessGestureHitTestBrowserTest,
                       MainframeGesturePinchGoesToMainFrame) {
  SetupRootAndChild();

  TestInputEventObserver root_frame_monitor(rwhi_root_);
  TestInputEventObserver child_frame_monitor(rwhi_child_);

  // Need child rect in main frame coords.
  gfx::Rect bounds = rwhv_child_->GetViewBounds();
  bounds.Offset(gfx::Point() - rwhva_root_->GetViewBounds().origin());

  gfx::Point main_frame_point(bounds.origin());
  main_frame_point += gfx::Vector2d(-5, -5);
  SendPinchBeginEndSequence(rwhva_root_, main_frame_point, rwhi_root_);

  // Verify root-RWHI gets TS/TE/GTD/GSB/GPB/GPE/GSE.
  EXPECT_TRUE(root_frame_monitor.EventWasReceived());
  EXPECT_EQ(blink::WebInputEvent::kTouchStart,
            root_frame_monitor.events_received()[0]);
  EXPECT_EQ(blink::WebInputEvent::kGestureTapDown,
            root_frame_monitor.events_received()[1]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollBegin,
            root_frame_monitor.events_received()[2]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchBegin,
            root_frame_monitor.events_received()[3]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchEnd,
            root_frame_monitor.events_received()[4]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollEnd,
            root_frame_monitor.events_received()[5]);
  EXPECT_EQ(blink::WebInputEvent::kTouchEnd,
            root_frame_monitor.events_received()[6]);

  // Verify child-RWHI gets no events.
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());
}

IN_PROC_BROWSER_TEST_P(SitePerProcessGestureHitTestBrowserTest,
                       SubframeGesturePinchDeniedBySubframeTouchAction) {
  SubframeGesturePinchTestHelper("/div_with_touch_action_none.html", false);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessGestureHitTestBrowserTest,
                       SubframeGesturePinchNoCrash) {
  SubframeGesturePinchTestHelper("/div_with_touch_action_auto.html", true);
}
#endif  // defined(USE_AURA)

// Test that MouseDown and MouseUp to the same coordinates do not result in
// different coordinates after routing. See bug https://crbug.com/670253.
#if defined(OS_ANDROID)
// Android uses fixed scale factor, which makes this test unnecessary.
#define MAYBE_MouseClickWithNonIntegerScaleFactor \
  DISABLED_MouseClickWithNonIntegerScaleFactor
#else
#define MAYBE_MouseClickWithNonIntegerScaleFactor \
  MouseClickWithNonIntegerScaleFactor
#endif
IN_PROC_BROWSER_TEST_P(SitePerProcessNonIntegerScaleFactorHitTestBrowserTest,
                       MAYBE_MouseClickWithNonIntegerScaleFactor) {
  GURL initial_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  RenderWidgetHostViewBase* rwhv = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetInputEventRouter();

  // Create listener for input events.
  RenderWidgetHostMouseEventMonitor event_monitor(
      root->current_frame_host()->GetRenderWidgetHost());

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  SetWebEventPositions(&mouse_event, gfx::Point(75, 75), rwhv);
  mouse_event.click_count = 1;
  event_monitor.ResetEventReceived();
  router->RouteMouseEvent(rwhv, &mouse_event, ui::LatencyInfo());

  EXPECT_TRUE(event_monitor.EventWasReceived());
  gfx::Point mouse_down_coords =
      gfx::Point(event_monitor.event().PositionInWidget().x,
                 event_monitor.event().PositionInWidget().y);
  event_monitor.ResetEventReceived();

  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  SetWebEventPositions(&mouse_event, gfx::Point(75, 75), rwhv);
  router->RouteMouseEvent(rwhv, &mouse_event, ui::LatencyInfo());

  EXPECT_TRUE(event_monitor.EventWasReceived());
  EXPECT_EQ(mouse_down_coords.x(), event_monitor.event().PositionInWidget().x);
  // The transform from browser to renderer is (2, 35) in DIP. When we
  // scale that to pixels, it's (3, 53). Note that 35 * 1.5 should be 52.5,
  // so we already lost precision there in the transform from draw quad.
  EXPECT_NEAR(mouse_down_coords.y(), event_monitor.event().PositionInWidget().y,
              1);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessNonIntegerScaleFactorHitTestBrowserTest,
                       NestedSurfaceHitTestTest) {
  NestedSurfaceHitTestTestHelper(shell(), embedded_test_server());
}

// Verify InputTargetClient works within an OOPIF process.
IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestBrowserTest, HitTestNestedFrames) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://a.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  FrameTreeNode* child_node = root->child_at(0);
  FrameTreeNode* grandchild_node = child_node->child_at(0);
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_grandchild =
      static_cast<RenderWidgetHostViewBase*>(
          grandchild_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  WaitForHitTestDataOrChildSurfaceReady(child_node->current_frame_host());
  WaitForHitTestDataOrChildSurfaceReady(grandchild_node->current_frame_host());

  // Create two points to hit test: One in the child of the main frame, and
  // one in the frame nested within that. The hit test request is sent to the
  // child's renderer.
  gfx::Point point_in_child(1, 1);
  gfx::PointF point_in_nested_child(5, 5);
  rwhv_grandchild->TransformPointToCoordSpaceForView(
      point_in_nested_child, rwhv_child, &point_in_nested_child);

  {
    base::RunLoop run_loop;
    viz::FrameSinkId received_frame_sink_id;
    gfx::PointF returned_point;
    base::Closure quit_closure =
        content::GetDeferredQuitTaskForRunLoop(&run_loop);
    DCHECK_NE(child_node->current_frame_host()->GetInputTargetClient(),
              nullptr);
    child_node->current_frame_host()->GetInputTargetClient()->FrameSinkIdAt(
        point_in_child,
        base::BindLambdaForTesting(
            [&](const viz::FrameSinkId& id, const gfx::PointF& point) {
              received_frame_sink_id = id;
              returned_point = point;
              quit_closure.Run();
            }));
    content::RunThisRunLoop(&run_loop);
    // |point_in_child| should hit test to the view for |child_node|.
    ASSERT_EQ(rwhv_child->GetFrameSinkId(), received_frame_sink_id);
    ASSERT_EQ(gfx::PointF(1, 1), returned_point);
  }

  {
    base::RunLoop run_loop;
    viz::FrameSinkId received_frame_sink_id;
    gfx::PointF returned_point;
    base::Closure quit_closure =
        content::GetDeferredQuitTaskForRunLoop(&run_loop);
    DCHECK_NE(child_node->current_frame_host()->GetInputTargetClient(),
              nullptr);
    child_node->current_frame_host()->GetInputTargetClient()->FrameSinkIdAt(
        gfx::ToCeiledPoint(point_in_nested_child),
        base::BindLambdaForTesting(
            [&](const viz::FrameSinkId& id, const gfx::PointF& point) {
              received_frame_sink_id = id;
              returned_point = point;
              quit_closure.Run();
            }));
    content::RunThisRunLoop(&run_loop);
    // |point_in_nested_child| should hit test to |rwhv_grandchild|.
    ASSERT_EQ(rwhv_grandchild->GetFrameSinkId(), received_frame_sink_id);
    EXPECT_NEAR(returned_point.x(), 5, 2);
    EXPECT_NEAR(returned_point.y(), 5, 2);
  }
}

class SitePerProcessHitTestDataGenerationBrowserTest
    : public SitePerProcessHitTestBrowserTest {
 public:
  SitePerProcessHitTestDataGenerationBrowserTest() {}

 protected:
  // Load the page |host_name| and retrieve the hit test data from HitTestQuery.
  std::vector<viz::AggregatedHitTestRegion> SetupAndGetHitTestData(
      const std::string& host_name) {
    GURL main_url(embedded_test_server()->GetURL(host_name));
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetFrameTree()
                              ->root();

    RenderWidgetHostViewBase* rwhv_root =
        static_cast<RenderWidgetHostViewBase*>(
            root->current_frame_host()->GetRenderWidgetHost()->GetView());

    for (unsigned i = 0; i < root->child_count(); i++) {
      WaitForHitTestDataOrChildSurfaceReady(
          root->child_at(i)->current_frame_host());
    }

    HitTestRegionObserver observer(rwhv_root->GetRootFrameSinkId());
    observer.WaitForHitTestData();

    device_scale_factor_ = rwhv_root->GetDeviceScaleFactor();
    DCHECK_GT(device_scale_factor_, 0);

    return observer.GetHitTestData();
  }

  float current_device_scale_factor() const { return device_scale_factor_; }

  gfx::QuadF TransformRectToQuadF(const gfx::Rect& rect,
                                  const gfx::Transform& transform,
                                  bool use_scale_factor = true) {
    gfx::Rect scaled_rect =
        use_scale_factor ? gfx::ScaleToEnclosingRect(rect, device_scale_factor_,
                                                     device_scale_factor_)
                         : rect;
    gfx::PointF p1(scaled_rect.origin());
    gfx::PointF p2(scaled_rect.top_right());
    gfx::PointF p3(scaled_rect.bottom_right());
    gfx::PointF p4(scaled_rect.bottom_left());
    transform.TransformPoint(&p1);
    transform.TransformPoint(&p2);
    transform.TransformPoint(&p3);
    transform.TransformPoint(&p4);
    return gfx::QuadF(p1, p2, p3, p4);
  }

  gfx::QuadF TransformRectToQuadF(
      const viz::AggregatedHitTestRegion& hit_test_region) {
    return TransformRectToQuadF(hit_test_region.rect,
                                hit_test_region.transform(), false);
  }

  bool ApproximatelyEqual(const gfx::PointF& p1, const gfx::PointF& p2) const {
    return std::abs(p1.x() - p2.x()) <= 1 && std::abs(p1.y() - p2.y()) <= 1;
  }

  bool ApproximatelyEqual(const gfx::QuadF& quad,
                          const gfx::QuadF& other) const {
    return ApproximatelyEqual(quad.p1(), other.p1()) &&
           ApproximatelyEqual(quad.p2(), other.p2()) &&
           ApproximatelyEqual(quad.p3(), other.p3()) &&
           ApproximatelyEqual(quad.p4(), other.p4());
  }

  gfx::Rect AxisAlignedLayoutRectFromHitTest(
      const viz::AggregatedHitTestRegion& hit_test_region) {
    DCHECK(hit_test_region.transform().Preserves2dAxisAlignment());
    gfx::RectF rect(hit_test_region.rect);
    hit_test_region.transform().TransformRect(&rect);
    return gfx::ToEnclosingRect(rect);
  }

 public:
  static const uint32_t kFastHitTestFlags;
  static const uint32_t kSlowHitTestFlags;
  float device_scale_factor_;
};

const uint32_t
    SitePerProcessHitTestDataGenerationBrowserTest::kFastHitTestFlags =
        viz::HitTestRegionFlags::kHitTestMine |
        viz::HitTestRegionFlags::kHitTestChildSurface |
        viz::HitTestRegionFlags::kHitTestMouse |
        viz::HitTestRegionFlags::kHitTestTouch;

const uint32_t
    SitePerProcessHitTestDataGenerationBrowserTest::kSlowHitTestFlags =
        SitePerProcessHitTestDataGenerationBrowserTest::kFastHitTestFlags |
        viz::HitTestRegionFlags::kHitTestAsk;

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestDataGenerationBrowserTest,
                       TransformedOOPIF) {
  if (!features::IsVizHitTestingEnabled())
    return;
  auto hit_test_data =
      SetupAndGetHitTestData("/frame_tree/page_with_transformed_iframe.html");
  float device_scale_factor = current_device_scale_factor();

  // Compute screen space transform for iframe element.
  gfx::Transform expected_transform;
  gfx::Transform translate;
  expected_transform.RotateAboutZAxis(-45);
  translate.Translate(-100 * device_scale_factor, -100 * device_scale_factor);
  expected_transform.PreconcatTransform(translate);

  DCHECK(hit_test_data.size() >= 3);
  // The iframe element in main page is transformed and also clips the content
  // of the subframe, so we expect to do slow path hit testing in this case.
  // TODO(sunxd): We should do fast path hit testing in this case. See
  // https://crbug.com/851507.
  EXPECT_TRUE(ApproximatelyEqual(
      TransformRectToQuadF(gfx::Rect(100, 100), expected_transform),
      TransformRectToQuadF(hit_test_data[2])));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestDataGenerationBrowserTest,
                       ClippedOOPIFFastPath) {
  if (!features::IsVizHitTestingEnabled())
    return;
  auto hit_test_data =
      SetupAndGetHitTestData("/frame_tree/page_with_clipped_iframe.html");
  float device_scale_factor = current_device_scale_factor();
  gfx::Transform expected_transform;
  // In V1 hit testing or V2 hit testing slow path, we expected unclipped iframe
  // bounds in its own space.
  gfx::Rect original_region(200, 200);
  gfx::Rect expected_transformed_region = gfx::ScaleToEnclosingRect(
      original_region, device_scale_factor, device_scale_factor);

  uint32_t expected_flags = kFastHitTestFlags;
  // Clip2 has overflow: visible property, so it does not apply clip to iframe.
  // Clip1 and clip3 all preserve 2d axis alignment, so we should allow fast
  // path hit testing for the iframe in V2 hit testing.
  // When VizDisplayCompositor is enabled, HitTestDataProviderDrawQuad will
  // override LTHI's hit test data.
  if (features::IsVizHitTestingDrawQuadEnabled()) {
    // In V1 hit testing, we expect slow path and the submitted region should be
    // equivalent to the unclipped iframe bounds.
    expected_flags = kSlowHitTestFlags;
  } else if (features::IsVizHitTestingSurfaceLayerEnabled()) {
    // In V2 hit testing fast path, we expect precise clipped iframe bounds in
    // its own space.
    expected_transformed_region = gfx::ScaleToEnclosingRect(
        gfx::Rect(100, 100), device_scale_factor, device_scale_factor);
  }

  // Apart from the iframe, it also contains data for root and main frame.
  DCHECK(hit_test_data.size() >= 3);
  EXPECT_TRUE(expected_transformed_region.ApproximatelyEqual(
      AxisAlignedLayoutRectFromHitTest(hit_test_data[2]),
      gfx::ToRoundedInt(device_scale_factor) + 2));
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(hit_test_data[2].transform()));
  EXPECT_EQ(expected_flags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestDataGenerationBrowserTest,
                       RotatedClippedOOPIF) {
  if (!features::IsVizHitTestingEnabled())
    return;
  auto hit_test_data = SetupAndGetHitTestData(
      "/frame_tree/page_with_rotated_clipped_iframe.html");
  float device_scale_factor = current_device_scale_factor();
  // +-Root
  // +---clip1
  // +-----clip2 rotateZ(45)
  // +-------clip3 rotateZ(-45)
  // +---------iframe
  //
  // +----------------300px--------------+
  // |\                                  |
  // |  \                                |
  // |    \                             100px
  // |- x --\                            |
  // |     /                             |
  // +-----------------------------------+
  //
  // Clipped region: x=100/sqrt(2), y=100.
  gfx::Transform expected_transform;
  gfx::Rect expected_region = gfx::ScaleToEnclosingRect(
      gfx::Rect(200, 200), device_scale_factor, device_scale_factor);
  if (!features::IsVizHitTestingDrawQuadEnabled()) {
    expected_region = gfx::ScaleToEnclosingRect(
        gfx::Rect(100 / 1.414, 100), device_scale_factor, device_scale_factor);
  }

  // Compute screen space transform for iframe element, since clip2 is rotated
  // and also clips the iframe, we expect to do slow path hit test on the
  // iframe.
  DCHECK(hit_test_data.size() >= 3);
  EXPECT_TRUE(expected_region.ApproximatelyEqual(hit_test_data[2].rect,
                                                 1 + device_scale_factor));
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(hit_test_data[2].transform()));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestDataGenerationBrowserTest,
                       ClippedRotatedOOPIF) {
  if (!features::IsVizHitTestingEnabled())
    return;
  auto hit_test_data = SetupAndGetHitTestData(
      "/frame_tree/page_with_clipped_rotated_iframe.html");
  float device_scale_factor = current_device_scale_factor();
  // +-Root
  // +---clip1
  // +---------iframe rotateZ(45deg)
  //
  // There are actually 2 clips applied to surface layer, in root space they
  // are:
  // bounding box of clip1: rect 0, 0 300x100, transform = identity;
  // bounding box of iframe itself: rect -100*sqrt(2), 0 200*sqrt(2)x200*sqrt(2)
  // transform: rotateZ(45).
  // In root space the two clips accumulates to:
  //   rect 0, 0 100*sqrt(2)x100, transform=identity
  // Transform this to layer's local space, the clip rect is:
  //   rect 0, -100/sqrt(2) (100+100/sqrt(2))x(100/sqrt(2))
  // So the intersected visible layer rect is:
  //   rect 0, 0, (100+100/sqrt(2)), 100/sqrt(2).
  // +----------------300px--------------+
  // |\                                  |
  // |  \                                |
  // |    \x                            100px
  // |   /  \                            |
  // | /y     \                          |
  // +-----------------------------------+
  gfx::Transform expected_transform;
  expected_transform.RotateAboutZAxis(-45);
  gfx::Rect expected_region = gfx::ScaleToEnclosingRect(
      gfx::Rect(200, 200), device_scale_factor, device_scale_factor);
  if (!features::IsVizHitTestingDrawQuadEnabled()) {
    expected_region =
        gfx::ScaleToEnclosingRect(gfx::Rect(100 + 100 / 1.414f, 100 / 1.414f),
                                  device_scale_factor, device_scale_factor);
  }

  // Since iframe is clipped into an octagon, we expect to do slow path hit
  // test on the iframe.
  DCHECK(hit_test_data.size() >= 3);
  EXPECT_TRUE(expected_region.ApproximatelyEqual(hit_test_data[2].rect,
                                                 1 + device_scale_factor));
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(hit_test_data[2].transform()));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestDataGenerationBrowserTest,
                       ClipPathOOPIF) {
  if (!features::IsVizHitTestingEnabled())
    return;
  auto hit_test_data =
      SetupAndGetHitTestData("/frame_tree/page_with_clip_path_iframe.html");
  float device_scale_factor = current_device_scale_factor();
  gfx::Transform expected_transform;
  gfx::Rect expected_region = gfx::ScaleToEnclosingRect(
      gfx::Rect(100, 100), device_scale_factor, device_scale_factor);
  if (!features::IsVizHitTestingDrawQuadEnabled()) {
    expected_region = gfx::ScaleToEnclosingRect(
        gfx::Rect(80, 80), device_scale_factor, device_scale_factor);
  }

  // Since iframe is clipped into an octagon, we expect to do slow path hit
  // test on the iframe.
  DCHECK(hit_test_data.size() >= 3);
  EXPECT_TRUE(expected_region.ApproximatelyEqual(hit_test_data[2].rect,
                                                 1 + device_scale_factor));
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(hit_test_data[2].transform()));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestDataGenerationBrowserTest,
                       OverlappedOOPIF) {
  if (!features::IsVizHitTestingEnabled())
    return;
  auto hit_test_data =
      SetupAndGetHitTestData("/frame_tree/page_with_overlapped_iframes.html");
  float device_scale_factor = current_device_scale_factor();
  gfx::Transform expected_transform1;
  gfx::Transform expected_transform2;
  expected_transform2.matrix().postTranslate(-100 * device_scale_factor, 0, 0);

  gfx::Rect expected_region = gfx::ScaleToEnclosingRect(
      gfx::Rect(100, 100), device_scale_factor, device_scale_factor);

  // Since iframe is occluded by a div in parent frame, we expect to do slow hit
  // test.
  DCHECK(hit_test_data.size() >= 4);
  EXPECT_EQ(expected_region.ToString(), hit_test_data[3].rect.ToString());
  EXPECT_TRUE(
      expected_transform1.ApproximatelyEqual(hit_test_data[3].transform()));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[3].flags);
  EXPECT_EQ(expected_region.ToString(), hit_test_data[2].rect.ToString());
  EXPECT_TRUE(
      expected_transform2.ApproximatelyEqual(hit_test_data[2].transform()));
  if (features::IsVizHitTestingDrawQuadEnabled())
    EXPECT_EQ(kSlowHitTestFlags, hit_test_data[2].flags);
  else if (features::IsVizHitTestingSurfaceLayerEnabled())
    EXPECT_EQ(kFastHitTestFlags, hit_test_data[2].flags);
}

IN_PROC_BROWSER_TEST_P(SitePerProcessHitTestDataGenerationBrowserTest,
                       MaskedOOPIF) {
  if (!features::IsVizHitTestingEnabled())
    return;
  auto hit_test_data =
      SetupAndGetHitTestData("/frame_tree/page_with_masked_iframe.html");
  float device_scale_factor = current_device_scale_factor();
  gfx::Transform expected_transform;
  gfx::Rect expected_region = gfx::ScaleToEnclosingRect(
      gfx::Rect(200, 200), device_scale_factor, device_scale_factor);

  // Since iframe clipped by clip-path and has a mask layer, we expect to do
  // slow path hit testing.
  DCHECK(hit_test_data.size() >= 3);
  EXPECT_EQ(expected_region.ToString(), hit_test_data[2].rect.ToString());
  EXPECT_TRUE(
      expected_transform.ApproximatelyEqual(hit_test_data[2].transform()));
  EXPECT_EQ(kSlowHitTestFlags, hit_test_data[2].flags);
}

static const int kHitTestOption[] = {0, 1, 2};
static const float kOneScale[] = {1.f};

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        SitePerProcessHitTestBrowserTest,
                        testing::Combine(testing::ValuesIn(kHitTestOption),
                                         testing::ValuesIn(kOneScale)));
// TODO(wjmaclean): Since the next two test fixtures only differ in DSF
// values, should we combine them into one using kMultiScale? This
// approach would make it more difficult to disable individual scales on
// particular platforms.
INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        SitePerProcessHighDPIHitTestBrowserTest,
                        testing::Combine(testing::ValuesIn(kHitTestOption),
                                         testing::ValuesIn(kOneScale)));
INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        SitePerProcessNonIntegerScaleFactorHitTestBrowserTest,
                        testing::Combine(testing::ValuesIn(kHitTestOption),
                                         testing::ValuesIn(kOneScale)));
INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        SitePerProcessEmulatedTouchBrowserTest,
                        testing::Combine(testing::ValuesIn(kHitTestOption),
                                         testing::ValuesIn(kOneScale)));

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        SitePerProcessHitTestDataGenerationBrowserTest,
                        testing::Combine(testing::ValuesIn(kHitTestOption),
                                         testing::ValuesIn(kOneScale)));
#if defined(USE_AURA)
static const float kMultiScale[] = {1.f, 1.5f, 2.f};

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        SitePerProcessInternalsHitTestBrowserTest,
                        testing::Combine(testing::ValuesIn(kHitTestOption),
                                         testing::ValuesIn(kMultiScale)));
INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        SitePerProcessMouseWheelHitTestBrowserTest,
                        testing::Combine(testing::ValuesIn(kHitTestOption),
                                         testing::ValuesIn(kOneScale)));
INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        SitePerProcessGestureHitTestBrowserTest,
                        testing::Combine(testing::ValuesIn(kHitTestOption),
                                         testing::ValuesIn(kOneScale)));
#endif

}  // namespace content
