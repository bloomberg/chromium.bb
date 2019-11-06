// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/visual_viewport.h"

#include <memory>

#include "cc/layers/picture_layer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_ax_context.h"
#include "third_party/blink/public/web/web_context_menu_data.h"
#include "third_party/blink/public/web/web_device_emulation_params.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/geometry/double_point.h"
#include "third_party/blink/renderer/platform/geometry/double_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

#include <string>

using testing::_;
using testing::PrintToString;
using testing::Mock;
using testing::UnorderedElementsAre;
using blink::url_test_helpers::ToKURL;

namespace blink {

::std::ostream& operator<<(::std::ostream& os, const WebContextMenuData& data) {
  return os << "Context menu location: [" << data.mouse_position.x << ", "
            << data.mouse_position.y << "]";
}

namespace {

void ConfigureAndroidCompositing(WebSettings* settings) {
  settings->SetPreferCompositingToLCDTextEnabled(true);
  settings->SetViewportMetaEnabled(true);
  settings->SetViewportEnabled(true);
  settings->SetMainFrameResizesAreOrientationChanges(true);
  settings->SetShrinksViewportContentToFit(true);
}

class VisualViewportTest : public testing::Test,
                           public PaintTestConfigurations {
 public:
  VisualViewportTest() : base_url_("http://www.test.com/") {}

  void InitializeWithDesktopSettings(
      void (*override_settings_func)(WebSettings*) = nullptr) {
    if (!override_settings_func)
      override_settings_func = &ConfigureSettings;
    helper_.Initialize(nullptr, nullptr, nullptr, override_settings_func);
    WebView()->SetDefaultPageScaleLimits(1, 4);
  }

  void InitializeWithAndroidSettings(
      void (*override_settings_func)(WebSettings*) = nullptr) {
    if (!override_settings_func)
      override_settings_func = &ConfigureAndroidSettings;
    helper_.Initialize(nullptr, nullptr, nullptr, override_settings_func);
    WebView()->SetDefaultPageScaleLimits(0.25f, 5);
  }

  ~VisualViewportTest() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void NavigateTo(const std::string& url) {
    frame_test_helpers::LoadFrame(WebView()->MainFrameImpl(), url);
  }

  void UpdateAllLifecyclePhases() {
    WebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
  }

  void UpdateAllLifecyclePhasesExceptPaint() {
    WebView()->MainFrameWidget()->UpdateLifecycle(
        WebWidget::LifecycleUpdate::kPrePaint,
        WebWidget::LifecycleUpdateReason::kTest);
  }

  PaintArtifactCompositor* paint_artifact_compositor() {
    LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();
    return frame_view.GetPaintArtifactCompositorForTesting();
  }

  void ForceFullCompositingUpdate() { UpdateAllLifecyclePhases(); }

  void RegisterMockedHttpURLLoad(const std::string& fileName) {
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), blink::test::CoreTestDataPath(),
        WebString::FromUTF8(fileName));
  }

  void RegisterMockedHttpURLLoad(const std::string& url,
                                 const std::string& fileName) {
    url_test_helpers::RegisterMockedURLLoad(
        ToKURL(url),
        blink::test::CoreTestDataPath(WebString::FromUTF8(fileName)));
  }

  WebViewImpl* WebView() const { return helper_.GetWebView(); }
  LocalFrame* GetFrame() const { return helper_.LocalMainFrame()->GetFrame(); }

  static void ConfigureSettings(WebSettings* settings) {
    settings->SetJavaScriptEnabled(true);
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  static void ConfigureAndroidSettings(WebSettings* settings) {
    ConfigureSettings(settings);
    settings->SetViewportEnabled(true);
    settings->SetViewportMetaEnabled(true);
    settings->SetShrinksViewportContentToFit(true);
    settings->SetMainFrameResizesAreOrientationChanges(true);
  }

  const GraphicsLayer* MainGraphicsLayer(const Document* document) {
    const auto* layer = document->GetLayoutView()->Layer();
    return layer->GetCompositedLayerMapping()
               ? layer->GetCompositedLayerMapping()->MainGraphicsLayer()
               : nullptr;
  }

  const GraphicsLayer* ScrollingContentsLayer(const Document* document) {
    const auto* layer = document->GetLayoutView()->Layer();
    return layer->GetCompositedLayerMapping()
               ? layer->GetCompositedLayerMapping()->ScrollingContentsLayer()
               : nullptr;
  }

  const DisplayItemClient& ScrollingBackgroundClient(const Document* document) {
    return document->GetLayoutView()
        ->GetScrollableArea()
        ->GetScrollingBackgroundDisplayItemClient();
  }

  const RasterInvalidationTracking* MainGraphicsLayerRasterInvalidationTracking(
      const Document* document) {
    const auto* layer = MainGraphicsLayer(document);
    return layer ? layer->GetRasterInvalidationTracking() : nullptr;
  }

  const RasterInvalidationTracking*
  ScrollingContentsLayerRasterInvalidationTracking(const Document* document) {
    const auto* layer = ScrollingContentsLayer(document);
    return layer ? layer->GetRasterInvalidationTracking() : nullptr;
  }

  bool MainGraphicsLayerHasRasterInvalidations(const Document* document) {
    const auto* tracking =
        MainGraphicsLayerRasterInvalidationTracking(document);
    return tracking && tracking->HasInvalidations();
  }

  bool ScrollingContentsLayerHasRasterInvalidations(const Document* document) {
    const auto* tracking =
        ScrollingContentsLayerRasterInvalidationTracking(document);
    return tracking && tracking->HasInvalidations();
  }

  const Vector<RasterInvalidationInfo>& MainGraphicsLayerRasterInvalidations(
      const Document* document) {
    DCHECK(MainGraphicsLayerHasRasterInvalidations(document));
    return MainGraphicsLayerRasterInvalidationTracking(document)
        ->Invalidations();
  }

  const Vector<RasterInvalidationInfo>&
  ScrollingContentsLayerRasterInvalidations(const Document* document) {
    DCHECK(ScrollingContentsLayerHasRasterInvalidations(document));
    return ScrollingContentsLayerRasterInvalidationTracking(document)
        ->Invalidations();
  }

 protected:
  std::string base_url_;
  frame_test_helpers::WebViewHelper helper_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(VisualViewportTest);

// Test that resizing the VisualViewport works as expected and that resizing the
// WebView resizes the VisualViewport.
TEST_P(VisualViewportTest, TestResize) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(320, 240));

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  IntSize web_view_size = WebView()->MainFrameWidget()->Size();

  // Make sure the visual viewport was initialized.
  EXPECT_EQ(web_view_size, visual_viewport.Size());

  // Resizing the WebView should change the VisualViewport.
  web_view_size = IntSize(640, 480);
  WebView()->MainFrameWidget()->Resize(web_view_size);
  EXPECT_EQ(web_view_size, IntSize(WebView()->MainFrameWidget()->Size()));
  EXPECT_EQ(web_view_size, visual_viewport.Size());

  // Resizing the visual viewport shouldn't affect the WebView.
  IntSize new_viewport_size = IntSize(320, 200);
  visual_viewport.SetSize(new_viewport_size);
  EXPECT_EQ(web_view_size, IntSize(WebView()->MainFrameWidget()->Size()));
  EXPECT_EQ(new_viewport_size, visual_viewport.Size());
}

// Make sure that the visibleContentRect method acurately reflects the scale and
// scroll location of the viewport with and without scrollbars.
TEST_P(VisualViewportTest, TestVisibleContentRect) {
  ScopedOverlayScrollbarsForTest overlay_scrollbars(false);
  InitializeWithDesktopSettings();

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  IntSize size = IntSize(150, 100);
  // Vertical scrollbar width and horizontal scrollbar height.
  IntSize scrollbar_size = IntSize(15, 15);

  WebView()->MainFrameWidget()->Resize(size);

  // Scroll layout viewport and verify visibleContentRect.
  WebView()->MainFrameImpl()->SetScrollOffset(WebSize(0, 50));

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_EQ(IntRect(IntPoint(0, 0), size - scrollbar_size),
            visual_viewport.VisibleContentRect(kExcludeScrollbars));
  EXPECT_EQ(IntRect(IntPoint(0, 0), size),
            visual_viewport.VisibleContentRect(kIncludeScrollbars));

  WebView()->SetPageScaleFactor(2.0);

  // Scroll visual viewport and verify visibleContentRect.
  size.Scale(0.5);
  scrollbar_size.Scale(0.5);
  visual_viewport.SetLocation(FloatPoint(10, 10));
  EXPECT_EQ(IntRect(IntPoint(10, 10), size - scrollbar_size),
            visual_viewport.VisibleContentRect(kExcludeScrollbars));
  EXPECT_EQ(IntRect(IntPoint(10, 10), size),
            visual_viewport.VisibleContentRect(kIncludeScrollbars));
}

// This tests that shrinking the WebView while the page is fully scrolled
// doesn't move the viewport up/left, it should keep the visible viewport
// unchanged from the user's perspective (shrinking the LocalFrameView will
// clamp the VisualViewport so we need to counter scroll the LocalFrameView to
// make it appear to stay still). This caused bugs like crbug.com/453859.
TEST_P(VisualViewportTest, TestResizeAtFullyScrolledPreservesViewportLocation) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(800, 600));

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  visual_viewport.SetScale(2);

  // Fully scroll both viewports.
  frame_view.LayoutViewport()->SetScrollOffset(ScrollOffset(10000, 10000),
                                               kProgrammaticScroll);
  visual_viewport.Move(FloatSize(10000, 10000));

  // Sanity check.
  ASSERT_EQ(FloatSize(400, 300), visual_viewport.GetScrollOffset());
  ASSERT_EQ(ScrollOffset(200, 1400),
            frame_view.LayoutViewport()->GetScrollOffset());

  IntPoint expected_location =
      frame_view.GetScrollableArea()->VisibleContentRect().Location();

  // Shrink the WebView, this should cause both viewports to shrink and
  // WebView should do whatever it needs to do to preserve the visible
  // location.
  WebView()->MainFrameWidget()->Resize(IntSize(700, 550));

  EXPECT_EQ(expected_location,
            frame_view.GetScrollableArea()->VisibleContentRect().Location());

  WebView()->MainFrameWidget()->Resize(IntSize(800, 600));

  EXPECT_EQ(expected_location,
            frame_view.GetScrollableArea()->VisibleContentRect().Location());
}

// Test that the VisualViewport works as expected in case of a scaled
// and scrolled viewport - scroll down.
TEST_P(VisualViewportTest, TestResizeAfterVerticalScroll) {
  /*
                 200                                 200
        |                   |               |                   |
        |                   |               |                   |
        |                   | 800           |                   | 800
        |-------------------|               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |   -------->   |                   |
        | 300               |               |                   |
        |                   |               |                   |
        |               400 |               |                   |
        |                   |               |-------------------|
        |                   |               |      75           |
        | 50                |               | 50             100|
        o-----              |               o----               |
        |    |              |               |   |  25           |
        |    |100           |               |-------------------|
        |    |              |               |                   |
        |    |              |               |                   |
        --------------------                --------------------

     */
  InitializeWithAndroidSettings();

  RegisterMockedHttpURLLoad("200-by-800-viewport.html");
  NavigateTo(base_url_ + "200-by-800-viewport.html");

  WebView()->MainFrameWidget()->Resize(IntSize(100, 200));

  // Scroll main frame to the bottom of the document
  WebView()->MainFrameImpl()->SetScrollOffset(WebSize(0, 400));
  EXPECT_EQ(ScrollOffset(0, 400),
            GetFrame()->View()->LayoutViewport()->GetScrollOffset());

  WebView()->SetPageScaleFactor(2.0);

  // Scroll visual viewport to the bottom of the main frame
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetLocation(FloatPoint(0, 300));
  EXPECT_FLOAT_SIZE_EQ(FloatSize(0, 300), visual_viewport.GetScrollOffset());

  // Verify the initial size of the visual viewport in the CSS pixels
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 100),
                       visual_viewport.VisibleRect().Size());

  // Verify the paint property nodes and GeometryMapper cache.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
      RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    UpdateAllLifecyclePhases();
    EXPECT_EQ(TransformationMatrix().Scale(2),
              visual_viewport.GetPageScaleNode()->Matrix());
    EXPECT_EQ(FloatSize(0, -300),
              visual_viewport.GetScrollTranslationNode()->Translation2D());
    EXPECT_EQ(TransformationMatrix().Scale(2).Translate(0, -300),
              GeometryMapper::SourceToDestinationProjection(
                  *visual_viewport.GetScrollTranslationNode(),
                  TransformPaintPropertyNode::Root())
                  .Matrix());
  }

  // Perform the resizing
  WebView()->MainFrameWidget()->Resize(IntSize(200, 100));

  // After resizing the scale changes 2.0 -> 4.0
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 25), visual_viewport.VisibleRect().Size());

  EXPECT_EQ(ScrollOffset(0, 625),
            GetFrame()->View()->LayoutViewport()->GetScrollOffset());
  EXPECT_FLOAT_SIZE_EQ(FloatSize(0, 75), visual_viewport.GetScrollOffset());

  // Verify the paint property nodes and GeometryMapper cache.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
      RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    UpdateAllLifecyclePhases();
    EXPECT_EQ(TransformationMatrix().Scale(4),
              visual_viewport.GetPageScaleNode()->Matrix());
    EXPECT_EQ(FloatSize(0, -75),
              visual_viewport.GetScrollTranslationNode()->Translation2D());
    EXPECT_EQ(TransformationMatrix().Scale(4).Translate(0, -75),
              GeometryMapper::SourceToDestinationProjection(
                  *visual_viewport.GetScrollTranslationNode(),
                  TransformPaintPropertyNode::Root())
                  .Matrix());
  }
}

// Test that the VisualViewport works as expected in case if a scaled
// and scrolled viewport - scroll right.
TEST_P(VisualViewportTest, TestResizeAfterHorizontalScroll) {
  /*
                 200                                 200
        ---------------o-----               ---------------o-----
        |              |    |               |            25|    |
        |              |    |               |              -----|
        |           100|    |               |100             50 |
        |              |    |               |                   |
        |              ---- |               |-------------------|
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |400                |   --------->  |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |                   |               |                   |
        |-------------------|               |                   |
        |                   |               |                   |

     */
  InitializeWithAndroidSettings();

  RegisterMockedHttpURLLoad("200-by-800-viewport.html");
  NavigateTo(base_url_ + "200-by-800-viewport.html");

  WebView()->MainFrameWidget()->Resize(IntSize(100, 200));

  // Outer viewport takes the whole width of the document.

  WebView()->SetPageScaleFactor(2.0);

  // Scroll visual viewport to the right edge of the frame
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetLocation(FloatPoint(150, 0));
  EXPECT_FLOAT_SIZE_EQ(FloatSize(150, 0), visual_viewport.GetScrollOffset());

  // Verify the initial size of the visual viewport in the CSS pixels
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 100),
                       visual_viewport.VisibleRect().Size());

  // Verify the paint property nodes and GeometryMapper cache.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
      RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    UpdateAllLifecyclePhases();
    EXPECT_EQ(TransformationMatrix().Scale(2),
              visual_viewport.GetPageScaleNode()->Matrix());
    EXPECT_EQ(FloatSize(-150, 0),
              visual_viewport.GetScrollTranslationNode()->Translation2D());
    EXPECT_EQ(TransformationMatrix().Scale(2).Translate(-150, 0),
              GeometryMapper::SourceToDestinationProjection(
                  *visual_viewport.GetScrollTranslationNode(),
                  TransformPaintPropertyNode::Root())
                  .Matrix());
  }

  WebView()->MainFrameWidget()->Resize(IntSize(200, 100));

  // After resizing the scale changes 2.0 -> 4.0
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 25), visual_viewport.VisibleRect().Size());

  EXPECT_EQ(ScrollOffset(0, 0),
            GetFrame()->View()->LayoutViewport()->GetScrollOffset());
  EXPECT_FLOAT_SIZE_EQ(FloatSize(150, 0), visual_viewport.GetScrollOffset());

  // Verify the paint property nodes and GeometryMapper cache.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
      RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    UpdateAllLifecyclePhases();
    EXPECT_EQ(TransformationMatrix().Scale(4),
              visual_viewport.GetPageScaleNode()->Matrix());
    EXPECT_EQ(FloatSize(-150, 0),
              visual_viewport.GetScrollTranslationNode()->Translation2D());
    EXPECT_EQ(TransformationMatrix().Scale(4).Translate(-150, 0),
              GeometryMapper::SourceToDestinationProjection(
                  *visual_viewport.GetScrollTranslationNode(),
                  TransformPaintPropertyNode::Root())
                  .Matrix());
  }
}

// Test that the container layer gets sized properly if the WebView is resized
// prior to the VisualViewport being attached to the layer tree.
TEST_P(VisualViewportTest, TestWebViewResizedBeforeAttachment) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  InitializeWithDesktopSettings();
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  // Make sure that a resize that comes in while there's no root layer is
  // honoured when we attach to the layer tree.
  WebFrameWidgetBase* main_frame_widget =
      WebView()->MainFrameImpl()->FrameWidgetImpl();
  main_frame_widget->SetRootGraphicsLayer(nullptr);
  WebView()->MainFrameWidget()->Resize(IntSize(320, 240));

  NavigateTo("about:blank");
  UpdateAllLifecyclePhases();
  main_frame_widget->SetRootGraphicsLayer(
      frame_view.GetLayoutView()->Compositor()->RootGraphicsLayer());

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_FLOAT_SIZE_EQ(IntSize(320, 240),
                       IntSize(visual_viewport.ContainerLayer()->Size()));

  if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    EXPECT_EQ(IntSize(320, 240),
              visual_viewport.GetScrollNode()->ContainerRect().Size());
  }
}

// Make sure that the visibleRect method acurately reflects the scale and scroll
// location of the viewport.
TEST_P(VisualViewportTest, TestVisibleRect) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(320, 240));

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  // Initial visible rect should be the whole frame.
  EXPECT_EQ(IntSize(WebView()->MainFrameWidget()->Size()),
            visual_viewport.Size());

  // Viewport is whole frame.
  IntSize size = IntSize(400, 200);
  WebView()->MainFrameWidget()->Resize(size);
  UpdateAllLifecyclePhases();
  visual_viewport.SetSize(size);

  // Scale the viewport to 2X; size should not change.
  FloatRect expected_rect(FloatPoint(0, 0), FloatSize(size));
  expected_rect.Scale(0.5);
  visual_viewport.SetScale(2);
  EXPECT_EQ(2, visual_viewport.Scale());
  EXPECT_EQ(size, visual_viewport.Size());
  EXPECT_FLOAT_RECT_EQ(expected_rect, visual_viewport.VisibleRect());

  // Move the viewport.
  expected_rect.SetLocation(FloatPoint(5, 7));
  visual_viewport.SetLocation(expected_rect.Location());
  EXPECT_FLOAT_RECT_EQ(expected_rect, visual_viewport.VisibleRect());

  expected_rect.SetLocation(FloatPoint(200, 100));
  visual_viewport.SetLocation(expected_rect.Location());
  EXPECT_FLOAT_RECT_EQ(expected_rect, visual_viewport.VisibleRect());

  // Scale the viewport to 3X to introduce some non-int values.
  FloatPoint oldLocation = expected_rect.Location();
  expected_rect = FloatRect(FloatPoint(), FloatSize(size));
  expected_rect.Scale(1 / 3.0f);
  expected_rect.SetLocation(oldLocation);
  visual_viewport.SetScale(3);
  EXPECT_FLOAT_RECT_EQ(expected_rect, visual_viewport.VisibleRect());

  expected_rect.SetLocation(FloatPoint(0.25f, 0.333f));
  visual_viewport.SetLocation(expected_rect.Location());
  EXPECT_FLOAT_RECT_EQ(expected_rect, visual_viewport.VisibleRect());
}

TEST_P(VisualViewportTest, TestFractionalScrollOffsetIsNotOverwritten) {
  ScopedFractionalScrollOffsetsForTest fractional_scroll_offsets(true);
  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(200, 250));

  RegisterMockedHttpURLLoad("200-by-800-viewport.html");
  NavigateTo(base_url_ + "200-by-800-viewport.html");

  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();
  frame_view.LayoutViewport()->SetScrollOffset(ScrollOffset(0, 10.5),
                                               kProgrammaticScroll);
  frame_view.LayoutViewport()->ScrollableArea::SetScrollOffset(
      ScrollOffset(10, 30.5), kCompositorScroll);

  EXPECT_EQ(30.5, frame_view.LayoutViewport()->GetScrollOffset().Height());
}

// Test that the viewport's scroll offset is always appropriately bounded such
// that the visual viewport always stays within the bounds of the main frame.
TEST_P(VisualViewportTest, TestOffsetClamping) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(320, 240));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(
      WebView()->MainFrameImpl(),
      "<!DOCTYPE html>"
      "<meta name='viewport' content='width=2000'>",
      base_url);
  ForceFullCompositingUpdate();

  // Visual viewport should be initialized to same size as frame so no scrolling
  // possible. At minimum scale, the viewport is 1280x960.
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  ASSERT_EQ(0.25, visual_viewport.Scale());
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());

  visual_viewport.SetLocation(FloatPoint(-1, -2));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());

  visual_viewport.SetLocation(FloatPoint(100, 200));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());

  visual_viewport.SetLocation(FloatPoint(-5, 10));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());

  // Scale to 2x. The viewport's visible rect should now have a size of 160x120.
  visual_viewport.SetScale(2);
  FloatPoint location(10, 50);
  visual_viewport.SetLocation(location);
  EXPECT_FLOAT_POINT_EQ(location, visual_viewport.VisibleRect().Location());

  visual_viewport.SetLocation(FloatPoint(10000, 10000));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(1120, 840),
                        visual_viewport.VisibleRect().Location());

  visual_viewport.SetLocation(FloatPoint(-2000, -2000));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());

  // Make sure offset gets clamped on scale out. Scale to 1.25 so the viewport
  // is 256x192.
  visual_viewport.SetLocation(FloatPoint(1120, 840));
  visual_viewport.SetScale(1.25);
  EXPECT_FLOAT_POINT_EQ(FloatPoint(1024, 768),
                        visual_viewport.VisibleRect().Location());

  // Scale out smaller than 1.
  visual_viewport.SetScale(0.25);
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());
}

// Test that the viewport can be scrolled around only within the main frame in
// the presence of viewport resizes, as would be the case if the on screen
// keyboard came up.
TEST_P(VisualViewportTest, TestOffsetClampingWithResize) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(320, 240));

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  // Visual viewport should be initialized to same size as frame so no scrolling
  // possible.
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());

  // Shrink the viewport vertically. The resize shouldn't affect the location,
  // but it should allow vertical scrolling.
  visual_viewport.SetSize(IntSize(320, 200));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());
  visual_viewport.SetLocation(FloatPoint(10, 20));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 20),
                        visual_viewport.VisibleRect().Location());
  visual_viewport.SetLocation(FloatPoint(0, 100));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 40),
                        visual_viewport.VisibleRect().Location());
  visual_viewport.SetLocation(FloatPoint(0, 10));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 10),
                        visual_viewport.VisibleRect().Location());
  visual_viewport.SetLocation(FloatPoint(0, -100));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());

  // Repeat the above but for horizontal dimension.
  visual_viewport.SetSize(IntSize(280, 240));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());
  visual_viewport.SetLocation(FloatPoint(10, 20));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 0),
                        visual_viewport.VisibleRect().Location());
  visual_viewport.SetLocation(FloatPoint(100, 0));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(40, 0),
                        visual_viewport.VisibleRect().Location());
  visual_viewport.SetLocation(FloatPoint(10, 0));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 0),
                        visual_viewport.VisibleRect().Location());
  visual_viewport.SetLocation(FloatPoint(-100, 0));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());

  // Now with both dimensions.
  visual_viewport.SetSize(IntSize(280, 200));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());
  visual_viewport.SetLocation(FloatPoint(10, 20));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 20),
                        visual_viewport.VisibleRect().Location());
  visual_viewport.SetLocation(FloatPoint(100, 100));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(40, 40),
                        visual_viewport.VisibleRect().Location());
  visual_viewport.SetLocation(FloatPoint(10, 3));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 3),
                        visual_viewport.VisibleRect().Location());
  visual_viewport.SetLocation(FloatPoint(-10, -4));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());
}

// Test that the viewport is scrollable but bounded appropriately within the
// main frame when we apply both scaling and resizes.
TEST_P(VisualViewportTest, TestOffsetClampingWithResizeAndScale) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(320, 240));

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  // Visual viewport should be initialized to same size as WebView so no
  // scrolling possible.
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());

  // Zoom in to 2X so we can scroll the viewport to 160x120.
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(FloatPoint(200, 200));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(160, 120),
                        visual_viewport.VisibleRect().Location());

  // Now resize the viewport to make it 10px smaller. Since we're zoomed in by
  // 2X it should allow us to scroll by 5px more.
  visual_viewport.SetSize(IntSize(310, 230));
  visual_viewport.SetLocation(FloatPoint(200, 200));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(165, 125),
                        visual_viewport.VisibleRect().Location());

  // The viewport can be larger than the main frame (currently 320, 240) though
  // typically the scale will be clamped to prevent it from actually being
  // larger.
  visual_viewport.SetSize(IntSize(330, 250));
  EXPECT_EQ(IntSize(330, 250), visual_viewport.Size());

  // Resize both the viewport and the frame to be larger.
  WebView()->MainFrameWidget()->Resize(IntSize(640, 480));
  UpdateAllLifecyclePhases();
  EXPECT_EQ(IntSize(WebView()->MainFrameWidget()->Size()),
            visual_viewport.Size());
  EXPECT_EQ(IntSize(WebView()->MainFrameWidget()->Size()),
            GetFrame()->View()->FrameRect().Size());
  visual_viewport.SetLocation(FloatPoint(1000, 1000));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(320, 240),
                        visual_viewport.VisibleRect().Location());

  // Make sure resizing the viewport doesn't change its offset if the resize
  // doesn't make the viewport go out of bounds.
  visual_viewport.SetLocation(FloatPoint(200, 200));
  visual_viewport.SetSize(IntSize(880, 560));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(200, 200),
                        visual_viewport.VisibleRect().Location());
}

// The main LocalFrameView's size should be set such that its the size of the
// visual viewport at minimum scale. If there's no explicit minimum scale set,
// the LocalFrameView should be set to the content width and height derived by
// the aspect ratio.
TEST_P(VisualViewportTest, TestFrameViewSizedToContent) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(320, 240));

  RegisterMockedHttpURLLoad("200-by-300-viewport.html");
  NavigateTo(base_url_ + "200-by-300-viewport.html");

  WebView()->MainFrameWidget()->Resize(IntSize(600, 800));
  UpdateAllLifecyclePhases();

  // Note: the size is ceiled and should match the behavior in CC's
  // LayerImpl::bounds().
  EXPECT_EQ(IntSize(200, 267),
            WebView()->MainFrameImpl()->GetFrameView()->FrameRect().Size());
}

// The main LocalFrameView's size should be set such that its the size of the
// visual viewport at minimum scale. On Desktop, the minimum scale is set at 1
// so make sure the LocalFrameView is sized to the viewport.
TEST_P(VisualViewportTest, TestFrameViewSizedToMinimumScale) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(320, 240));

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  WebView()->MainFrameWidget()->Resize(IntSize(100, 160));
  UpdateAllLifecyclePhases();

  EXPECT_EQ(IntSize(100, 160),
            WebView()->MainFrameImpl()->GetFrameView()->FrameRect().Size());
}

// Test that attaching a new frame view resets the size of the inner viewport
// scroll layer. crbug.com/423189.
TEST_P(VisualViewportTest, TestAttachingNewFrameSetsInnerScrollLayerSize) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(320, 240));

  // Load a wider page first, the navigation should resize the scroll layer to
  // the smaller size on the second navigation.
  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.Move(ScrollOffset(50, 60));

  // Move and scale the viewport to make sure it gets reset in the navigation.
  EXPECT_EQ(FloatSize(50, 60), visual_viewport.GetScrollOffset());
  EXPECT_EQ(2, visual_viewport.Scale());

  // Navigate again, this time the LocalFrameView should be smaller.
  RegisterMockedHttpURLLoad("viewport-device-width.html");
  NavigateTo(base_url_ + "viewport-device-width.html");
  UpdateAllLifecyclePhases();

  // Ensure the scroll contents size matches the frame view's size.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(IntSize(320, 240),
              IntSize(visual_viewport.ScrollLayer()->Size()));
  }
  if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() ||
      RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(IntSize(320, 240),
              visual_viewport.GetScrollNode()->ContentsSize());
  }

  // Ensure the location and scale were reset.
  EXPECT_EQ(FloatSize(), visual_viewport.GetScrollOffset());
  EXPECT_EQ(1, visual_viewport.Scale());
}

// The main LocalFrameView's size should be set such that its the size of the
// visual viewport at minimum scale. Test that the LocalFrameView is
// appropriately sized in the presence of a viewport <meta> tag.
TEST_P(VisualViewportTest, TestFrameViewSizedToViewportMetaMinimumScale) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(320, 240));

  RegisterMockedHttpURLLoad("200-by-300-min-scale-2.html");
  NavigateTo(base_url_ + "200-by-300-min-scale-2.html");

  WebView()->MainFrameWidget()->Resize(IntSize(100, 160));
  UpdateAllLifecyclePhases();

  EXPECT_EQ(IntSize(50, 80),
            WebView()->MainFrameImpl()->GetFrameView()->FrameRect().Size());
}

// Test that the visual viewport still gets sized in AutoSize/AutoResize mode.
TEST_P(VisualViewportTest, TestVisualViewportGetsSizeInAutoSizeMode) {
  InitializeWithDesktopSettings();

  EXPECT_EQ(IntSize(0, 0), IntSize(WebView()->MainFrameWidget()->Size()));
  EXPECT_EQ(IntSize(0, 0), GetFrame()->GetPage()->GetVisualViewport().Size());

  WebView()->EnableAutoResizeMode(WebSize(10, 10), WebSize(1000, 1000));

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  EXPECT_EQ(IntSize(200, 300),
            GetFrame()->GetPage()->GetVisualViewport().Size());
}

// Test that the text selection handle's position accounts for the visual
// viewport.
TEST_P(VisualViewportTest, TestTextSelectionHandles) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(500, 800));

  RegisterMockedHttpURLLoad("pinch-viewport-input-field.html");
  NavigateTo(base_url_ + "pinch-viewport-input-field.html");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  WebView()->SetInitialFocus(false);

  WebRect original_anchor;
  WebRect original_focus;
  WebView()->MainFrameWidget()->SelectionBounds(original_anchor,
                                                original_focus);

  WebView()->SetPageScaleFactor(2);
  visual_viewport.SetLocation(FloatPoint(100, 400));

  WebRect anchor;
  WebRect focus;
  WebView()->MainFrameWidget()->SelectionBounds(anchor, focus);

  IntPoint expected(IntRect(original_anchor).Location());
  expected.MoveBy(-FlooredIntPoint(visual_viewport.VisibleRect().Location()));
  expected.Scale(visual_viewport.Scale(), visual_viewport.Scale());

  EXPECT_EQ(expected, IntRect(anchor).Location());
  EXPECT_EQ(expected, IntRect(focus).Location());

  // FIXME(bokan) - http://crbug.com/364154 - Figure out how to test text
  // selection as well rather than just carret.
}

// Test that the HistoryItem for the page stores the visual viewport's offset
// and scale.
TEST_P(VisualViewportTest, TestSavedToHistoryItem) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(200, 300));
  UpdateAllLifecyclePhases();

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  EXPECT_FALSE(To<LocalFrame>(WebView()->GetPage()->MainFrame())
                   ->Loader()
                   .GetDocumentLoader()
                   ->GetHistoryItem()
                   ->GetViewState()
                   .has_value());

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);

  EXPECT_EQ(2, To<LocalFrame>(WebView()->GetPage()->MainFrame())
                   ->Loader()
                   .GetDocumentLoader()
                   ->GetHistoryItem()
                   ->GetViewState()
                   ->page_scale_factor_);

  visual_viewport.SetLocation(FloatPoint(10, 20));

  EXPECT_EQ(ScrollOffset(10, 20),
            To<LocalFrame>(WebView()->GetPage()->MainFrame())
                ->Loader()
                .GetDocumentLoader()
                ->GetHistoryItem()
                ->GetViewState()
                ->visual_viewport_scroll_offset_);
}

// Test restoring a HistoryItem properly restores the visual viewport's state.
TEST_P(VisualViewportTest, TestRestoredFromHistoryItem) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(200, 300));

  RegisterMockedHttpURLLoad("200-by-300.html");

  WebHistoryItem item;
  item.Initialize();
  WebURL destination_url(
      url_test_helpers::ToKURL(base_url_ + "200-by-300.html"));
  item.SetURLString(destination_url.GetString());
  item.SetVisualViewportScrollOffset(WebFloatPoint(100, 120));
  item.SetPageScaleFactor(2);

  frame_test_helpers::LoadHistoryItem(WebView()->MainFrameImpl(), item,
                                      mojom::FetchCacheMode::kDefault);

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_EQ(2, visual_viewport.Scale());

  EXPECT_FLOAT_POINT_EQ(FloatPoint(100, 120),
                        visual_viewport.VisibleRect().Location());
}

// Test restoring a HistoryItem without the visual viewport offset falls back to
// distributing the scroll offset between the main frame and the visual
// viewport.
TEST_P(VisualViewportTest, TestRestoredFromLegacyHistoryItem) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(100, 150));

  RegisterMockedHttpURLLoad("200-by-300-viewport.html");

  WebHistoryItem item;
  item.Initialize();
  WebURL destination_url(
      url_test_helpers::ToKURL(base_url_ + "200-by-300-viewport.html"));
  item.SetURLString(destination_url.GetString());
  // (-1, -1) will be used if the HistoryItem is an older version prior to
  // having visual viewport scroll offset.
  item.SetVisualViewportScrollOffset(WebFloatPoint(-1, -1));
  item.SetScrollOffset(WebPoint(120, 180));
  item.SetPageScaleFactor(2);

  frame_test_helpers::LoadHistoryItem(WebView()->MainFrameImpl(), item,
                                      mojom::FetchCacheMode::kDefault);

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_EQ(2, visual_viewport.Scale());
  EXPECT_EQ(ScrollOffset(100, 150),
            GetFrame()->View()->LayoutViewport()->GetScrollOffset());
  EXPECT_FLOAT_POINT_EQ(FloatPoint(20, 30),
                        visual_viewport.VisibleRect().Location());
}

// Test that navigation to a new page with a different sized main frame doesn't
// clobber the history item's main frame scroll offset. crbug.com/371867
TEST_P(VisualViewportTest,
       TestNavigateToSmallerFrameViewHistoryItemClobberBug) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(400, 400));
  UpdateAllLifecyclePhases();

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  LocalFrameView* frame_view = WebView()->MainFrameImpl()->GetFrameView();
  frame_view->LayoutViewport()->SetScrollOffset(ScrollOffset(0, 1000),
                                                kProgrammaticScroll);

  EXPECT_EQ(IntSize(1000, 1000), frame_view->FrameRect().Size());

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(FloatPoint(350, 350));

  Persistent<HistoryItem> firstItem = WebView()
                                          ->MainFrameImpl()
                                          ->GetFrame()
                                          ->Loader()
                                          .GetDocumentLoader()
                                          ->GetHistoryItem();
  EXPECT_EQ(ScrollOffset(0, 1000), firstItem->GetViewState()->scroll_offset_);

  // Now navigate to a page which causes a smaller frame_view. Make sure that
  // navigating doesn't cause the history item to set a new scroll offset
  // before the item was replaced.
  NavigateTo("about:blank");
  frame_view = WebView()->MainFrameImpl()->GetFrameView();

  EXPECT_NE(firstItem, WebView()
                           ->MainFrameImpl()
                           ->GetFrame()
                           ->Loader()
                           .GetDocumentLoader()
                           ->GetHistoryItem());
  EXPECT_LT(frame_view->FrameRect().Size().Width(), 1000);
  EXPECT_EQ(ScrollOffset(0, 1000), firstItem->GetViewState()->scroll_offset_);
}

// Test that the coordinates sent into moveRangeSelection are offset by the
// visual viewport's location.
TEST_P(VisualViewportTest,
       DISABLED_TestWebFrameRangeAccountsForVisualViewportScroll) {
  InitializeWithDesktopSettings();
  WebView()->GetSettings()->SetDefaultFontSize(12);
  WebView()->MainFrameWidget()->Resize(WebSize(640, 480));
  RegisterMockedHttpURLLoad("move_range.html");
  NavigateTo(base_url_ + "move_range.html");

  WebRect base_rect;
  WebRect extent_rect;

  WebView()->SetPageScaleFactor(2);
  WebLocalFrame* mainFrame = WebView()->MainFrameImpl();

  // Select some text and get the base and extent rects (that's the start of
  // the range and its end). Do a sanity check that the expected text is
  // selected
  mainFrame->ExecuteScript(WebScriptSource("selectRange();"));
  EXPECT_EQ("ir", mainFrame->SelectionAsText().Utf8());

  WebView()->MainFrameWidget()->SelectionBounds(base_rect, extent_rect);
  WebPoint initialPoint(base_rect.x, base_rect.y);
  WebPoint endPoint(extent_rect.x, extent_rect.y);

  // Move the visual viewport over and make the selection in the same
  // screen-space location. The selection should change to two characters to the
  // right and down one line.
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.Move(ScrollOffset(60, 25));
  mainFrame->MoveRangeSelection(initialPoint, endPoint);
  EXPECT_EQ("t ", mainFrame->SelectionAsText().Utf8());
}

// Test that resizing the WebView causes ViewportConstrained objects to
// relayout.
TEST_P(VisualViewportTest, TestWebViewResizeCausesViewportConstrainedLayout) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(500, 300));

  RegisterMockedHttpURLLoad("pinch-viewport-fixed-pos.html");
  NavigateTo(base_url_ + "pinch-viewport-fixed-pos.html");

  LayoutObject* navbar =
      GetFrame()->GetDocument()->getElementById("navbar")->GetLayoutObject();

  EXPECT_FALSE(navbar->NeedsLayout());

  GetFrame()->View()->Resize(IntSize(500, 200));

  EXPECT_TRUE(navbar->NeedsLayout());
}

class VisualViewportMockWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  MOCK_METHOD1(ShowContextMenu, void(const WebContextMenuData&));
  MOCK_METHOD0(DidChangeScrollOffset, void());
};

MATCHER_P2(ContextMenuAtLocation,
           x,
           y,
           std::string(negation ? "is" : "isn't") + " at expected location [" +
               PrintToString(x) + ", " + PrintToString(y) + "]") {
  return arg.mouse_position.x == x && arg.mouse_position.y == y;
}

// Test that the context menu's location is correct in the presence of visual
// viewport offset.
TEST_P(VisualViewportTest, TestContextMenuShownInCorrectLocation) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(200, 300));

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  WebMouseEvent mouse_down_event(WebInputEvent::kMouseDown,
                                 WebInputEvent::kNoModifiers,
                                 WebInputEvent::GetStaticTimeStampForTests());
  mouse_down_event.SetPositionInWidget(10, 10);
  mouse_down_event.SetPositionInScreen(110, 210);
  mouse_down_event.click_count = 1;
  mouse_down_event.button = WebMouseEvent::Button::kRight;

  // Corresponding release event (Windows shows context menu on release).
  WebMouseEvent mouse_up_event(mouse_down_event);
  mouse_up_event.SetType(WebInputEvent::kMouseUp);

  WebLocalFrameClient* old_client = WebView()->MainFrameImpl()->Client();
  VisualViewportMockWebFrameClient mock_web_frame_client;
  EXPECT_CALL(mock_web_frame_client,
              ShowContextMenu(ContextMenuAtLocation(
                  mouse_down_event.PositionInWidget().x,
                  mouse_down_event.PositionInWidget().y)));

  // Do a sanity check with no scale applied.
  WebView()->MainFrameImpl()->SetClient(&mock_web_frame_client);
  WebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_down_event));
  WebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_up_event));

  Mock::VerifyAndClearExpectations(&mock_web_frame_client);
  mouse_down_event.button = WebMouseEvent::Button::kLeft;
  WebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_down_event));

  // Now pinch zoom into the page and move the visual viewport. The context menu
  // should still appear at the location of the event, relative to the WebView.
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  WebView()->SetPageScaleFactor(2);
  EXPECT_CALL(mock_web_frame_client, DidChangeScrollOffset());
  visual_viewport.SetLocation(FloatPoint(60, 80));
  EXPECT_CALL(mock_web_frame_client,
              ShowContextMenu(ContextMenuAtLocation(
                  mouse_down_event.PositionInWidget().x,
                  mouse_down_event.PositionInWidget().y)));

  mouse_down_event.button = WebMouseEvent::Button::kRight;
  WebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_down_event));
  WebView()->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_up_event));

  // Reset the old client so destruction can occur naturally.
  WebView()->MainFrameImpl()->SetClient(old_client);
}

// Test that the client is notified if page scroll events.
TEST_P(VisualViewportTest, TestClientNotifiedOfScrollEvents) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(200, 300));

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  WebLocalFrameClient* old_client = WebView()->MainFrameImpl()->Client();
  VisualViewportMockWebFrameClient mock_web_frame_client;
  WebView()->MainFrameImpl()->SetClient(&mock_web_frame_client);

  WebView()->SetPageScaleFactor(2);
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  EXPECT_CALL(mock_web_frame_client, DidChangeScrollOffset());
  visual_viewport.SetLocation(FloatPoint(60, 80));
  Mock::VerifyAndClearExpectations(&mock_web_frame_client);

  // Scroll vertically.
  EXPECT_CALL(mock_web_frame_client, DidChangeScrollOffset());
  visual_viewport.SetLocation(FloatPoint(60, 90));
  Mock::VerifyAndClearExpectations(&mock_web_frame_client);

  // Scroll horizontally.
  EXPECT_CALL(mock_web_frame_client, DidChangeScrollOffset());
  visual_viewport.SetLocation(FloatPoint(70, 90));

  // Reset the old client so destruction can occur naturally.
  WebView()->MainFrameImpl()->SetClient(old_client);
}

// Tests that calling scroll into view on a visible element doesn't cause
// a scroll due to a fractional offset. Bug crbug.com/463356.
TEST_P(VisualViewportTest, ScrollIntoViewFractionalOffset) {
  InitializeWithAndroidSettings();

  WebView()->MainFrameWidget()->Resize(IntSize(1000, 1000));

  RegisterMockedHttpURLLoad("scroll-into-view.html");
  NavigateTo(base_url_ + "scroll-into-view.html");

  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();
  ScrollableArea* layout_viewport_scrollable_area = frame_view.LayoutViewport();
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  Element* inputBox = GetFrame()->GetDocument()->getElementById("box");

  WebView()->SetPageScaleFactor(2);

  // The element is already in the view so the scrollIntoView shouldn't move
  // the viewport at all.
  WebView()->SetVisualViewportOffset(WebFloatPoint(250.25f, 100.25f));
  layout_viewport_scrollable_area->SetScrollOffset(ScrollOffset(0, 900.75),
                                                   kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled()) {
    EXPECT_EQ(ScrollOffset(0, 900.75),
              layout_viewport_scrollable_area->GetScrollOffset());
  } else {
    EXPECT_EQ(ScrollOffset(0, 900),
              layout_viewport_scrollable_area->GetScrollOffset());
  }
  EXPECT_EQ(FloatSize(250.25f, 100.25f), visual_viewport.GetScrollOffset());

  // Change the fractional part of the frameview to one that would round down.
  layout_viewport_scrollable_area->SetScrollOffset(ScrollOffset(0, 900.125),
                                                   kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled()) {
    EXPECT_EQ(ScrollOffset(0, 900.125),
              layout_viewport_scrollable_area->GetScrollOffset());
  } else {
    EXPECT_EQ(ScrollOffset(0, 900),
              layout_viewport_scrollable_area->GetScrollOffset());
  }
  EXPECT_EQ(FloatSize(250.25f, 100.25f), visual_viewport.GetScrollOffset());

  // Repeat both tests above with the visual viewport at a high fractional.
  WebView()->SetVisualViewportOffset(WebFloatPoint(250.875f, 100.875f));
  layout_viewport_scrollable_area->SetScrollOffset(ScrollOffset(0, 900.75),
                                                   kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled()) {
    EXPECT_EQ(ScrollOffset(0, 900.75),
              layout_viewport_scrollable_area->GetScrollOffset());
  } else {
    EXPECT_EQ(ScrollOffset(0, 900),
              layout_viewport_scrollable_area->GetScrollOffset());
  }
  EXPECT_EQ(FloatSize(250.875f, 100.875f), visual_viewport.GetScrollOffset());

  // Change the fractional part of the frameview to one that would round down.
  layout_viewport_scrollable_area->SetScrollOffset(ScrollOffset(0, 900.125),
                                                   kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled()) {
    EXPECT_EQ(ScrollOffset(0, 900.125),
              layout_viewport_scrollable_area->GetScrollOffset());
  } else {
    EXPECT_EQ(ScrollOffset(0, 900),
              layout_viewport_scrollable_area->GetScrollOffset());
  }
  EXPECT_EQ(FloatSize(250.875f, 100.875f), visual_viewport.GetScrollOffset());

  // Both viewports with a 0.5 fraction.
  WebView()->SetVisualViewportOffset(WebFloatPoint(250.5f, 100.5f));
  layout_viewport_scrollable_area->SetScrollOffset(ScrollOffset(0, 900.5),
                                                   kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  if (RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled()) {
    EXPECT_EQ(ScrollOffset(0, 900.5),
              layout_viewport_scrollable_area->GetScrollOffset());
  } else {
    EXPECT_EQ(ScrollOffset(0, 900),
              layout_viewport_scrollable_area->GetScrollOffset());
  }
  EXPECT_EQ(FloatSize(250.5f, 100.5f), visual_viewport.GetScrollOffset());
}

static ScrollOffset expectedMaxLayoutViewportScrollOffset(
    VisualViewport& visual_viewport,
    LocalFrameView& frame_view) {
  float aspect_ratio = visual_viewport.VisibleRect().Width() /
                       visual_viewport.VisibleRect().Height();
  float new_height = frame_view.FrameRect().Width() / aspect_ratio;
  IntSize contents_size = frame_view.LayoutViewport()->ContentsSize();
  return ScrollOffset(contents_size.Width() - frame_view.FrameRect().Width(),
                      contents_size.Height() - new_height);
}

TEST_P(VisualViewportTest, TestBrowserControlsAdjustment) {
  InitializeWithAndroidSettings();
  WebView()->ResizeWithBrowserControls(IntSize(500, 450), 20, 0, false);

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  visual_viewport.SetScale(1);
  EXPECT_EQ(FloatSize(500, 450), visual_viewport.VisibleRect().Size());
  EXPECT_EQ(IntSize(1000, 900), frame_view.FrameRect().Size());

  // Simulate bringing down the browser controls by 20px.
  WebView()->MainFrameWidget()->ApplyViewportChanges(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1, false, 1,
       cc::BrowserControlsState::kBoth});
  EXPECT_EQ(FloatSize(500, 430), visual_viewport.VisibleRect().Size());

  // Test that the scroll bounds are adjusted appropriately: the visual viewport
  // should be shrunk by 20px to 430px. The outer viewport was shrunk to
  // maintain the
  // aspect ratio so it's height is 860px.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_EQ(FloatSize(500, 860 - 430), visual_viewport.GetScrollOffset());

  // The outer viewport (LocalFrameView) should be affected as well.
  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        kUserScroll);
  EXPECT_EQ(expectedMaxLayoutViewportScrollOffset(visual_viewport, frame_view),
            frame_view.LayoutViewport()->GetScrollOffset());

  // Simulate bringing up the browser controls by 10.5px.
  WebView()->MainFrameWidget()->ApplyViewportChanges(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1, false, -10.5f / 20,
       cc::BrowserControlsState::kBoth});
  EXPECT_FLOAT_SIZE_EQ(FloatSize(500, 440.5f),
                       visual_viewport.VisibleRect().Size());

  // maximumScrollPosition |ceil|s the browser controls adjustment.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_FLOAT_SIZE_EQ(FloatSize(500, 881 - 441),
                       visual_viewport.GetScrollOffset());

  // The outer viewport (LocalFrameView) should be affected as well.
  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        kUserScroll);
  EXPECT_EQ(expectedMaxLayoutViewportScrollOffset(visual_viewport, frame_view),
            frame_view.LayoutViewport()->GetScrollOffset());
}

TEST_P(VisualViewportTest, TestBrowserControlsAdjustmentWithScale) {
  InitializeWithAndroidSettings();
  WebView()->ResizeWithBrowserControls(IntSize(500, 450), 20, 0, false);

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  visual_viewport.SetScale(2);
  EXPECT_EQ(FloatSize(250, 225), visual_viewport.VisibleRect().Size());
  EXPECT_EQ(IntSize(1000, 900), frame_view.FrameRect().Size());

  // Simulate bringing down the browser controls by 20px. Since we're zoomed in,
  // the browser controls take up half as much space (in document-space) than
  // they do at an unzoomed level.
  WebView()->MainFrameWidget()->ApplyViewportChanges(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1, false, 1,
       cc::BrowserControlsState::kBoth});
  EXPECT_EQ(FloatSize(250, 215), visual_viewport.VisibleRect().Size());

  // Test that the scroll bounds are adjusted appropriately.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_EQ(FloatSize(750, 860 - 215), visual_viewport.GetScrollOffset());

  // The outer viewport (LocalFrameView) should be affected as well.
  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        kUserScroll);
  ScrollOffset expected =
      expectedMaxLayoutViewportScrollOffset(visual_viewport, frame_view);
  EXPECT_EQ(expected, frame_view.LayoutViewport()->GetScrollOffset());

  // Scale back out, LocalFrameView max scroll shouldn't have changed. Visual
  // viewport should be moved up to accomodate larger view.
  WebView()->MainFrameWidget()->ApplyViewportChanges(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 0.5f, false, 0,
       cc::BrowserControlsState::kBoth});
  EXPECT_EQ(1, visual_viewport.Scale());
  EXPECT_EQ(expected, frame_view.LayoutViewport()->GetScrollOffset());
  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        kUserScroll);
  EXPECT_EQ(expected, frame_view.LayoutViewport()->GetScrollOffset());

  EXPECT_EQ(FloatSize(500, 860 - 430), visual_viewport.GetScrollOffset());
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_EQ(FloatSize(500, 860 - 430), visual_viewport.GetScrollOffset());

  // Scale out, use a scale that causes fractional rects.
  WebView()->MainFrameWidget()->ApplyViewportChanges(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 0.8f, false, -1,
       cc::BrowserControlsState::kBoth});
  EXPECT_EQ(FloatSize(625, 562.5), visual_viewport.VisibleRect().Size());

  // Bring out the browser controls by 11
  WebView()->MainFrameWidget()->ApplyViewportChanges(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1, false, 11 / 20.f,
       cc::BrowserControlsState::kBoth});
  EXPECT_EQ(FloatSize(625, 548.75), visual_viewport.VisibleRect().Size());

  // Ensure max scroll offsets are updated properly.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_FLOAT_SIZE_EQ(FloatSize(375, 877.5 - 548.75),
                       visual_viewport.GetScrollOffset());

  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        kUserScroll);
  EXPECT_EQ(expectedMaxLayoutViewportScrollOffset(visual_viewport, frame_view),
            frame_view.LayoutViewport()->GetScrollOffset());
}

// Tests that a scroll all the way to the bottom of the page, while hiding the
// browser controls doesn't cause a clamp in the viewport scroll offset when the
// top controls initiated resize occurs.
TEST_P(VisualViewportTest, TestBrowserControlsAdjustmentAndResize) {
  int browser_controls_height = 20;
  int visual_viewport_height = 450;
  int layout_viewport_height = 900;
  float page_scale = 2;
  float min_page_scale = 0.5;

  InitializeWithAndroidSettings();

  // Initialize with browser controls showing and shrinking the Blink size.
  WebView()->ResizeWithBrowserControls(
      WebSize(500, visual_viewport_height - browser_controls_height), 20, 0,
      true);
  WebView()->GetBrowserControls().SetShownRatio(1);

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  visual_viewport.SetScale(page_scale);
  EXPECT_EQ(FloatSize(250, (visual_viewport_height - browser_controls_height) /
                               page_scale),
            visual_viewport.VisibleRect().Size());
  EXPECT_EQ(IntSize(1000, layout_viewport_height -
                              browser_controls_height / min_page_scale),
            frame_view.FrameRect().Size());
  EXPECT_EQ(IntSize(500, visual_viewport_height - browser_controls_height),
            visual_viewport.Size());

  // Scroll all the way to the bottom, hiding the browser controls in the
  // process.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        kUserScroll);
  WebView()->GetBrowserControls().SetShownRatio(0);

  EXPECT_EQ(FloatSize(250, visual_viewport_height / page_scale),
            visual_viewport.VisibleRect().Size());

  ScrollOffset frame_view_expected =
      expectedMaxLayoutViewportScrollOffset(visual_viewport, frame_view);
  ScrollOffset visual_viewport_expected = ScrollOffset(
      750, layout_viewport_height - visual_viewport_height / page_scale);

  EXPECT_EQ(visual_viewport_expected, visual_viewport.GetScrollOffset());
  EXPECT_EQ(frame_view_expected,
            frame_view.LayoutViewport()->GetScrollOffset());

  ScrollOffset total_expected = visual_viewport_expected + frame_view_expected;

  // Resize the widget to match the browser controls adjustment. Ensure that the
  // total offset (i.e. what the user sees) doesn't change because of clamping
  // the offsets to valid values.
  WebView()->ResizeWithBrowserControls(WebSize(500, visual_viewport_height), 20,
                                       0, false);

  EXPECT_EQ(IntSize(500, visual_viewport_height), visual_viewport.Size());
  EXPECT_EQ(FloatSize(250, visual_viewport_height / page_scale),
            visual_viewport.VisibleRect().Size());
  EXPECT_EQ(IntSize(1000, layout_viewport_height),
            frame_view.FrameRect().Size());
  EXPECT_EQ(total_expected, visual_viewport.GetScrollOffset() +
                                frame_view.LayoutViewport()->GetScrollOffset());
}

// Tests that a scroll all the way to the bottom while showing the browser
// controls doesn't cause a clamp to the viewport scroll offset when the browser
// controls initiated resize occurs.
TEST_P(VisualViewportTest, TestBrowserControlsShrinkAdjustmentAndResize) {
  int browser_controls_height = 20;
  int visual_viewport_height = 500;
  int layout_viewport_height = 1000;
  int content_height = 2000;
  float page_scale = 2;
  float min_page_scale = 0.5;

  InitializeWithAndroidSettings();

  // Initialize with browser controls hidden and not shrinking the Blink size.
  WebView()->ResizeWithBrowserControls(IntSize(500, visual_viewport_height), 20,
                                       0, false);
  WebView()->GetBrowserControls().SetShownRatio(0);

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  visual_viewport.SetScale(page_scale);
  EXPECT_EQ(FloatSize(250, visual_viewport_height / page_scale),
            visual_viewport.VisibleRect().Size());
  EXPECT_EQ(IntSize(1000, layout_viewport_height),
            frame_view.FrameRect().Size());
  EXPECT_EQ(IntSize(500, visual_viewport_height), visual_viewport.Size());

  // Scroll all the way to the bottom, showing the the browser controls in the
  // process. (This could happen via window.scrollTo during a scroll, for
  // example).
  WebView()->GetBrowserControls().SetShownRatio(1);
  visual_viewport.Move(ScrollOffset(10000, 10000));
  frame_view.LayoutViewport()->ScrollBy(ScrollOffset(10000, 10000),
                                        kUserScroll);

  EXPECT_EQ(FloatSize(250, (visual_viewport_height - browser_controls_height) /
                               page_scale),
            visual_viewport.VisibleRect().Size());

  ScrollOffset frame_view_expected(
      0, content_height - (layout_viewport_height -
                           browser_controls_height / min_page_scale));
  ScrollOffset visual_viewport_expected = ScrollOffset(
      750, (layout_viewport_height - browser_controls_height / min_page_scale -
            visual_viewport.VisibleRect().Height()));

  EXPECT_EQ(visual_viewport_expected, visual_viewport.GetScrollOffset());
  EXPECT_EQ(frame_view_expected,
            frame_view.LayoutViewport()->GetScrollOffset());

  ScrollOffset total_expected = visual_viewport_expected + frame_view_expected;

  // Resize the widget to match the browser controls adjustment. Ensure that the
  // total offset (i.e. what the user sees) doesn't change because of clamping
  // the offsets to valid values.
  WebView()->ResizeWithBrowserControls(
      WebSize(500, visual_viewport_height - browser_controls_height), 20, 0,
      true);

  EXPECT_EQ(IntSize(500, visual_viewport_height - browser_controls_height),
            visual_viewport.Size());
  EXPECT_EQ(FloatSize(250, (visual_viewport_height - browser_controls_height) /
                               page_scale),
            visual_viewport.VisibleRect().Size());
  EXPECT_EQ(IntSize(1000, layout_viewport_height -
                              browser_controls_height / min_page_scale),
            frame_view.FrameRect().Size());
  EXPECT_EQ(total_expected, visual_viewport.GetScrollOffset() +
                                frame_view.LayoutViewport()->GetScrollOffset());
}

// Tests that a resize due to browser controls hiding doesn't incorrectly clamp
// the main frame's scroll offset. crbug.com/428193.
TEST_P(VisualViewportTest, TestTopControlHidingResizeDoesntClampMainFrame) {
  InitializeWithAndroidSettings();
  WebView()->ResizeWithBrowserControls(WebView()->MainFrameWidget()->Size(),
                                       500, 0, false);
  WebView()->MainFrameWidget()->ApplyViewportChanges(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1, false, 1,
       cc::BrowserControlsState::kBoth});
  WebView()->ResizeWithBrowserControls(WebSize(1000, 1000), 500, 0, true);

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");
  UpdateAllLifecyclePhases();

  // Scroll the LocalFrameView to the bottom of the page but "hide" the browser
  // controls on the compositor side so the max scroll position should account
  // for the full viewport height.
  WebView()->MainFrameWidget()->ApplyViewportChanges(
      {gfx::ScrollOffset(), gfx::Vector2dF(), 1, false, -1,
       cc::BrowserControlsState::kBoth});
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();
  frame_view.LayoutViewport()->SetScrollOffset(ScrollOffset(0, 10000),
                                               kProgrammaticScroll);
  EXPECT_EQ(500, frame_view.LayoutViewport()->GetScrollOffset().Height());

  // Now send the resize, make sure the scroll offset doesn't change.
  WebView()->ResizeWithBrowserControls(WebSize(1000, 1500), 500, 0, false);
  EXPECT_EQ(500, frame_view.LayoutViewport()->GetScrollOffset().Height());
}

static void configureHiddenScrollbarsSettings(WebSettings* settings) {
  VisualViewportTest::ConfigureAndroidSettings(settings);
  settings->SetHideScrollbars(true);
}

// Tests that scrollbar layers are not attached to the inner viewport container
// layer when hideScrollbars WebSetting is true.
TEST_P(VisualViewportTest,
       TestScrollbarsNotAttachedWhenHideScrollbarsSettingIsTrue) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  InitializeWithAndroidSettings(configureHiddenScrollbarsSettings);
  WebView()->MainFrameWidget()->Resize(IntSize(100, 150));
  NavigateTo("about:blank");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_FALSE(visual_viewport.LayerForHorizontalScrollbar());
  EXPECT_FALSE(visual_viewport.LayerForVerticalScrollbar());
}

// Tests that scrollbar layers are attached to the inner viewport container
// layer when hideScrollbars WebSetting is false.
TEST_P(VisualViewportTest,
       TestScrollbarsAttachedWhenHideScrollbarsSettingIsFalse) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(100, 150));
  NavigateTo("about:blank");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_TRUE(visual_viewport.LayerForHorizontalScrollbar());
  EXPECT_TRUE(visual_viewport.LayerForVerticalScrollbar());
  EXPECT_TRUE(visual_viewport.LayerForHorizontalScrollbar()->Parent());
  EXPECT_TRUE(visual_viewport.LayerForVerticalScrollbar()->Parent());
}

// Tests that the layout viewport's scroll layer bounds are updated in a
// compositing change update. crbug.com/423188.
TEST_P(VisualViewportTest, TestChangingContentSizeAffectsScrollBounds) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(100, 150));

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  WebView()->MainFrameImpl()->ExecuteScript(
      WebScriptSource("var content = document.getElementById(\"content\");"
                      "content.style.width = \"1500px\";"
                      "content.style.height = \"2400px\";"));
  frame_view.UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);
  cc::Layer* scroll_layer =
      frame_view.LayoutViewport()->LayerForScrolling()->CcLayer();

  EXPECT_EQ(gfx::Size(1500, 2400), scroll_layer->bounds());

  if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    EXPECT_EQ(IntSize(1500, 2400), frame_view.GetLayoutView()
                                       ->FirstFragment()
                                       .PaintProperties()
                                       ->Scroll()
                                       ->ContentsSize());
  }
}

// Tests that resizing the visual viepwort keeps its bounds within the outer
// viewport.
TEST_P(VisualViewportTest, ResizeVisualViewportStaysWithinOuterViewport) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(100, 200));

  NavigateTo("about:blank");
  UpdateAllLifecyclePhases();

  WebView()->ResizeVisualViewport(IntSize(100, 100));

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.Move(ScrollOffset(0, 100));

  EXPECT_EQ(100, visual_viewport.GetScrollOffset().Height());

  WebView()->ResizeVisualViewport(IntSize(100, 200));

  EXPECT_EQ(0, visual_viewport.GetScrollOffset().Height());
}

TEST_P(VisualViewportTest, ElementBoundsInViewportSpaceAccountsForViewport) {
  InitializeWithAndroidSettings();

  WebView()->MainFrameWidget()->Resize(IntSize(500, 800));

  RegisterMockedHttpURLLoad("pinch-viewport-input-field.html");
  NavigateTo(base_url_ + "pinch-viewport-input-field.html");

  WebView()->SetInitialFocus(false);
  Element* input_element = WebView()->FocusedElement();

  IntRect bounds = input_element->GetLayoutObject()->AbsoluteBoundingBoxRect();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  FloatPoint scrollDelta(250, 400);
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(scrollDelta);

  const IntRect bounds_in_viewport = input_element->BoundsInViewport();
  IntRect expectedBounds = bounds;
  expectedBounds.Scale(2.f);
  FloatPoint expectedScrollDelta = scrollDelta;
  expectedScrollDelta.Scale(2.f, 2.f);

  EXPECT_EQ(RoundedIntPoint(FloatPoint(FloatPoint(expectedBounds.Location()) -
                                       expectedScrollDelta)),
            bounds_in_viewport.Location());
  EXPECT_EQ(expectedBounds.Size(), bounds_in_viewport.Size());
}

TEST_P(VisualViewportTest, ElementVisibleBoundsInVisualViewport) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(640, 1080));
  RegisterMockedHttpURLLoad("viewport-select.html");
  NavigateTo(base_url_ + "viewport-select.html");

  ASSERT_EQ(2.0f, WebView()->PageScaleFactor());
  WebView()->SetInitialFocus(false);
  Element* element = WebView()->FocusedElement();
  EXPECT_FALSE(element->VisibleBoundsInVisualViewport().IsEmpty());

  WebView()->SetPageScaleFactor(4.0);
  EXPECT_TRUE(element->VisibleBoundsInVisualViewport().IsEmpty());
}

// Test that the various window.scroll and document.body.scroll properties and
// methods don't change with the visual viewport.
TEST_P(VisualViewportTest, visualViewportIsInert) {
  WebViewImpl* web_view_impl =
      helper_.InitializeWithSettings(&ConfigureAndroidCompositing);

  web_view_impl->MainFrameWidget()->Resize(IntSize(200, 300));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(
      web_view_impl->MainFrameImpl(),
      "<!DOCTYPE html>"
      "<meta name='viewport' content='width=200,minimum-scale=1'>"
      "<style>"
      "  body {"
      "    width: 800px;"
      "    height: 800px;"
      "    margin: 0;"
      "  }"
      "</style>",
      base_url);
  UpdateAllLifecyclePhases();
  LocalDOMWindow* window =
      web_view_impl->MainFrameImpl()->GetFrame()->DomWindow();
  HTMLElement* html = ToHTMLHtmlElement(window->document()->documentElement());

  ASSERT_EQ(200, window->innerWidth());
  ASSERT_EQ(300, window->innerHeight());
  ASSERT_EQ(200, html->clientWidth());
  ASSERT_EQ(300, html->clientHeight());

  VisualViewport& visual_viewport = web_view_impl->MainFrameImpl()
                                        ->GetFrame()
                                        ->GetPage()
                                        ->GetVisualViewport();
  visual_viewport.SetScale(2);

  ASSERT_EQ(100, visual_viewport.VisibleRect().Width());
  ASSERT_EQ(150, visual_viewport.VisibleRect().Height());

  EXPECT_EQ(200, window->innerWidth());
  EXPECT_EQ(300, window->innerHeight());
  EXPECT_EQ(200, html->clientWidth());
  EXPECT_EQ(300, html->clientHeight());

  visual_viewport.SetScrollOffset(ScrollOffset(10, 15), kProgrammaticScroll,
                                  kScrollBehaviorInstant,
                                  ScrollableArea::ScrollCallback());

  ASSERT_EQ(10, visual_viewport.GetScrollOffset().Width());
  ASSERT_EQ(15, visual_viewport.GetScrollOffset().Height());
  EXPECT_EQ(0, window->scrollX());
  EXPECT_EQ(0, window->scrollY());

  html->setScrollLeft(5);
  html->setScrollTop(30);
  EXPECT_EQ(5, html->scrollLeft());
  EXPECT_EQ(30, html->scrollTop());
  EXPECT_EQ(10, visual_viewport.GetScrollOffset().Width());
  EXPECT_EQ(15, visual_viewport.GetScrollOffset().Height());

  html->setScrollLeft(5000);
  html->setScrollTop(5000);
  EXPECT_EQ(600, html->scrollLeft());
  EXPECT_EQ(500, html->scrollTop());
  EXPECT_EQ(10, visual_viewport.GetScrollOffset().Width());
  EXPECT_EQ(15, visual_viewport.GetScrollOffset().Height());

  html->setScrollLeft(0);
  html->setScrollTop(0);
  EXPECT_EQ(0, html->scrollLeft());
  EXPECT_EQ(0, html->scrollTop());
  EXPECT_EQ(10, visual_viewport.GetScrollOffset().Width());
  EXPECT_EQ(15, visual_viewport.GetScrollOffset().Height());

  window->scrollTo(5000, 5000);
  EXPECT_EQ(600, html->scrollLeft());
  EXPECT_EQ(500, html->scrollTop());
  EXPECT_EQ(10, visual_viewport.GetScrollOffset().Width());
  EXPECT_EQ(15, visual_viewport.GetScrollOffset().Height());
}

// Tests that when a new frame is created, it is created with the intended size
// (i.e. viewport at minimum scale, 100x200 / 0.5).
TEST_P(VisualViewportTest, TestMainFrameInitializationSizing) {
  InitializeWithAndroidSettings();

  WebView()->MainFrameWidget()->Resize(IntSize(100, 200));

  RegisterMockedHttpURLLoad("content-width-1000-min-scale.html");
  NavigateTo(base_url_ + "content-width-1000-min-scale.html");

  WebLocalFrameImpl* local_frame = WebView()->MainFrameImpl();
  // The shutdown() calls are a hack to prevent this test from violating
  // invariants about frame state during navigation/detach.
  local_frame->GetFrame()->GetDocument()->Shutdown();
  local_frame->CreateFrameView();

  LocalFrameView& frame_view = *local_frame->GetFrameView();
  EXPECT_EQ(IntSize(200, 400), frame_view.FrameRect().Size());
  frame_view.Dispose();
}

// Tests that the maximum scroll offset of the viewport can be fractional.
TEST_P(VisualViewportTest, FractionalMaxScrollOffset) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(101, 201));
  NavigateTo("about:blank");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  ScrollableArea* scrollable_area = &visual_viewport;

  WebView()->SetPageScaleFactor(1.0);
  EXPECT_EQ(ScrollOffset(), scrollable_area->MaximumScrollOffset());

  WebView()->SetPageScaleFactor(2);
  EXPECT_EQ(ScrollOffset(101. / 2., 201. / 2.),
            scrollable_area->MaximumScrollOffset());
}

// Tests that the slow scrolling after an impl scroll on the visual viewport is
// continuous. crbug.com/453460 was caused by the impl-path not updating the
// ScrollAnimatorBase class.
TEST_P(VisualViewportTest, SlowScrollAfterImplScroll) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(800, 600));
  NavigateTo("about:blank");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  // Apply some scroll and scale from the impl-side.
  WebView()->MainFrameWidget()->ApplyViewportChanges(
      {gfx::ScrollOffset(300, 200), gfx::Vector2dF(0, 0), 2, false, 0,
       cc::BrowserControlsState::kBoth});

  EXPECT_EQ(FloatSize(300, 200), visual_viewport.GetScrollOffset());

  // Send a scroll event on the main thread path.
  WebGestureEvent gsb(
      WebInputEvent::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  gsb.SetFrameScale(1);
  gsb.data.scroll_begin.delta_x_hint = -50;
  gsb.data.scroll_begin.delta_x_hint = -60;
  gsb.data.scroll_begin.delta_hint_units =
      ScrollGranularity::kScrollByPrecisePixel;
  GetFrame()->GetEventHandler().HandleGestureEvent(gsb);

  WebGestureEvent gsu(
      WebInputEvent::kGestureScrollUpdate, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(), WebGestureDevice::kTouchpad);
  gsu.SetFrameScale(1);
  gsu.data.scroll_update.delta_x = -50;
  gsu.data.scroll_update.delta_y = -60;
  gsu.data.scroll_update.delta_units = ScrollGranularity::kScrollByPrecisePixel;
  gsu.data.scroll_update.velocity_x = 1;
  gsu.data.scroll_update.velocity_y = 1;

  GetFrame()->GetEventHandler().HandleGestureEvent(gsu);

  // The scroll sent from the impl-side must not be overwritten.
  EXPECT_EQ(FloatSize(350, 260), visual_viewport.GetScrollOffset());
}

TEST_P(VisualViewportTest, AccessibilityHitTestWhileZoomedIn) {
  InitializeWithDesktopSettings();

  RegisterMockedHttpURLLoad("hit-test.html");
  NavigateTo(base_url_ + "hit-test.html");

  WebView()->MainFrameWidget()->Resize(IntSize(500, 500));
  UpdateAllLifecyclePhases();

  WebDocument web_doc = WebView()->MainFrameImpl()->GetDocument();
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  WebAXContext ax_context(web_doc);

  WebView()->SetPageScaleFactor(2);
  WebView()->SetVisualViewportOffset(WebFloatPoint(200, 230));
  frame_view.LayoutViewport()->SetScrollOffset(ScrollOffset(400, 1100),
                                               kProgrammaticScroll);

  // FIXME(504057): PaintLayerScrollableArea dirties the compositing state.
  ForceFullCompositingUpdate();

  // Because of where the visual viewport is located, this should hit the bottom
  // right target (target 4).
  WebAXObject hitNode =
      WebAXObject::FromWebDocument(web_doc).HitTest(WebPoint(154, 165));
  ax::mojom::NameFrom name_from;
  WebVector<WebAXObject> name_objects;
  EXPECT_EQ(std::string("Target4"),
            hitNode.GetName(name_from, name_objects).Utf8());
}

// Tests that the maximum scroll offset of the viewport can be fractional.
TEST_P(VisualViewportTest, TestCoordinateTransforms) {
  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(800, 600));
  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  VisualViewport& visual_viewport = WebView()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  // At scale = 1 the transform should be a no-op.
  visual_viewport.SetScale(1);
  EXPECT_FLOAT_POINT_EQ(
      FloatPoint(314, 273),
      visual_viewport.ViewportToRootFrame(FloatPoint(314, 273)));
  EXPECT_FLOAT_POINT_EQ(
      FloatPoint(314, 273),
      visual_viewport.RootFrameToViewport(FloatPoint(314, 273)));

  // At scale = 2.
  visual_viewport.SetScale(2);
  EXPECT_FLOAT_POINT_EQ(FloatPoint(55, 75), visual_viewport.ViewportToRootFrame(
                                                FloatPoint(110, 150)));
  EXPECT_FLOAT_POINT_EQ(
      FloatPoint(110, 150),
      visual_viewport.RootFrameToViewport(FloatPoint(55, 75)));

  // At scale = 2 and with the visual viewport offset.
  visual_viewport.SetLocation(FloatPoint(10, 12));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(50, 62), visual_viewport.ViewportToRootFrame(
                                                FloatPoint(80, 100)));
  EXPECT_FLOAT_POINT_EQ(
      FloatPoint(80, 100),
      visual_viewport.RootFrameToViewport(FloatPoint(50, 62)));

  // Test points that will cause non-integer values.
  EXPECT_FLOAT_POINT_EQ(
      FloatPoint(50.5, 62.4),
      visual_viewport.ViewportToRootFrame(FloatPoint(81, 100.8)));
  EXPECT_FLOAT_POINT_EQ(
      FloatPoint(81, 100.8),
      visual_viewport.RootFrameToViewport(FloatPoint(50.5, 62.4)));

  // Scrolling the main frame should have no effect.
  frame_view.LayoutViewport()->SetScrollOffset(ScrollOffset(100, 120),
                                               kProgrammaticScroll);
  EXPECT_FLOAT_POINT_EQ(FloatPoint(50, 62), visual_viewport.ViewportToRootFrame(
                                                FloatPoint(80, 100)));
  EXPECT_FLOAT_POINT_EQ(
      FloatPoint(80, 100),
      visual_viewport.RootFrameToViewport(FloatPoint(50, 62)));
}

// Tests that the window dimensions are available before a full layout occurs.
// More specifically, it checks that the innerWidth and innerHeight window
// properties will trigger a layout which will cause an update to viewport
// constraints and a refreshed initial scale. crbug.com/466718
TEST_P(VisualViewportTest, WindowDimensionsOnLoad) {
  InitializeWithAndroidSettings();
  RegisterMockedHttpURLLoad("window_dimensions.html");
  WebView()->MainFrameWidget()->Resize(IntSize(800, 600));
  NavigateTo(base_url_ + "window_dimensions.html");

  Element* output = GetFrame()->GetDocument()->getElementById("output");
  DCHECK(output);
  EXPECT_EQ("1600x1200", output->InnerHTMLAsString());
}

// Similar to above but make sure the initial scale is updated with the content
// width for a very wide page. That is, make that innerWidth/Height actually
// trigger a layout of the content, and not just an update of the viepwort.
// crbug.com/466718
TEST_P(VisualViewportTest, WindowDimensionsOnLoadWideContent) {
  InitializeWithAndroidSettings();
  RegisterMockedHttpURLLoad("window_dimensions_wide_div.html");
  WebView()->MainFrameWidget()->Resize(IntSize(800, 600));
  NavigateTo(base_url_ + "window_dimensions_wide_div.html");

  Element* output = GetFrame()->GetDocument()->getElementById("output");
  DCHECK(output);
  EXPECT_EQ("2000x1500", output->InnerHTMLAsString());
}

TEST_P(VisualViewportTest, ResizeWithScrollAnchoring) {
  InitializeWithDesktopSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(800, 600));

  RegisterMockedHttpURLLoad("icb-relative-content.html");
  NavigateTo(base_url_ + "icb-relative-content.html");

  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();
  frame_view.LayoutViewport()->SetScrollOffset(ScrollOffset(700, 500),
                                               kProgrammaticScroll);
  UpdateAllLifecyclePhases();

  WebView()->MainFrameWidget()->Resize(IntSize(800, 300));
  EXPECT_EQ(ScrollOffset(700, 200),
            frame_view.LayoutViewport()->GetScrollOffset());
}

// Ensure that resize anchoring as happens when browser controls hide/show
// affects the scrollable area that's currently set as the root scroller.
TEST_P(VisualViewportTest, ResizeAnchoringWithRootScroller) {
  ScopedSetRootScrollerForTest root_scroller(true);

  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(800, 600));

  RegisterMockedHttpURLLoad("root-scroller-div.html");
  NavigateTo(base_url_ + "root-scroller-div.html");

  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  Element* scroller = GetFrame()->GetDocument()->getElementById("rootScroller");
  NonThrowableExceptionState non_throw;
  GetFrame()->GetDocument()->setRootScroller(scroller, non_throw);

  WebView()->SetPageScaleFactor(3.f);
  frame_view.GetScrollableArea()->SetScrollOffset(ScrollOffset(0, 400),
                                                  kProgrammaticScroll);

  VisualViewport& visual_viewport = WebView()->GetPage()->GetVisualViewport();
  visual_viewport.SetScrollOffset(ScrollOffset(0, 400), kProgrammaticScroll,
                                  kScrollBehaviorInstant,
                                  ScrollableArea::ScrollCallback());

  WebView()->MainFrameWidget()->Resize(IntSize(800, 500));

  EXPECT_EQ(ScrollOffset(), frame_view.LayoutViewport()->GetScrollOffset());
}

// Ensure that resize anchoring as happens when the device is rotated affects
// the scrollable area that's currently set as the root scroller.
TEST_P(VisualViewportTest, RotationAnchoringWithRootScroller) {
  ScopedSetRootScrollerForTest root_scroller(true);

  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(800, 600));

  RegisterMockedHttpURLLoad("root-scroller-div.html");
  NavigateTo(base_url_ + "root-scroller-div.html");

  LocalFrameView& frame_view = *WebView()->MainFrameImpl()->GetFrameView();

  Element* scroller = GetFrame()->GetDocument()->getElementById("rootScroller");
  NonThrowableExceptionState non_throw;
  GetFrame()->GetDocument()->setRootScroller(scroller, non_throw);
  UpdateAllLifecyclePhases();

  scroller->setScrollTop(800);

  WebView()->MainFrameWidget()->Resize(IntSize(600, 800));

  EXPECT_EQ(ScrollOffset(), frame_view.LayoutViewport()->GetScrollOffset());
  EXPECT_EQ(600, scroller->scrollTop());
}

// Make sure a composited background-attachment:fixed background gets resized
// by browser controls.
TEST_P(VisualViewportTest, ResizeCompositedAndFixedBackground) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  WebViewImpl* web_view_impl =
      helper_.InitializeWithSettings(&ConfigureAndroidCompositing);

  int page_width = 640;
  int page_height = 480;
  float browser_controls_height = 50.0f;
  int smallest_height = page_height - browser_controls_height;

  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, page_height),
                                           browser_controls_height, 0, false);

  RegisterMockedHttpURLLoad("http://example.com/foo.png", "white-1x1.png");
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view_impl->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<style>"
                                     "  body {"
                                     "    background: url('foo.png');"
                                     "    background-attachment: fixed;"
                                     "    background-size: cover;"
                                     "    background-repeat: no-repeat;"
                                     "  }"
                                     "  div { height:1000px; width: 200px; }"
                                     "</style>"
                                     "<div></div>",
                                     base_url);

  UpdateAllLifecyclePhases();
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();
  GraphicsLayer* backgroundLayer = document->GetLayoutView()
                                       ->Layer()
                                       ->GetCompositedLayerMapping()
                                       ->MainGraphicsLayer();
  ASSERT_TRUE(backgroundLayer);

  ASSERT_EQ(page_width, backgroundLayer->Size().width());
  ASSERT_EQ(page_height, backgroundLayer->Size().height());
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(smallest_height, document->View()->GetLayoutSize().Height());

  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, smallest_height),
                                           browser_controls_height, 0, true);

  // The layout size should not have changed.
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(smallest_height, document->View()->GetLayoutSize().Height());

  // The background layer's size should have changed though.
  EXPECT_EQ(page_width, backgroundLayer->Size().width());
  EXPECT_EQ(smallest_height, backgroundLayer->Size().height());

  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, page_height),
                                           browser_controls_height, 0, true);

  // The background layer's size should change again.
  EXPECT_EQ(page_width, backgroundLayer->Size().width());
  EXPECT_EQ(page_height, backgroundLayer->Size().height());
}

static void ConfigureAndroidNonCompositing(WebSettings* settings) {
  settings->SetPreferCompositingToLCDTextEnabled(false);
  settings->SetViewportMetaEnabled(true);
  settings->SetViewportEnabled(true);
  settings->SetMainFrameResizesAreOrientationChanges(true);
  settings->SetShrinksViewportContentToFit(true);
}

// Make sure a non-composited background-attachment:fixed background gets
// resized by browser controls.
TEST_P(VisualViewportTest, ResizeNonCompositedAndFixedBackground) {
  // This test needs the |FastMobileScrolling| feature to be disabled
  // although it is stable on Android.
  ScopedFastMobileScrollingForTest fast_mobile_scrolling(false);

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  WebViewImpl* web_view_impl =
      helper_.InitializeWithSettings(&ConfigureAndroidNonCompositing);

  int page_width = 640;
  int page_height = 480;
  float browser_controls_height = 50.0f;
  int smallest_height = page_height - browser_controls_height;

  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, page_height),
                                           browser_controls_height, 0, false);

  RegisterMockedHttpURLLoad("http://example.com/foo.png", "white-1x1.png");
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view_impl->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<style>"
                                     "  body {"
                                     "    margin: 0px;"
                                     "    background: url('foo.png');"
                                     "    background-attachment: fixed;"
                                     "    background-size: cover;"
                                     "    background-repeat: no-repeat;"
                                     "  }"
                                     "  div { height:1000px; width: 200px; }"
                                     "</style>"
                                     "<div></div>",
                                     base_url);
  UpdateAllLifecyclePhases();
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();
  document->View()->SetTracksPaintInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, smallest_height),
                                           browser_controls_height, 0, true);

  // The layout size should not have changed.
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(smallest_height, document->View()->GetLayoutSize().Height());

  // Fixed-attachment background is affected by viewport size.
  EXPECT_FALSE(MainGraphicsLayerHasRasterInvalidations(document));
  ASSERT_TRUE(ScrollingContentsLayerHasRasterInvalidations(document));
  EXPECT_THAT(
      ScrollingContentsLayerRasterInvalidations(document),
      UnorderedElementsAre(RasterInvalidationInfo{
          &ScrollingBackgroundClient(document),
          ScrollingBackgroundClient(document).DebugName(),
          IntRect(0, 0, 640, 1000), PaintInvalidationReason::kBackground}));

  document->View()->SetTracksPaintInvalidations(false);

  document->View()->SetTracksPaintInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, page_height),
                                           browser_controls_height, 0, true);

  // Fixed-attachment background is affected by viewport size.
  EXPECT_FALSE(MainGraphicsLayerHasRasterInvalidations(document));
  ASSERT_TRUE(ScrollingContentsLayerHasRasterInvalidations(document));
  EXPECT_THAT(
      ScrollingContentsLayerRasterInvalidations(document),
      UnorderedElementsAre(RasterInvalidationInfo{
          &ScrollingBackgroundClient(document),
          ScrollingBackgroundClient(document).DebugName(),
          IntRect(0, 0, 640, 1000), PaintInvalidationReason::kBackground}));

  document->View()->SetTracksPaintInvalidations(false);
}

// Make sure a browser control resize with background-attachment:not-fixed
// background doesn't cause invalidation or layout.
TEST_P(VisualViewportTest, ResizeNonFixedBackgroundNoLayoutOrInvalidation) {
  WebViewImpl* web_view_impl =
      helper_.InitializeWithSettings(&ConfigureAndroidCompositing);

  int page_width = 640;
  int page_height = 480;
  float browser_controls_height = 50.0f;
  int smallest_height = page_height - browser_controls_height;

  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, page_height),
                                           browser_controls_height, 0, false);

  RegisterMockedHttpURLLoad("http://example.com/foo.png", "white-1x1.png");
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  // This time the background is the default attachment.
  frame_test_helpers::LoadHTMLString(web_view_impl->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<style>"
                                     "  body {"
                                     "    margin: 0px;"
                                     "    background: url('foo.png');"
                                     "    background-size: cover;"
                                     "    background-repeat: no-repeat;"
                                     "  }"
                                     "  div { height:1000px; width: 200px; }"
                                     "</style>"
                                     "<div></div>",
                                     base_url);
  UpdateAllLifecyclePhases();
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();

  // A resize will do a layout synchronously so manually check that we don't
  // setNeedsLayout from viewportSizeChanged.
  document->View()->ViewportSizeChanged(false, true);
  unsigned needs_layout_objects = 0;
  unsigned total_objects = 0;
  bool is_subtree = false;
  EXPECT_FALSE(document->View()->NeedsLayout());
  document->View()->CountObjectsNeedingLayout(needs_layout_objects,
                                              total_objects, is_subtree);
  EXPECT_EQ(0u, needs_layout_objects);

  UpdateAllLifecyclePhases();
  // Do a real resize to check for invalidations.
  document->View()->SetTracksPaintInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, smallest_height),
                                           browser_controls_height, 0, true);

  // The layout size should not have changed.
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(smallest_height, document->View()->GetLayoutSize().Height());

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // No invalidations should have occurred on either main layer or scrolling
    // contents layer.
    EXPECT_FALSE(MainGraphicsLayerHasRasterInvalidations(document));
    EXPECT_FALSE(ScrollingContentsLayerHasRasterInvalidations(document));
  }

  document->View()->SetTracksPaintInvalidations(false);
}

TEST_P(VisualViewportTest, InvalidateLayoutViewWhenDocumentSmallerThanView) {
  WebViewImpl* web_view_impl =
      helper_.InitializeWithSettings(&ConfigureAndroidCompositing);

  int page_width = 320;
  int page_height = 590;
  float browser_controls_height = 50.0f;
  int largest_height = page_height + browser_controls_height;

  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, page_height),
                                           browser_controls_height, 0, true);

  frame_test_helpers::LoadFrame(web_view_impl->MainFrameImpl(), "about:blank");
  UpdateAllLifecyclePhases();
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();

  // Do a resize to check for invalidations.
  document->View()->SetTracksPaintInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, largest_height),
                                           browser_controls_height, 0, false);

  // The layout size should not have changed.
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(page_height, document->View()->GetLayoutSize().Height());

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // Incremental raster invalidation is needed because the resize exposes
    // unpainted area of background.
    EXPECT_FALSE(MainGraphicsLayerHasRasterInvalidations(document));
    ASSERT_TRUE(ScrollingContentsLayerHasRasterInvalidations(document));
    EXPECT_THAT(
        ScrollingContentsLayerRasterInvalidations(document),
        UnorderedElementsAre(RasterInvalidationInfo{
            &ScrollingBackgroundClient(document),
            ScrollingBackgroundClient(document).DebugName(),
            IntRect(0, 590, 320, 50), PaintInvalidationReason::kIncremental}));
  }

  document->View()->SetTracksPaintInvalidations(false);

  // Resize back to the original size.
  document->View()->SetTracksPaintInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, page_height),
                                           browser_controls_height, 0, false);

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // No raster invalidation is needed because of no change within the root
    // scrolling layer.
    EXPECT_FALSE(MainGraphicsLayerHasRasterInvalidations(document));
    EXPECT_FALSE(ScrollingContentsLayerHasRasterInvalidations(document));
  }

  document->View()->SetTracksPaintInvalidations(false);
}

// Ensure we create transform node for overscroll elasticity properly.
TEST_P(VisualViewportTest, EnsureOverscrollElasticityTransformNode) {
  if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
    return;

  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(400, 400));
  NavigateTo("about:blank");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto* node = visual_viewport.GetOverscrollElasticityTransformNode();
  CompositorElementId element_id =
      visual_viewport.GetCompositorOverscrollElasticityElementId();
  EXPECT_TRUE(node);
  EXPECT_EQ(element_id, node->GetCompositorElementId());
}

// Ensure we create effect node for scrollbar properly.
TEST_P(VisualViewportTest, EnsureEffectNodeForScrollbars) {
  if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() ||
      // TODO(wangxianzhu): Should this work for CompositeAfterPaint?
      RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  InitializeWithAndroidSettings();
  WebView()->MainFrameWidget()->Resize(IntSize(400, 400));
  NavigateTo("about:blank");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  auto* vertical_scrollbar = visual_viewport.LayerForVerticalScrollbar();
  auto* horizontal_scrollbar = visual_viewport.LayerForHorizontalScrollbar();
  ASSERT_TRUE(vertical_scrollbar);
  ASSERT_TRUE(horizontal_scrollbar);

  auto& vertical_scrollbar_state = vertical_scrollbar->GetPropertyTreeState();
  auto& horizontal_scrollbar_state =
      horizontal_scrollbar->GetPropertyTreeState();

  ScrollbarThemeOverlay& theme = ScrollbarThemeOverlay::MobileTheme();
  int scrollbar_thickness = clampTo<int>(std::floor(
      GetFrame()->GetPage()->GetChromeClient().WindowToViewportScalar(
          theme.ScrollbarThickness(kRegularScrollbar))));

  EXPECT_EQ(vertical_scrollbar_state.Effect().GetCompositorElementId(),
            visual_viewport.GetScrollbarElementId(
                ScrollbarOrientation::kVerticalScrollbar));
  EXPECT_EQ(vertical_scrollbar->GetOffsetFromTransformNode(),
            IntPoint(400 - scrollbar_thickness, 0));

  EXPECT_EQ(horizontal_scrollbar_state.Effect().GetCompositorElementId(),
            visual_viewport.GetScrollbarElementId(
                ScrollbarOrientation::kHorizontalScrollbar));
  EXPECT_EQ(horizontal_scrollbar->GetOffsetFromTransformNode(),
            IntPoint(0, 400 - scrollbar_thickness));

  EXPECT_EQ(vertical_scrollbar_state.Effect().Parent(),
            horizontal_scrollbar_state.Effect().Parent());
}

// Make sure we don't crash when the visual viewport's height is 0. This can
// happen transiently in autoresize mode and cause a crash. This test passes if
// it doesn't crash.
TEST_P(VisualViewportTest, AutoResizeNoHeightUsesMinimumHeight) {
  InitializeWithDesktopSettings();
  WebView()->ResizeWithBrowserControls(WebSize(0, 0), 0, 0, false);
  WebView()->EnableAutoResizeMode(WebSize(25, 25), WebSize(100, 100));
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(WebView()->MainFrameImpl(),
                                     "<!DOCTYPE html>"
                                     "<style>"
                                     "  body {"
                                     "    margin: 0px;"
                                     "  }"
                                     "  div { height:110vh; width: 110vw; }"
                                     "</style>"
                                     "<div></div>",
                                     base_url);
}

class VisualViewportSimTest : public SimTest {
 public:
  VisualViewportSimTest() {}

  void SetUp() override {
    SimTest::SetUp();

    // Use settings that resemble the Android configuration.
    WebView().GetSettings()->SetViewportEnabled(true);
    WebView().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
    WebView().GetSettings()->SetViewportMetaEnabled(true);
    WebView().GetSettings()->SetViewportEnabled(true);
    WebView().GetSettings()->SetMainFrameResizesAreOrientationChanges(true);
    WebView().GetSettings()->SetShrinksViewportContentToFit(true);
    WebView().SetDefaultPageScaleLimits(0.25f, 5);
  }
};

// Test that we correcty size the visual viewport's scrolling contents layer
// when the layout viewport is smaller.
TEST_F(VisualViewportSimTest, ScrollingContentsSmallerThanContainer) {
  WebView().MainFrameWidget()->Resize(WebSize(400, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
          <!DOCTYPE html>
          <meta name="viewport" content="width=320">
          <style>
            body {
              height: 2000px;
            }
          </style>
      )HTML");
  Compositor().BeginFrame();

  ASSERT_EQ(1.25f, WebView().MinimumPageScaleFactor());

  VisualViewport& visual_viewport = WebView().GetPage()->GetVisualViewport();
  EXPECT_EQ(gfx::Size(400, 600), visual_viewport.ContainerLayer()->Size());
  EXPECT_EQ(gfx::Size(400, 600),
            visual_viewport.ContainerLayer()->CcLayer()->bounds());
  EXPECT_EQ(gfx::Size(320, 480), visual_viewport.ScrollLayer()->Size());
  EXPECT_EQ(gfx::Size(320, 480),
            visual_viewport.ScrollLayer()->CcLayer()->bounds());

  if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    EXPECT_EQ(IntSize(400, 600),
              visual_viewport.GetScrollNode()->ContainerRect().Size());
    EXPECT_EQ(IntSize(320, 480),
              visual_viewport.GetScrollNode()->ContentsSize());
  }

  WebView().MainFrameWidget()->ApplyViewportChanges(
      {gfx::ScrollOffset(1, 1), gfx::Vector2dF(), 2, false, 1,
       cc::BrowserControlsState::kBoth});
  EXPECT_EQ(gfx::Size(400, 600), visual_viewport.ContainerLayer()->Size());
  EXPECT_EQ(gfx::Size(400, 600),
            visual_viewport.ContainerLayer()->CcLayer()->bounds());
  EXPECT_EQ(gfx::Size(320, 480), visual_viewport.ScrollLayer()->Size());
  EXPECT_EQ(gfx::Size(320, 480),
            visual_viewport.ScrollLayer()->CcLayer()->bounds());

  if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    EXPECT_EQ(IntSize(400, 600),
              visual_viewport.GetScrollNode()->ContainerRect().Size());
    EXPECT_EQ(IntSize(320, 480),
              visual_viewport.GetScrollNode()->ContentsSize());
  }
}

TEST_P(VisualViewportTest, DeviceEmulationTransformNode) {
  InitializeWithAndroidSettings();

  WebDeviceEmulationParams params;
  params.viewport_offset = WebFloatPoint(314, 159);
  params.viewport_scale = 1.f;
  WebView()->EnableDeviceEmulation(params);

  WebView()->MainFrameWidget()->Resize(IntSize(400, 400));
  NavigateTo("about:blank");
  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  TransformationMatrix expected_transform = TransformationMatrix();
  expected_transform.Translate(-params.viewport_offset.x,
                               -params.viewport_offset.y);
  EXPECT_EQ(expected_transform.To2DTranslation(),
            visual_viewport.GetDeviceEmulationTransformNode()->Translation2D());

  // Set an identity device emulation transform and ensure the transform
  // paint property node is cleared.
  WebView()->EnableDeviceEmulation(WebDeviceEmulationParams());
  UpdateAllLifecyclePhases();
  EXPECT_EQ(visual_viewport.GetDeviceEmulationTransformNode(), nullptr);
}

// When a pinch-zoom occurs, the viewport scale and translation nodes can be
// directly updated without a PaintArtifactCompositor update.
TEST_P(VisualViewportTest, DirectPinchZoomPropertyUpdate) {
  // TODO(crbug.com/953322): Implement this optimization for
  // CompositeAfterPaint.
  if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() ||
      RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  InitializeWithAndroidSettings();

  RegisterMockedHttpURLLoad("200-by-800-viewport.html");
  NavigateTo(base_url_ + "200-by-800-viewport.html");

  WebView()->MainFrameWidget()->Resize(IntSize(100, 200));

  // Scroll visual viewport to the right edge of the frame
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetScaleAndLocation(2.f, true, FloatPoint(150, 10));

  EXPECT_FLOAT_SIZE_EQ(FloatSize(150, 10), visual_viewport.GetScrollOffset());
  EXPECT_EQ(2.f, visual_viewport.Scale());
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Update the scale and location and ensure that a PaintArtifactCompositor
  // update is not required.
  visual_viewport.SetScaleAndLocation(3.f, true, FloatPoint(120, 10));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  EXPECT_FLOAT_SIZE_EQ(FloatSize(120, 10), visual_viewport.GetScrollOffset());
  EXPECT_EQ(3.f, visual_viewport.Scale());
}

// |TransformPaintPropertyNode::in_subtree_of_page_scale| should be false for
// the page scale transform node and all ancestors, and should be true for
// descendants of the page scale transform node.
TEST_P(VisualViewportTest, InSubtreeOfPageScale) {
  InitializeWithAndroidSettings();
  RegisterMockedHttpURLLoad("200-by-800-viewport.html");
  NavigateTo(base_url_ + "200-by-800-viewport.html");

  UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  const auto* page_scale = visual_viewport.GetPageScaleNode();
  // The page scale is not in its own subtree.
  EXPECT_FALSE(page_scale->IsInSubtreeOfPageScale());
  // Ancestors of the page scale are not in the page scale's subtree.
  for (const auto* ancestor = page_scale->Parent(); ancestor;
       ancestor = ancestor->Parent()) {
    EXPECT_FALSE(ancestor->IsInSubtreeOfPageScale());
  }

  const auto* view = GetFrame()->View()->GetLayoutView();
  const auto& view_contents_transform =
      view->FirstFragment().ContentsProperties().Transform();
  // Descendants of the page scale node should have |IsInSubtreeOfPageScale|.
  EXPECT_TRUE(view_contents_transform.IsInSubtreeOfPageScale());
  for (const auto* ancestor = view_contents_transform.Parent();
       ancestor != page_scale; ancestor = ancestor->Parent()) {
    EXPECT_TRUE(ancestor->IsInSubtreeOfPageScale());
  }
}

}  // namespace
}  // namespace blink
