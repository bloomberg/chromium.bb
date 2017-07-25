// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/VisualViewport.h"

#include <memory>

#include "core/dom/Document.h"
#include "core/frame/BrowserControls.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayer.h"
#include "platform/geometry/DoublePoint.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebCoalescedInputEvent.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebContextMenuData.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebSettings.h"
#include "public/web/WebViewClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <string>

using ::testing::_;
using ::testing::PrintToString;
using ::testing::Mock;
using blink::URLTestHelpers::ToKURL;

namespace blink {

::std::ostream& operator<<(::std::ostream& os, const WebContextMenuData& data) {
  return os << "Context menu location: [" << data.mouse_position.x << ", "
            << data.mouse_position.y << "]";
}

namespace {

void configureAndroidCompositing(WebSettings* settings) {
  settings->SetAcceleratedCompositingEnabled(true);
  settings->SetPreferCompositingToLCDTextEnabled(true);
  settings->SetViewportMetaEnabled(true);
  settings->SetViewportEnabled(true);
  settings->SetMainFrameResizesAreOrientationChanges(true);
  settings->SetShrinksViewportContentToFit(true);
}

typedef bool TestParamRootLayerScrolling;
class VisualViewportTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest {
 public:
  VisualViewportTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        base_url_("http://www.test.com/") {}

  void InitializeWithDesktopSettings(
      void (*override_settings_func)(WebSettings*) = 0) {
    if (!override_settings_func)
      override_settings_func = &ConfigureSettings;
    helper_.Initialize(nullptr, &mock_web_view_client_, nullptr,
                       override_settings_func);
    WebViewImpl()->SetDefaultPageScaleLimits(1, 4);
  }

  void InitializeWithAndroidSettings(
      void (*override_settings_func)(WebSettings*) = 0) {
    if (!override_settings_func)
      override_settings_func = &ConfigureAndroidSettings;
    helper_.Initialize(nullptr, &mock_web_view_client_, nullptr,
                       override_settings_func);
    WebViewImpl()->SetDefaultPageScaleLimits(0.25f, 5);
  }

  ~VisualViewportTest() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void NavigateTo(const std::string& url) {
    FrameTestHelpers::LoadFrame(WebViewImpl()->MainFrameImpl(), url);
  }

  void ForceFullCompositingUpdate() {
    WebViewImpl()->UpdateAllLifecyclePhases();
  }

  void RegisterMockedHttpURLLoad(const std::string& fileName) {
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), blink::testing::CoreTestDataPath(),
        WebString::FromUTF8(fileName));
  }

  void RegisterMockedHttpURLLoad(const std::string& url,
                                 const std::string& fileName) {
    URLTestHelpers::RegisterMockedURLLoad(
        ToKURL(url),
        blink::testing::CoreTestDataPath(WebString::FromUTF8(fileName)));
  }

  WebViewBase* WebViewImpl() const { return helper_.WebView(); }
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

 protected:
  std::string base_url_;
  FrameTestHelpers::TestWebViewClient mock_web_view_client_;

 private:
  FrameTestHelpers::WebViewHelper helper_;
};

INSTANTIATE_TEST_CASE_P(All, VisualViewportTest, ::testing::Bool());

// Test that resizing the VisualViewport works as expected and that resizing the
// WebView resizes the VisualViewport.
TEST_P(VisualViewportTest, TestResize) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(320, 240));

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  IntSize web_view_size = WebViewImpl()->Size();

  // Make sure the visual viewport was initialized.
  EXPECT_SIZE_EQ(web_view_size, visual_viewport.Size());

  // Resizing the WebView should change the VisualViewport.
  web_view_size = IntSize(640, 480);
  WebViewImpl()->Resize(web_view_size);
  EXPECT_SIZE_EQ(web_view_size, IntSize(WebViewImpl()->Size()));
  EXPECT_SIZE_EQ(web_view_size, visual_viewport.Size());

  // Resizing the visual viewport shouldn't affect the WebView.
  IntSize new_viewport_size = IntSize(320, 200);
  visual_viewport.SetSize(new_viewport_size);
  EXPECT_SIZE_EQ(web_view_size, IntSize(WebViewImpl()->Size()));
  EXPECT_SIZE_EQ(new_viewport_size, visual_viewport.Size());
}

// Make sure that the visibleContentRect method acurately reflects the scale and
// scroll location of the viewport with and without scrollbars.
TEST_P(VisualViewportTest, TestVisibleContentRect) {
  RuntimeEnabledFeatures::SetOverlayScrollbarsEnabled(false);
  InitializeWithDesktopSettings();

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  IntSize size = IntSize(150, 100);
  // Vertical scrollbar width and horizontal scrollbar height.
  IntSize scrollbar_size = IntSize(15, 15);

  WebViewImpl()->Resize(size);

  // Scroll layout viewport and verify visibleContentRect.
  WebViewImpl()->MainFrameImpl()->SetScrollOffset(WebSize(0, 50));

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_EQ(IntRect(IntPoint(0, 0), size - scrollbar_size),
            visual_viewport.VisibleContentRect(kExcludeScrollbars));
  EXPECT_EQ(IntRect(IntPoint(0, 0), size),
            visual_viewport.VisibleContentRect(kIncludeScrollbars));

  WebViewImpl()->SetPageScaleFactor(2.0);

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
  WebViewImpl()->Resize(IntSize(800, 600));

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  visual_viewport.SetScale(2);

  // Fully scroll both viewports.
  frame_view.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(10000, 10000), kProgrammaticScroll);
  visual_viewport.Move(FloatSize(10000, 10000));

  // Sanity check.
  ASSERT_SIZE_EQ(FloatSize(400, 300), visual_viewport.GetScrollOffset());
  ASSERT_SIZE_EQ(ScrollOffset(200, 1400),
                 frame_view.LayoutViewportScrollableArea()->GetScrollOffset());

  IntPoint expected_location =
      frame_view.GetScrollableArea()->VisibleContentRect().Location();

  // Shrink the WebView, this should cause both viewports to shrink and
  // WebView should do whatever it needs to do to preserve the visible
  // location.
  WebViewImpl()->Resize(IntSize(700, 550));

  EXPECT_POINT_EQ(
      expected_location,
      frame_view.GetScrollableArea()->VisibleContentRect().Location());

  WebViewImpl()->Resize(IntSize(800, 600));

  EXPECT_POINT_EQ(
      expected_location,
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

  WebViewImpl()->Resize(IntSize(100, 200));

  // Scroll main frame to the bottom of the document
  WebViewImpl()->MainFrameImpl()->SetScrollOffset(WebSize(0, 400));
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 400),
      GetFrame()->View()->LayoutViewportScrollableArea()->GetScrollOffset());

  WebViewImpl()->SetPageScaleFactor(2.0);

  // Scroll visual viewport to the bottom of the main frame
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetLocation(FloatPoint(0, 300));
  EXPECT_FLOAT_SIZE_EQ(FloatSize(0, 300), visual_viewport.GetScrollOffset());

  // Verify the initial size of the visual viewport in the CSS pixels
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 100),
                       visual_viewport.VisibleRect().Size());

  // Perform the resizing
  WebViewImpl()->Resize(IntSize(200, 100));

  // After resizing the scale changes 2.0 -> 4.0
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 25), visual_viewport.VisibleRect().Size());

  EXPECT_SIZE_EQ(
      ScrollOffset(0, 625),
      GetFrame()->View()->LayoutViewportScrollableArea()->GetScrollOffset());
  EXPECT_FLOAT_SIZE_EQ(FloatSize(0, 75), visual_viewport.GetScrollOffset());
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

  WebViewImpl()->Resize(IntSize(100, 200));

  // Outer viewport takes the whole width of the document.

  WebViewImpl()->SetPageScaleFactor(2.0);

  // Scroll visual viewport to the right edge of the frame
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetLocation(FloatPoint(150, 0));
  EXPECT_FLOAT_SIZE_EQ(FloatSize(150, 0), visual_viewport.GetScrollOffset());

  // Verify the initial size of the visual viewport in the CSS pixels
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 100),
                       visual_viewport.VisibleRect().Size());

  WebViewImpl()->Resize(IntSize(200, 100));

  // After resizing the scale changes 2.0 -> 4.0
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 25), visual_viewport.VisibleRect().Size());

  EXPECT_SIZE_EQ(ScrollOffset(0, 0), GetFrame()->View()->GetScrollOffset());
  EXPECT_FLOAT_SIZE_EQ(FloatSize(150, 0), visual_viewport.GetScrollOffset());
}

// Test that the container layer gets sized properly if the WebView is resized
// prior to the VisualViewport being attached to the layer tree.
TEST_P(VisualViewportTest, TestWebViewResizedBeforeAttachment) {
  InitializeWithDesktopSettings();
  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();

  // Make sure that a resize that comes in while there's no root layer is
  // honoured when we attach to the layer tree.
  WebFrameWidgetBase* main_frame_widget =
      WebViewImpl()->MainFrameImpl()->FrameWidget();
  main_frame_widget->SetRootGraphicsLayer(nullptr);
  WebViewImpl()->Resize(IntSize(320, 240));

  NavigateTo("about:blank");
  WebViewImpl()->UpdateAllLifecyclePhases();
  main_frame_widget->SetRootGraphicsLayer(
      frame_view.GetLayoutViewItem().Compositor()->RootGraphicsLayer());

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_FLOAT_SIZE_EQ(FloatSize(320, 240),
                       visual_viewport.ContainerLayer()->Size());
}

// Make sure that the visibleRect method acurately reflects the scale and scroll
// location of the viewport.
TEST_P(VisualViewportTest, TestVisibleRect) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(320, 240));

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  // Initial visible rect should be the whole frame.
  EXPECT_SIZE_EQ(IntSize(WebViewImpl()->Size()), visual_viewport.Size());

  // Viewport is whole frame.
  IntSize size = IntSize(400, 200);
  WebViewImpl()->Resize(size);
  WebViewImpl()->UpdateAllLifecyclePhases();
  visual_viewport.SetSize(size);

  // Scale the viewport to 2X; size should not change.
  FloatRect expected_rect(FloatPoint(0, 0), FloatSize(size));
  expected_rect.Scale(0.5);
  visual_viewport.SetScale(2);
  EXPECT_EQ(2, visual_viewport.Scale());
  EXPECT_SIZE_EQ(size, visual_viewport.Size());
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

// Make sure that the visibleRectInDocument method acurately reflects the scale
// and scroll location of the viewport relative to the document.
TEST_P(VisualViewportTest, TestVisibleRectInDocument) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(100, 400));

  RegisterMockedHttpURLLoad("200-by-800-viewport.html");
  NavigateTo(base_url_ + "200-by-800-viewport.html");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  // Scale the viewport to 2X and move it.
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(FloatPoint(10, 15));
  EXPECT_FLOAT_RECT_EQ(FloatRect(10, 15, 50, 200),
                       visual_viewport.VisibleRectInDocument());

  // Scroll the layout viewport. Ensure its offset is reflected in
  // visibleRectInDocument().
  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();
  frame_view.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(40, 100), kProgrammaticScroll);
  EXPECT_FLOAT_RECT_EQ(FloatRect(50, 115, 50, 200),
                       visual_viewport.VisibleRectInDocument());
}

TEST_P(VisualViewportTest, TestFractionalScrollOffsetIsNotOverwritten) {
  bool orig_fractional_offsets_enabled =
      RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled();
  RuntimeEnabledFeatures::SetFractionalScrollOffsetsEnabled(true);

  InitializeWithAndroidSettings();
  WebViewImpl()->Resize(IntSize(200, 250));

  RegisterMockedHttpURLLoad("200-by-800-viewport.html");
  NavigateTo(base_url_ + "200-by-800-viewport.html");

  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();
  frame_view.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 10.5), kProgrammaticScroll);
  frame_view.LayoutViewportScrollableArea()->ScrollableArea::SetScrollOffset(
      ScrollOffset(10, 30.5), kCompositorScroll);

  EXPECT_EQ(
      30.5,
      frame_view.LayoutViewportScrollableArea()->GetScrollOffset().Height());

  RuntimeEnabledFeatures::SetFractionalScrollOffsetsEnabled(
      orig_fractional_offsets_enabled);
}

// Test that the viewport's scroll offset is always appropriately bounded such
// that the visual viewport always stays within the bounds of the main frame.
TEST_P(VisualViewportTest, TestOffsetClamping) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(320, 240));

  NavigateTo("about:blank");
  ForceFullCompositingUpdate();

  // Visual viewport should be initialized to same size as frame so no scrolling
  // possible.
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
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

  // Scale by 2x. The viewport's visible rect should now have a size of 160x120.
  visual_viewport.SetScale(2);
  FloatPoint location(10, 50);
  visual_viewport.SetLocation(location);
  EXPECT_FLOAT_POINT_EQ(location, visual_viewport.VisibleRect().Location());

  visual_viewport.SetLocation(FloatPoint(1000, 2000));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(160, 120),
                        visual_viewport.VisibleRect().Location());

  visual_viewport.SetLocation(FloatPoint(-1000, -2000));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visual_viewport.VisibleRect().Location());

  // Make sure offset gets clamped on scale out. Scale to 1.25 so the viewport
  // is 256x192.
  visual_viewport.SetLocation(FloatPoint(160, 120));
  visual_viewport.SetScale(1.25);
  EXPECT_FLOAT_POINT_EQ(FloatPoint(64, 48),
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
  WebViewImpl()->Resize(IntSize(320, 240));

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
  WebViewImpl()->Resize(IntSize(320, 240));

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
  EXPECT_SIZE_EQ(IntSize(330, 250), visual_viewport.Size());

  // Resize both the viewport and the frame to be larger.
  WebViewImpl()->Resize(IntSize(640, 480));
  WebViewImpl()->UpdateAllLifecyclePhases();
  EXPECT_SIZE_EQ(IntSize(WebViewImpl()->Size()), visual_viewport.Size());
  EXPECT_SIZE_EQ(IntSize(WebViewImpl()->Size()),
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
  WebViewImpl()->Resize(IntSize(320, 240));

  RegisterMockedHttpURLLoad("200-by-300-viewport.html");
  NavigateTo(base_url_ + "200-by-300-viewport.html");

  WebViewImpl()->Resize(IntSize(600, 800));
  WebViewImpl()->UpdateAllLifecyclePhases();

  // Note: the size is ceiled and should match the behavior in CC's
  // LayerImpl::bounds().
  EXPECT_SIZE_EQ(
      IntSize(200, 267),
      WebViewImpl()->MainFrameImpl()->GetFrameView()->FrameRect().Size());
}

// The main LocalFrameView's size should be set such that its the size of the
// visual viewport at minimum scale. On Desktop, the minimum scale is set at 1
// so make sure the LocalFrameView is sized to the viewport.
TEST_P(VisualViewportTest, TestFrameViewSizedToMinimumScale) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(320, 240));

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  WebViewImpl()->Resize(IntSize(100, 160));
  WebViewImpl()->UpdateAllLifecyclePhases();

  EXPECT_SIZE_EQ(
      IntSize(100, 160),
      WebViewImpl()->MainFrameImpl()->GetFrameView()->FrameRect().Size());
}

// Test that attaching a new frame view resets the size of the inner viewport
// scroll layer. crbug.com/423189.
TEST_P(VisualViewportTest, TestAttachingNewFrameSetsInnerScrollLayerSize) {
  InitializeWithAndroidSettings();
  WebViewImpl()->Resize(IntSize(320, 240));

  // Load a wider page first, the navigation should resize the scroll layer to
  // the smaller size on the second navigation.
  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");
  WebViewImpl()->UpdateAllLifecyclePhases();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.Move(ScrollOffset(50, 60));

  // Move and scale the viewport to make sure it gets reset in the navigation.
  EXPECT_SIZE_EQ(FloatSize(50, 60), visual_viewport.GetScrollOffset());
  EXPECT_EQ(2, visual_viewport.Scale());

  // Navigate again, this time the LocalFrameView should be smaller.
  RegisterMockedHttpURLLoad("viewport-device-width.html");
  NavigateTo(base_url_ + "viewport-device-width.html");

  // Ensure the scroll layer matches the frame view's size.
  EXPECT_SIZE_EQ(FloatSize(320, 240), visual_viewport.ScrollLayer()->Size());

  EXPECT_EQ(CompositorElementIdNamespace::kViewport,
            NamespaceFromCompositorElementId(
                visual_viewport.ScrollLayer()->GetElementId()));

  // Ensure the location and scale were reset.
  EXPECT_SIZE_EQ(FloatSize(), visual_viewport.GetScrollOffset());
  EXPECT_EQ(1, visual_viewport.Scale());
}

// The main LocalFrameView's size should be set such that its the size of the
// visual viewport at minimum scale. Test that the LocalFrameView is
// appropriately sized in the presence of a viewport <meta> tag.
TEST_P(VisualViewportTest, TestFrameViewSizedToViewportMetaMinimumScale) {
  InitializeWithAndroidSettings();
  WebViewImpl()->Resize(IntSize(320, 240));

  RegisterMockedHttpURLLoad("200-by-300-min-scale-2.html");
  NavigateTo(base_url_ + "200-by-300-min-scale-2.html");

  WebViewImpl()->Resize(IntSize(100, 160));
  WebViewImpl()->UpdateAllLifecyclePhases();

  EXPECT_SIZE_EQ(
      IntSize(50, 80),
      WebViewImpl()->MainFrameImpl()->GetFrameView()->FrameRect().Size());
}

// Test that the visual viewport still gets sized in AutoSize/AutoResize mode.
TEST_P(VisualViewportTest, TestVisualViewportGetsSizeInAutoSizeMode) {
  InitializeWithDesktopSettings();

  EXPECT_SIZE_EQ(IntSize(0, 0), IntSize(WebViewImpl()->Size()));
  EXPECT_SIZE_EQ(IntSize(0, 0),
                 GetFrame()->GetPage()->GetVisualViewport().Size());

  WebViewImpl()->EnableAutoResizeMode(WebSize(10, 10), WebSize(1000, 1000));

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  EXPECT_SIZE_EQ(IntSize(200, 300),
                 GetFrame()->GetPage()->GetVisualViewport().Size());
}

// Test that the text selection handle's position accounts for the visual
// viewport.
TEST_P(VisualViewportTest, TestTextSelectionHandles) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(500, 800));

  RegisterMockedHttpURLLoad("pinch-viewport-input-field.html");
  NavigateTo(base_url_ + "pinch-viewport-input-field.html");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  WebViewImpl()->SetInitialFocus(false);

  WebRect original_anchor;
  WebRect original_focus;
  WebViewImpl()->SelectionBounds(original_anchor, original_focus);

  WebViewImpl()->SetPageScaleFactor(2);
  visual_viewport.SetLocation(FloatPoint(100, 400));

  WebRect anchor;
  WebRect focus;
  WebViewImpl()->SelectionBounds(anchor, focus);

  IntPoint expected(IntRect(original_anchor).Location());
  expected.MoveBy(-FlooredIntPoint(visual_viewport.VisibleRect().Location()));
  expected.Scale(visual_viewport.Scale(), visual_viewport.Scale());

  EXPECT_POINT_EQ(expected, IntRect(anchor).Location());
  EXPECT_POINT_EQ(expected, IntRect(focus).Location());

  // FIXME(bokan) - http://crbug.com/364154 - Figure out how to test text
  // selection as well rather than just carret.
}

// Test that the HistoryItem for the page stores the visual viewport's offset
// and scale.
TEST_P(VisualViewportTest, TestSavedToHistoryItem) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(200, 300));
  WebViewImpl()->UpdateAllLifecyclePhases();

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  EXPECT_EQ(nullptr, ToLocalFrame(WebViewImpl()->GetPage()->MainFrame())
                         ->Loader()
                         .GetDocumentLoader()
                         ->GetHistoryItem()
                         ->GetViewState());

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);

  EXPECT_EQ(2, ToLocalFrame(WebViewImpl()->GetPage()->MainFrame())
                   ->Loader()
                   .GetDocumentLoader()
                   ->GetHistoryItem()
                   ->GetViewState()
                   ->page_scale_factor_);

  visual_viewport.SetLocation(FloatPoint(10, 20));

  EXPECT_SIZE_EQ(ScrollOffset(10, 20),
                 ToLocalFrame(WebViewImpl()->GetPage()->MainFrame())
                     ->Loader()
                     .GetDocumentLoader()
                     ->GetHistoryItem()
                     ->GetViewState()
                     ->visual_viewport_scroll_offset_);
}

// Test restoring a HistoryItem properly restores the visual viewport's state.
TEST_P(VisualViewportTest, TestRestoredFromHistoryItem) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(200, 300));

  RegisterMockedHttpURLLoad("200-by-300.html");

  WebHistoryItem item;
  item.Initialize();
  WebURL destination_url(URLTestHelpers::ToKURL(base_url_ + "200-by-300.html"));
  item.SetURLString(destination_url.GetString());
  item.SetVisualViewportScrollOffset(WebFloatPoint(100, 120));
  item.SetPageScaleFactor(2);

  FrameTestHelpers::LoadHistoryItem(WebViewImpl()->MainFrameImpl(), item,
                                    kWebHistoryDifferentDocumentLoad,
                                    WebCachePolicy::kUseProtocolCachePolicy);

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
  WebViewImpl()->Resize(IntSize(100, 150));

  RegisterMockedHttpURLLoad("200-by-300-viewport.html");

  WebHistoryItem item;
  item.Initialize();
  WebURL destination_url(
      URLTestHelpers::ToKURL(base_url_ + "200-by-300-viewport.html"));
  item.SetURLString(destination_url.GetString());
  // (-1, -1) will be used if the HistoryItem is an older version prior to
  // having visual viewport scroll offset.
  item.SetVisualViewportScrollOffset(WebFloatPoint(-1, -1));
  item.SetScrollOffset(WebPoint(120, 180));
  item.SetPageScaleFactor(2);

  FrameTestHelpers::LoadHistoryItem(WebViewImpl()->MainFrameImpl(), item,
                                    kWebHistoryDifferentDocumentLoad,
                                    WebCachePolicy::kUseProtocolCachePolicy);

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_EQ(2, visual_viewport.Scale());
  EXPECT_SIZE_EQ(
      ScrollOffset(100, 150),
      GetFrame()->View()->LayoutViewportScrollableArea()->GetScrollOffset());
  EXPECT_FLOAT_POINT_EQ(FloatPoint(20, 30),
                        visual_viewport.VisibleRect().Location());
}

// Test that navigation to a new page with a different sized main frame doesn't
// clobber the history item's main frame scroll offset. crbug.com/371867
TEST_P(VisualViewportTest,
       TestNavigateToSmallerFrameViewHistoryItemClobberBug) {
  InitializeWithAndroidSettings();
  WebViewImpl()->Resize(IntSize(400, 400));
  WebViewImpl()->UpdateAllLifecyclePhases();

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  LocalFrameView* frame_view = WebViewImpl()->MainFrameImpl()->GetFrameView();
  frame_view->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 1000), kProgrammaticScroll);

  EXPECT_SIZE_EQ(IntSize(1000, 1000), frame_view->FrameRect().Size());

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(FloatPoint(350, 350));

  Persistent<HistoryItem> firstItem = WebViewImpl()
                                          ->MainFrameImpl()
                                          ->GetFrame()
                                          ->Loader()
                                          .GetDocumentLoader()
                                          ->GetHistoryItem();
  EXPECT_SIZE_EQ(ScrollOffset(0, 1000),
                 firstItem->GetViewState()->scroll_offset_);

  // Now navigate to a page which causes a smaller frame_view. Make sure that
  // navigating doesn't cause the history item to set a new scroll offset
  // before the item was replaced.
  NavigateTo("about:blank");
  frame_view = WebViewImpl()->MainFrameImpl()->GetFrameView();

  EXPECT_NE(firstItem, WebViewImpl()
                           ->MainFrameImpl()
                           ->GetFrame()
                           ->Loader()
                           .GetDocumentLoader()
                           ->GetHistoryItem());
  EXPECT_LT(frame_view->FrameRect().Size().Width(), 1000);
  EXPECT_SIZE_EQ(ScrollOffset(0, 1000),
                 firstItem->GetViewState()->scroll_offset_);
}

// Test that the coordinates sent into moveRangeSelection are offset by the
// visual viewport's location.
TEST_P(VisualViewportTest,
       DISABLED_TestWebFrameRangeAccountsForVisualViewportScroll) {
  InitializeWithDesktopSettings();
  WebViewImpl()->GetSettings()->SetDefaultFontSize(12);
  WebViewImpl()->Resize(WebSize(640, 480));
  RegisterMockedHttpURLLoad("move_range.html");
  NavigateTo(base_url_ + "move_range.html");

  WebRect base_rect;
  WebRect extent_rect;

  WebViewImpl()->SetPageScaleFactor(2);
  WebLocalFrame* mainFrame = WebViewImpl()->MainFrameImpl();

  // Select some text and get the base and extent rects (that's the start of
  // the range and its end). Do a sanity check that the expected text is
  // selected
  mainFrame->ExecuteScript(WebScriptSource("selectRange();"));
  EXPECT_EQ("ir", mainFrame->SelectionAsText().Utf8());

  WebViewImpl()->SelectionBounds(base_rect, extent_rect);
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

// Test that the scrollFocusedEditableElementIntoRect method works with the
// visual viewport.
TEST_P(VisualViewportTest, DISABLED_TestScrollFocusedEditableElementIntoRect) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(500, 300));

  RegisterMockedHttpURLLoad("pinch-viewport-input-field.html");
  NavigateTo(base_url_ + "pinch-viewport-input-field.html");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  WebViewImpl()->ResizeVisualViewport(IntSize(200, 100));
  WebViewImpl()->SetInitialFocus(false);
  visual_viewport.SetLocation(FloatPoint());
  WebViewImpl()->ScrollFocusedEditableElementIntoRect(IntRect(0, 0, 500, 200));

  EXPECT_SIZE_EQ(
      ScrollOffset(0, GetFrame()->View()->MaximumScrollOffset().Height()),
      GetFrame()->View()->GetScrollOffset());
  EXPECT_FLOAT_POINT_EQ(FloatPoint(150, 200),
                        visual_viewport.VisibleRect().Location());

  // Try it again but with the page zoomed in
  GetFrame()->View()->SetScrollOffset(ScrollOffset(0, 0), kProgrammaticScroll);
  WebViewImpl()->ResizeVisualViewport(IntSize(500, 300));
  visual_viewport.SetLocation(FloatPoint(0, 0));

  WebViewImpl()->SetPageScaleFactor(2);
  WebViewImpl()->ScrollFocusedEditableElementIntoRect(IntRect(0, 0, 500, 200));
  EXPECT_SIZE_EQ(
      ScrollOffset(0, GetFrame()->View()->MaximumScrollOffset().Height()),
      GetFrame()->View()->GetScrollOffset());
  EXPECT_FLOAT_POINT_EQ(FloatPoint(125, 150),
                        visual_viewport.VisibleRect().Location());

  // Once more but make sure that we don't move the visual viewport unless
  // necessary.
  RegisterMockedHttpURLLoad("pinch-viewport-input-field-long-and-wide.html");
  NavigateTo(base_url_ + "pinch-viewport-input-field-long-and-wide.html");
  WebViewImpl()->SetInitialFocus(false);
  visual_viewport.SetLocation(FloatPoint());
  GetFrame()->View()->SetScrollOffset(ScrollOffset(0, 0), kProgrammaticScroll);
  WebViewImpl()->ResizeVisualViewport(IntSize(500, 300));
  visual_viewport.SetLocation(FloatPoint(30, 50));

  WebViewImpl()->SetPageScaleFactor(2);
  WebViewImpl()->ScrollFocusedEditableElementIntoRect(IntRect(0, 0, 500, 200));
  EXPECT_SIZE_EQ(ScrollOffset(200 - 30 - 75, 600 - 50 - 65),
                 GetFrame()->View()->GetScrollOffset());
  EXPECT_FLOAT_POINT_EQ(FloatPoint(30, 50),
                        visual_viewport.VisibleRect().Location());
}

// Test that resizing the WebView causes ViewportConstrained objects to
// relayout.
TEST_P(VisualViewportTest, TestWebViewResizeCausesViewportConstrainedLayout) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(500, 300));

  RegisterMockedHttpURLLoad("pinch-viewport-fixed-pos.html");
  NavigateTo(base_url_ + "pinch-viewport-fixed-pos.html");

  LayoutObject* navbar =
      GetFrame()->GetDocument()->getElementById("navbar")->GetLayoutObject();

  EXPECT_FALSE(navbar->NeedsLayout());

  GetFrame()->View()->Resize(IntSize(500, 200));

  EXPECT_TRUE(navbar->NeedsLayout());
}

class MockWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
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
  WebViewImpl()->Resize(IntSize(200, 300));

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  WebMouseEvent mouse_down_event(WebInputEvent::kMouseDown,
                                 WebInputEvent::kNoModifiers,
                                 WebInputEvent::kTimeStampForTesting);
  mouse_down_event.SetPositionInWidget(10, 10);
  mouse_down_event.SetPositionInScreen(110, 210);
  mouse_down_event.click_count = 1;
  mouse_down_event.button = WebMouseEvent::Button::kRight;

  // Corresponding release event (Windows shows context menu on release).
  WebMouseEvent mouse_up_event(mouse_down_event);
  mouse_up_event.SetType(WebInputEvent::kMouseUp);

  WebFrameClient* old_client = WebViewImpl()->MainFrameImpl()->Client();
  MockWebFrameClient mock_web_frame_client;
  EXPECT_CALL(mock_web_frame_client,
              ShowContextMenu(ContextMenuAtLocation(
                  mouse_down_event.PositionInWidget().x,
                  mouse_down_event.PositionInWidget().y)));

  // Do a sanity check with no scale applied.
  WebViewImpl()->MainFrameImpl()->SetClient(&mock_web_frame_client);
  WebViewImpl()->HandleInputEvent(WebCoalescedInputEvent(mouse_down_event));
  WebViewImpl()->HandleInputEvent(WebCoalescedInputEvent(mouse_up_event));

  Mock::VerifyAndClearExpectations(&mock_web_frame_client);
  mouse_down_event.button = WebMouseEvent::Button::kLeft;
  WebViewImpl()->HandleInputEvent(WebCoalescedInputEvent(mouse_down_event));

  // Now pinch zoom into the page and move the visual viewport. The context menu
  // should still appear at the location of the event, relative to the WebView.
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  WebViewImpl()->SetPageScaleFactor(2);
  EXPECT_CALL(mock_web_frame_client, DidChangeScrollOffset());
  visual_viewport.SetLocation(FloatPoint(60, 80));
  EXPECT_CALL(mock_web_frame_client,
              ShowContextMenu(ContextMenuAtLocation(
                  mouse_down_event.PositionInWidget().x,
                  mouse_down_event.PositionInWidget().y)));

  mouse_down_event.button = WebMouseEvent::Button::kRight;
  WebViewImpl()->HandleInputEvent(WebCoalescedInputEvent(mouse_down_event));
  WebViewImpl()->HandleInputEvent(WebCoalescedInputEvent(mouse_up_event));

  // Reset the old client so destruction can occur naturally.
  WebViewImpl()->MainFrameImpl()->SetClient(old_client);
}

// Test that the client is notified if page scroll events.
TEST_P(VisualViewportTest, TestClientNotifiedOfScrollEvents) {
  InitializeWithAndroidSettings();
  WebViewImpl()->Resize(IntSize(200, 300));

  RegisterMockedHttpURLLoad("200-by-300.html");
  NavigateTo(base_url_ + "200-by-300.html");

  WebFrameClient* old_client = WebViewImpl()->MainFrameImpl()->Client();
  MockWebFrameClient mock_web_frame_client;
  WebViewImpl()->MainFrameImpl()->SetClient(&mock_web_frame_client);

  WebViewImpl()->SetPageScaleFactor(2);
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
  WebViewImpl()->MainFrameImpl()->SetClient(old_client);
}

// Tests that calling scroll into view on a visible element doesn't cause
// a scroll due to a fractional offset. Bug crbug.com/463356.
TEST_P(VisualViewportTest, ScrollIntoViewFractionalOffset) {
  InitializeWithAndroidSettings();

  WebViewImpl()->Resize(IntSize(1000, 1000));

  RegisterMockedHttpURLLoad("scroll-into-view.html");
  NavigateTo(base_url_ + "scroll-into-view.html");

  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();
  ScrollableArea* layout_viewport_scrollable_area =
      frame_view.LayoutViewportScrollableArea();
  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  Element* inputBox = GetFrame()->GetDocument()->getElementById("box");

  WebViewImpl()->SetPageScaleFactor(2);

  // The element is already in the view so the scrollIntoView shouldn't move
  // the viewport at all.
  WebViewImpl()->SetVisualViewportOffset(WebFloatPoint(250.25f, 100.25f));
  layout_viewport_scrollable_area->SetScrollOffset(ScrollOffset(0, 900.75),
                                                   kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  EXPECT_SIZE_EQ(ScrollOffset(0, 900),
                 layout_viewport_scrollable_area->GetScrollOffset());
  EXPECT_SIZE_EQ(FloatSize(250.25f, 100.25f),
                 visual_viewport.GetScrollOffset());

  // Change the fractional part of the frameview to one that would round down.
  layout_viewport_scrollable_area->SetScrollOffset(ScrollOffset(0, 900.125),
                                                   kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  EXPECT_SIZE_EQ(ScrollOffset(0, 900),
                 layout_viewport_scrollable_area->GetScrollOffset());
  EXPECT_SIZE_EQ(FloatSize(250.25f, 100.25f),
                 visual_viewport.GetScrollOffset());

  // Repeat both tests above with the visual viewport at a high fractional.
  WebViewImpl()->SetVisualViewportOffset(WebFloatPoint(250.875f, 100.875f));
  layout_viewport_scrollable_area->SetScrollOffset(ScrollOffset(0, 900.75),
                                                   kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  EXPECT_SIZE_EQ(ScrollOffset(0, 900),
                 layout_viewport_scrollable_area->GetScrollOffset());
  EXPECT_SIZE_EQ(FloatSize(250.875f, 100.875f),
                 visual_viewport.GetScrollOffset());

  // Change the fractional part of the frameview to one that would round down.
  layout_viewport_scrollable_area->SetScrollOffset(ScrollOffset(0, 900.125),
                                                   kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  EXPECT_SIZE_EQ(ScrollOffset(0, 900),
                 layout_viewport_scrollable_area->GetScrollOffset());
  EXPECT_SIZE_EQ(FloatSize(250.875f, 100.875f),
                 visual_viewport.GetScrollOffset());

  // Both viewports with a 0.5 fraction.
  WebViewImpl()->SetVisualViewportOffset(WebFloatPoint(250.5f, 100.5f));
  layout_viewport_scrollable_area->SetScrollOffset(ScrollOffset(0, 900.5),
                                                   kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  EXPECT_SIZE_EQ(ScrollOffset(0, 900),
                 layout_viewport_scrollable_area->GetScrollOffset());
  EXPECT_SIZE_EQ(FloatSize(250.5f, 100.5f), visual_viewport.GetScrollOffset());
}

static ScrollOffset expectedMaxFrameViewScrollOffset(
    VisualViewport& visual_viewport,
    LocalFrameView& frame_view) {
  float aspect_ratio = visual_viewport.VisibleRect().Width() /
                       visual_viewport.VisibleRect().Height();
  float new_height = frame_view.FrameRect().Width() / aspect_ratio;
  return ScrollOffset(
      frame_view.ContentsSize().Width() - frame_view.FrameRect().Width(),
      frame_view.ContentsSize().Height() - new_height);
}

TEST_P(VisualViewportTest, TestBrowserControlsAdjustment) {
  InitializeWithAndroidSettings();
  WebViewImpl()->ResizeWithBrowserControls(IntSize(500, 450), 20, 0, false);

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();

  visual_viewport.SetScale(1);
  EXPECT_SIZE_EQ(IntSize(500, 450), visual_viewport.VisibleRect().Size());
  EXPECT_SIZE_EQ(IntSize(1000, 900), frame_view.FrameRect().Size());

  // Simulate bringing down the browser controls by 20px.
  WebViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 1, 1);
  EXPECT_SIZE_EQ(IntSize(500, 430), visual_viewport.VisibleRect().Size());

  // Test that the scroll bounds are adjusted appropriately: the visual viewport
  // should be shrunk by 20px to 430px. The outer viewport was shrunk to
  // maintain the
  // aspect ratio so it's height is 860px.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_SIZE_EQ(FloatSize(500, 860 - 430), visual_viewport.GetScrollOffset());

  // The outer viewport (LocalFrameView) should be affected as well.
  frame_view.LayoutViewportScrollableArea()->ScrollBy(
      ScrollOffset(10000, 10000), kUserScroll);
  EXPECT_SIZE_EQ(expectedMaxFrameViewScrollOffset(visual_viewport, frame_view),
                 frame_view.LayoutViewportScrollableArea()->GetScrollOffset());

  // Simulate bringing up the browser controls by 10.5px.
  WebViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 1, -10.5f / 20);
  EXPECT_FLOAT_SIZE_EQ(FloatSize(500, 440.5f),
                       visual_viewport.VisibleRect().Size());

  // maximumScrollPosition |ceil|s the browser controls adjustment.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_FLOAT_SIZE_EQ(FloatSize(500, 881 - 441),
                       visual_viewport.GetScrollOffset());

  // The outer viewport (LocalFrameView) should be affected as well.
  frame_view.LayoutViewportScrollableArea()->ScrollBy(
      ScrollOffset(10000, 10000), kUserScroll);
  EXPECT_SIZE_EQ(expectedMaxFrameViewScrollOffset(visual_viewport, frame_view),
                 frame_view.LayoutViewportScrollableArea()->GetScrollOffset());
}

TEST_P(VisualViewportTest, TestBrowserControlsAdjustmentWithScale) {
  InitializeWithAndroidSettings();
  WebViewImpl()->ResizeWithBrowserControls(IntSize(500, 450), 20, 0, false);

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();

  visual_viewport.SetScale(2);
  EXPECT_SIZE_EQ(IntSize(250, 225), visual_viewport.VisibleRect().Size());
  EXPECT_SIZE_EQ(IntSize(1000, 900), frame_view.FrameRect().Size());

  // Simulate bringing down the browser controls by 20px. Since we're zoomed in,
  // the browser controls take up half as much space (in document-space) than
  // they do at an unzoomed level.
  WebViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 1, 1);
  EXPECT_SIZE_EQ(IntSize(250, 215), visual_viewport.VisibleRect().Size());

  // Test that the scroll bounds are adjusted appropriately.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_SIZE_EQ(FloatSize(750, 860 - 215), visual_viewport.GetScrollOffset());

  // The outer viewport (LocalFrameView) should be affected as well.
  frame_view.LayoutViewportScrollableArea()->ScrollBy(
      ScrollOffset(10000, 10000), kUserScroll);
  ScrollOffset expected =
      expectedMaxFrameViewScrollOffset(visual_viewport, frame_view);
  EXPECT_SIZE_EQ(expected,
                 frame_view.LayoutViewportScrollableArea()->GetScrollOffset());

  // Scale back out, LocalFrameView max scroll shouldn't have changed. Visual
  // viewport should be moved up to accomodate larger view.
  WebViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 0.5f, 0);
  EXPECT_EQ(1, visual_viewport.Scale());
  EXPECT_SIZE_EQ(expected,
                 frame_view.LayoutViewportScrollableArea()->GetScrollOffset());
  frame_view.LayoutViewportScrollableArea()->ScrollBy(
      ScrollOffset(10000, 10000), kUserScroll);
  EXPECT_SIZE_EQ(expected,
                 frame_view.LayoutViewportScrollableArea()->GetScrollOffset());

  EXPECT_SIZE_EQ(FloatSize(500, 860 - 430), visual_viewport.GetScrollOffset());
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_SIZE_EQ(FloatSize(500, 860 - 430), visual_viewport.GetScrollOffset());

  // Scale out, use a scale that causes fractional rects.
  WebViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 0.8f, -1);
  EXPECT_SIZE_EQ(FloatSize(625, 562.5), visual_viewport.VisibleRect().Size());

  // Bring out the browser controls by 11
  WebViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 1, 11 / 20.f);
  EXPECT_SIZE_EQ(FloatSize(625, 548.75), visual_viewport.VisibleRect().Size());

  // Ensure max scroll offsets are updated properly.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  EXPECT_FLOAT_SIZE_EQ(FloatSize(375, 877.5 - 548.75),
                       visual_viewport.GetScrollOffset());

  frame_view.LayoutViewportScrollableArea()->ScrollBy(
      ScrollOffset(10000, 10000), kUserScroll);
  EXPECT_SIZE_EQ(expectedMaxFrameViewScrollOffset(visual_viewport, frame_view),
                 frame_view.LayoutViewportScrollableArea()->GetScrollOffset());
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
  WebViewImpl()->ResizeWithBrowserControls(
      WebSize(500, visual_viewport_height - browser_controls_height), 20, 0,
      true);
  WebViewImpl()->GetBrowserControls().SetShownRatio(1);

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();

  visual_viewport.SetScale(page_scale);
  EXPECT_SIZE_EQ(
      IntSize(250,
              (visual_viewport_height - browser_controls_height) / page_scale),
      visual_viewport.VisibleRect().Size());
  EXPECT_SIZE_EQ(IntSize(1000, layout_viewport_height -
                                   browser_controls_height / min_page_scale),
                 frame_view.FrameRect().Size());
  EXPECT_SIZE_EQ(IntSize(500, visual_viewport_height - browser_controls_height),
                 visual_viewport.Size());

  // Scroll all the way to the bottom, hiding the browser controls in the
  // process.
  visual_viewport.Move(ScrollOffset(10000, 10000));
  frame_view.LayoutViewportScrollableArea()->ScrollBy(
      ScrollOffset(10000, 10000), kUserScroll);
  WebViewImpl()->GetBrowserControls().SetShownRatio(0);

  EXPECT_SIZE_EQ(IntSize(250, visual_viewport_height / page_scale),
                 visual_viewport.VisibleRect().Size());

  ScrollOffset frame_view_expected =
      expectedMaxFrameViewScrollOffset(visual_viewport, frame_view);
  ScrollOffset visual_viewport_expected = ScrollOffset(
      750, layout_viewport_height - visual_viewport_height / page_scale);

  EXPECT_SIZE_EQ(visual_viewport_expected, visual_viewport.GetScrollOffset());
  EXPECT_SIZE_EQ(frame_view_expected,
                 frame_view.LayoutViewportScrollableArea()->GetScrollOffset());

  ScrollOffset total_expected = visual_viewport_expected + frame_view_expected;

  // Resize the widget to match the browser controls adjustment. Ensure that the
  // total offset (i.e. what the user sees) doesn't change because of clamping
  // the offsets to valid values.
  WebViewImpl()->ResizeWithBrowserControls(WebSize(500, visual_viewport_height),
                                           20, 0, false);

  EXPECT_SIZE_EQ(IntSize(500, visual_viewport_height), visual_viewport.Size());
  EXPECT_SIZE_EQ(IntSize(250, visual_viewport_height / page_scale),
                 visual_viewport.VisibleRect().Size());
  EXPECT_SIZE_EQ(IntSize(1000, layout_viewport_height),
                 frame_view.FrameRect().Size());
  EXPECT_SIZE_EQ(
      total_expected,
      visual_viewport.GetScrollOffset() +
          frame_view.LayoutViewportScrollableArea()->GetScrollOffset());
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
  WebViewImpl()->ResizeWithBrowserControls(IntSize(500, visual_viewport_height),
                                           20, 0, false);
  WebViewImpl()->GetBrowserControls().SetShownRatio(0);

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();

  visual_viewport.SetScale(page_scale);
  EXPECT_SIZE_EQ(IntSize(250, visual_viewport_height / page_scale),
                 visual_viewport.VisibleRect().Size());
  EXPECT_SIZE_EQ(IntSize(1000, layout_viewport_height),
                 frame_view.FrameRect().Size());
  EXPECT_SIZE_EQ(IntSize(500, visual_viewport_height), visual_viewport.Size());

  // Scroll all the way to the bottom, showing the the browser controls in the
  // process. (This could happen via window.scrollTo during a scroll, for
  // example).
  WebViewImpl()->GetBrowserControls().SetShownRatio(1);
  visual_viewport.Move(ScrollOffset(10000, 10000));
  frame_view.LayoutViewportScrollableArea()->ScrollBy(
      ScrollOffset(10000, 10000), kUserScroll);

  EXPECT_SIZE_EQ(
      IntSize(250,
              (visual_viewport_height - browser_controls_height) / page_scale),
      visual_viewport.VisibleRect().Size());

  ScrollOffset frame_view_expected(
      0, content_height - (layout_viewport_height -
                           browser_controls_height / min_page_scale));
  ScrollOffset visual_viewport_expected = ScrollOffset(
      750, (layout_viewport_height - browser_controls_height / min_page_scale -
            visual_viewport.VisibleRect().Height()));

  EXPECT_SIZE_EQ(visual_viewport_expected, visual_viewport.GetScrollOffset());
  EXPECT_SIZE_EQ(frame_view_expected,
                 frame_view.LayoutViewportScrollableArea()->GetScrollOffset());

  ScrollOffset total_expected = visual_viewport_expected + frame_view_expected;

  // Resize the widget to match the browser controls adjustment. Ensure that the
  // total offset (i.e. what the user sees) doesn't change because of clamping
  // the offsets to valid values.
  WebViewImpl()->ResizeWithBrowserControls(
      WebSize(500, visual_viewport_height - browser_controls_height), 20, 0,
      true);

  EXPECT_SIZE_EQ(IntSize(500, visual_viewport_height - browser_controls_height),
                 visual_viewport.Size());
  EXPECT_SIZE_EQ(
      IntSize(250,
              (visual_viewport_height - browser_controls_height) / page_scale),
      visual_viewport.VisibleRect().Size());
  EXPECT_SIZE_EQ(IntSize(1000, layout_viewport_height -
                                   browser_controls_height / min_page_scale),
                 frame_view.FrameRect().Size());
  EXPECT_SIZE_EQ(
      total_expected,
      visual_viewport.GetScrollOffset() +
          frame_view.LayoutViewportScrollableArea()->GetScrollOffset());
}

// Tests that a resize due to browser controls hiding doesn't incorrectly clamp
// the main frame's scroll offset. crbug.com/428193.
TEST_P(VisualViewportTest, TestTopControlHidingResizeDoesntClampMainFrame) {
  InitializeWithAndroidSettings();
  WebViewImpl()->ResizeWithBrowserControls(WebViewImpl()->Size(), 500, 0,
                                           false);
  WebViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 1, 1);
  WebViewImpl()->ResizeWithBrowserControls(WebSize(1000, 1000), 500, 0, true);

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  // Scroll the LocalFrameView to the bottom of the page but "hide" the browser
  // controls on the compositor side so the max scroll position should account
  // for the full viewport height.
  WebViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 1, -1);
  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();
  frame_view.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 10000), kProgrammaticScroll);
  EXPECT_EQ(
      500,
      frame_view.LayoutViewportScrollableArea()->GetScrollOffset().Height());

  // Now send the resize, make sure the scroll offset doesn't change.
  WebViewImpl()->ResizeWithBrowserControls(WebSize(1000, 1500), 500, 0, false);
  EXPECT_EQ(
      500,
      frame_view.LayoutViewportScrollableArea()->GetScrollOffset().Height());
}

static void configureHiddenScrollbarsSettings(WebSettings* settings) {
  VisualViewportTest::ConfigureAndroidSettings(settings);
  settings->SetHideScrollbars(true);
}

// Tests that scrollbar layers are not attached to the inner viewport container
// layer when hideScrollbars WebSetting is true.
TEST_P(VisualViewportTest,
       TestScrollbarsNotAttachedWhenHideScrollbarsSettingIsTrue) {
  InitializeWithAndroidSettings(configureHiddenScrollbarsSettings);
  WebViewImpl()->Resize(IntSize(100, 150));
  NavigateTo("about:blank");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_FALSE(visual_viewport.LayerForHorizontalScrollbar()->Parent());
  EXPECT_FALSE(visual_viewport.LayerForVerticalScrollbar()->Parent());
}

// Tests that scrollbar layers are attached to the inner viewport container
// layer when hideScrollbars WebSetting is false.
TEST_P(VisualViewportTest,
       TestScrollbarsAttachedWhenHideScrollbarsSettingIsFalse) {
  InitializeWithAndroidSettings();
  WebViewImpl()->Resize(IntSize(100, 150));
  NavigateTo("about:blank");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  EXPECT_TRUE(visual_viewport.LayerForHorizontalScrollbar()->Parent());
  EXPECT_TRUE(visual_viewport.LayerForVerticalScrollbar()->Parent());
}

// Tests that the layout viewport's scroll layer bounds are updated in a
// compositing change update. crbug.com/423188.
TEST_P(VisualViewportTest, TestChangingContentSizeAffectsScrollBounds) {
  InitializeWithAndroidSettings();
  WebViewImpl()->Resize(IntSize(100, 150));

  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();

  WebViewImpl()->MainFrameImpl()->ExecuteScript(
      WebScriptSource("var content = document.getElementById(\"content\");"
                      "content.style.width = \"1500px\";"
                      "content.style.height = \"2400px\";"));
  frame_view.UpdateAllLifecyclePhases();
  WebLayer* scrollLayer = frame_view.LayoutViewportScrollableArea()
                              ->LayerForScrolling()
                              ->PlatformLayer();

  EXPECT_SIZE_EQ(IntSize(1500, 2400), IntSize(scrollLayer->Bounds()));
}

// Tests that resizing the visual viepwort keeps its bounds within the outer
// viewport.
TEST_P(VisualViewportTest, ResizeVisualViewportStaysWithinOuterViewport) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(100, 200));

  NavigateTo("about:blank");
  WebViewImpl()->UpdateAllLifecyclePhases();

  WebViewImpl()->ResizeVisualViewport(IntSize(100, 100));

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  visual_viewport.Move(ScrollOffset(0, 100));

  EXPECT_EQ(100, visual_viewport.GetScrollOffset().Height());

  WebViewImpl()->ResizeVisualViewport(IntSize(100, 200));

  EXPECT_EQ(0, visual_viewport.GetScrollOffset().Height());
}

TEST_P(VisualViewportTest, ElementBoundsInViewportSpaceAccountsForViewport) {
  InitializeWithAndroidSettings();

  WebViewImpl()->Resize(IntSize(500, 800));

  RegisterMockedHttpURLLoad("pinch-viewport-input-field.html");
  NavigateTo(base_url_ + "pinch-viewport-input-field.html");

  WebViewImpl()->SetInitialFocus(false);
  Element* input_element = WebViewImpl()->FocusedElement();

  IntRect bounds = input_element->GetLayoutObject()->AbsoluteBoundingBoxRect();

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  IntPoint scrollDelta(250, 400);
  visual_viewport.SetScale(2);
  visual_viewport.SetLocation(scrollDelta);

  const IntRect bounds_in_viewport = input_element->BoundsInViewport();
  IntRect expectedBounds = bounds;
  expectedBounds.Scale(2.f);
  IntPoint expectedScrollDelta = scrollDelta;
  expectedScrollDelta.Scale(2.f, 2.f);

  EXPECT_POINT_EQ(IntPoint(expectedBounds.Location() - expectedScrollDelta),
                  bounds_in_viewport.Location());
  EXPECT_SIZE_EQ(expectedBounds.Size(), bounds_in_viewport.Size());
}

TEST_P(VisualViewportTest, ElementVisibleBoundsInVisualViewport) {
  InitializeWithAndroidSettings();
  WebViewImpl()->Resize(IntSize(640, 1080));
  RegisterMockedHttpURLLoad("viewport-select.html");
  NavigateTo(base_url_ + "viewport-select.html");

  ASSERT_EQ(2.0f, WebViewImpl()->PageScaleFactor());
  WebViewImpl()->SetInitialFocus(false);
  Element* element = WebViewImpl()->FocusedElement();
  EXPECT_FALSE(element->VisibleBoundsInVisualViewport().IsEmpty());

  WebViewImpl()->SetPageScaleFactor(4.0);
  EXPECT_TRUE(element->VisibleBoundsInVisualViewport().IsEmpty());
}

// Test that the various window.scroll and document.body.scroll properties and
// methods don't change with the visual viewport.
TEST_P(VisualViewportTest, visualViewportIsInert) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewBase* web_view_impl = web_view_helper.Initialize(
      nullptr, nullptr, nullptr, &configureAndroidCompositing);

  web_view_impl->Resize(IntSize(200, 300));

  WebURL base_url = URLTestHelpers::ToKURL("http://example.com/");
  FrameTestHelpers::LoadHTMLString(
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
  web_view_impl->UpdateAllLifecyclePhases();

  LocalDOMWindow* window =
      web_view_impl->MainFrameImpl()->GetFrame()->DomWindow();
  HTMLElement* html = toHTMLHtmlElement(window->document()->documentElement());

  ASSERT_EQ(200, window->innerWidth());
  ASSERT_EQ(300, window->innerHeight());
  ASSERT_EQ(200, html->clientWidth());
  ASSERT_EQ(300, html->clientHeight());

  VisualViewport& visual_viewport = web_view_impl->MainFrameImpl()
                                        ->GetFrame()
                                        ->GetPage()
                                        ->GetVisualViewport();
  visual_viewport.SetScale(2);

  ASSERT_EQ(100, visual_viewport.VisibleSize().Width());
  ASSERT_EQ(150, visual_viewport.VisibleSize().Height());

  EXPECT_EQ(200, window->innerWidth());
  EXPECT_EQ(300, window->innerHeight());
  EXPECT_EQ(200, html->clientWidth());
  EXPECT_EQ(300, html->clientHeight());

  visual_viewport.SetScrollOffset(ScrollOffset(10, 15), kProgrammaticScroll);

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

  WebViewImpl()->Resize(IntSize(100, 200));

  RegisterMockedHttpURLLoad("content-width-1000-min-scale.html");
  NavigateTo(base_url_ + "content-width-1000-min-scale.html");

  WebLocalFrameBase* local_frame = WebViewImpl()->MainFrameImpl();
  // The shutdown() calls are a hack to prevent this test from violating
  // invariants about frame state during navigation/detach.
  local_frame->GetFrame()->GetDocument()->Shutdown();
  local_frame->CreateFrameView();

  LocalFrameView& frame_view = *local_frame->GetFrameView();
  EXPECT_SIZE_EQ(IntSize(200, 400), frame_view.FrameRect().Size());
  frame_view.Dispose();
}

// Tests that the maximum scroll offset of the viewport can be fractional.
TEST_P(VisualViewportTest, FractionalMaxScrollOffset) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(101, 201));
  NavigateTo("about:blank");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();
  ScrollableArea* scrollable_area = &visual_viewport;

  WebViewImpl()->SetPageScaleFactor(1.0);
  EXPECT_SIZE_EQ(ScrollOffset(), scrollable_area->MaximumScrollOffset());

  WebViewImpl()->SetPageScaleFactor(2);
  EXPECT_SIZE_EQ(ScrollOffset(101. / 2., 201. / 2.),
                 scrollable_area->MaximumScrollOffset());
}

// Tests that the slow scrolling after an impl scroll on the visual viewport is
// continuous. crbug.com/453460 was caused by the impl-path not updating the
// ScrollAnimatorBase class.
TEST_P(VisualViewportTest, SlowScrollAfterImplScroll) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(800, 600));
  NavigateTo("about:blank");

  VisualViewport& visual_viewport = GetFrame()->GetPage()->GetVisualViewport();

  // Apply some scroll and scale from the impl-side.
  WebViewImpl()->ApplyViewportDeltas(WebFloatSize(300, 200), WebFloatSize(0, 0),
                                     WebFloatSize(0, 0), 2, 0);

  EXPECT_SIZE_EQ(FloatSize(300, 200), visual_viewport.GetScrollOffset());

  // Send a scroll event on the main thread path.
  WebGestureEvent gsb(WebInputEvent::kGestureScrollBegin,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::kTimeStampForTesting);
  gsb.SetFrameScale(1);
  gsb.source_device = kWebGestureDeviceTouchpad;
  gsb.data.scroll_begin.delta_x_hint = -50;
  gsb.data.scroll_begin.delta_x_hint = -60;
  gsb.data.scroll_begin.delta_hint_units = WebGestureEvent::kPrecisePixels;
  GetFrame()->GetEventHandler().HandleGestureEvent(gsb);

  WebGestureEvent gsu(WebInputEvent::kGestureScrollUpdate,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::kTimeStampForTesting);
  gsu.SetFrameScale(1);
  gsu.source_device = kWebGestureDeviceTouchpad;
  gsu.data.scroll_update.delta_x = -50;
  gsu.data.scroll_update.delta_y = -60;
  gsu.data.scroll_update.delta_units = WebGestureEvent::kPrecisePixels;
  gsu.data.scroll_update.velocity_x = 1;
  gsu.data.scroll_update.velocity_y = 1;

  GetFrame()->GetEventHandler().HandleGestureEvent(gsu);

  // The scroll sent from the impl-side must not be overwritten.
  EXPECT_SIZE_EQ(FloatSize(350, 260), visual_viewport.GetScrollOffset());
}

static void accessibilitySettings(WebSettings* settings) {
  VisualViewportTest::ConfigureSettings(settings);
  settings->SetAccessibilityEnabled(true);
}

TEST_P(VisualViewportTest, AccessibilityHitTestWhileZoomedIn) {
  InitializeWithDesktopSettings(accessibilitySettings);

  RegisterMockedHttpURLLoad("hit-test.html");
  NavigateTo(base_url_ + "hit-test.html");

  WebViewImpl()->Resize(IntSize(500, 500));
  WebViewImpl()->UpdateAllLifecyclePhases();

  WebDocument web_doc = WebViewImpl()->MainFrameImpl()->GetDocument();
  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();

  WebViewImpl()->SetPageScaleFactor(2);
  WebViewImpl()->SetVisualViewportOffset(WebFloatPoint(200, 230));
  frame_view.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(400, 1100), kProgrammaticScroll);

  // FIXME(504057): PaintLayerScrollableArea dirties the compositing state.
  ForceFullCompositingUpdate();

  // Because of where the visual viewport is located, this should hit the bottom
  // right target (target 4).
  WebAXObject hitNode =
      WebAXObject::FromWebDocument(web_doc).HitTest(WebPoint(154, 165));
  WebAXNameFrom name_from;
  WebVector<WebAXObject> name_objects;
  EXPECT_EQ(std::string("Target4"),
            hitNode.GetName(name_from, name_objects).Utf8());
}

// Tests that the maximum scroll offset of the viewport can be fractional.
TEST_P(VisualViewportTest, TestCoordinateTransforms) {
  InitializeWithAndroidSettings();
  WebViewImpl()->Resize(IntSize(800, 600));
  RegisterMockedHttpURLLoad("content-width-1000.html");
  NavigateTo(base_url_ + "content-width-1000.html");

  VisualViewport& visual_viewport =
      WebViewImpl()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();

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
  frame_view.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(100, 120), kProgrammaticScroll);
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
  WebViewImpl()->Resize(IntSize(800, 600));
  NavigateTo(base_url_ + "window_dimensions.html");

  Element* output = GetFrame()->GetDocument()->getElementById("output");
  DCHECK(output);
  EXPECT_EQ(std::string("1600x1200"),
            std::string(output->innerHTML().Ascii().data()));
}

// Similar to above but make sure the initial scale is updated with the content
// width for a very wide page. That is, make that innerWidth/Height actually
// trigger a layout of the content, and not just an update of the viepwort.
// crbug.com/466718
TEST_P(VisualViewportTest, WindowDimensionsOnLoadWideContent) {
  InitializeWithAndroidSettings();
  RegisterMockedHttpURLLoad("window_dimensions_wide_div.html");
  WebViewImpl()->Resize(IntSize(800, 600));
  NavigateTo(base_url_ + "window_dimensions_wide_div.html");

  Element* output = GetFrame()->GetDocument()->getElementById("output");
  DCHECK(output);
  EXPECT_EQ(std::string("2000x1500"),
            std::string(output->innerHTML().Ascii().data()));
}

TEST_P(VisualViewportTest, PinchZoomGestureScrollsVisualViewportOnly) {
  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(100, 100));

  RegisterMockedHttpURLLoad("200-by-800-viewport.html");
  NavigateTo(base_url_ + "200-by-800-viewport.html");

  WebGestureEvent pinch_update(WebInputEvent::kGesturePinchUpdate,
                               WebInputEvent::kNoModifiers,
                               WebInputEvent::kTimeStampForTesting);
  pinch_update.source_device = kWebGestureDeviceTouchpad;
  pinch_update.x = 100;
  pinch_update.y = 100;
  pinch_update.data.pinch_update.scale = 2;
  pinch_update.data.pinch_update.zoom_disabled = false;

  WebViewImpl()->HandleInputEvent(WebCoalescedInputEvent(pinch_update));

  VisualViewport& visual_viewport =
      WebViewImpl()->GetPage()->GetVisualViewport();
  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();

  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 50), visual_viewport.GetScrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 0),
                 frame_view.LayoutViewportScrollableArea()->GetScrollOffset());
}

TEST_P(VisualViewportTest, ResizeWithScrollAnchoring) {
  bool wasScrollAnchoringEnabled =
      RuntimeEnabledFeatures::ScrollAnchoringEnabled();
  RuntimeEnabledFeatures::SetScrollAnchoringEnabled(true);

  InitializeWithDesktopSettings();
  WebViewImpl()->Resize(IntSize(800, 600));

  RegisterMockedHttpURLLoad("icb-relative-content.html");
  NavigateTo(base_url_ + "icb-relative-content.html");

  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();
  frame_view.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(700, 500), kProgrammaticScroll);
  WebViewImpl()->UpdateAllLifecyclePhases();

  WebViewImpl()->Resize(IntSize(800, 300));
  EXPECT_SIZE_EQ(ScrollOffset(700, 200),
                 frame_view.LayoutViewportScrollableArea()->GetScrollOffset());

  RuntimeEnabledFeatures::SetScrollAnchoringEnabled(wasScrollAnchoringEnabled);
}

// Ensure that resize anchoring as happens when browser controls hide/show
// affects the scrollable area that's currently set as the root scroller.
TEST_P(VisualViewportTest, ResizeAnchoringWithRootScroller) {
  bool wasRootScrollerEnabled =
      RuntimeEnabledFeatures::SetRootScrollerEnabled();
  RuntimeEnabledFeatures::SetSetRootScrollerEnabled(true);

  InitializeWithAndroidSettings();
  WebViewImpl()->Resize(IntSize(800, 600));

  RegisterMockedHttpURLLoad("root-scroller-div.html");
  NavigateTo(base_url_ + "root-scroller-div.html");

  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();

  Element* scroller = GetFrame()->GetDocument()->getElementById("rootScroller");
  NonThrowableExceptionState non_throw;
  GetFrame()->GetDocument()->setRootScroller(scroller, non_throw);

  WebViewImpl()->SetPageScaleFactor(3.f);
  frame_view.GetScrollableArea()->SetScrollOffset(ScrollOffset(0, 400),
                                                  kProgrammaticScroll);

  VisualViewport& visual_viewport =
      WebViewImpl()->GetPage()->GetVisualViewport();
  visual_viewport.SetScrollOffset(ScrollOffset(0, 400), kProgrammaticScroll);

  WebViewImpl()->Resize(IntSize(800, 500));

  EXPECT_SIZE_EQ(ScrollOffset(),
                 frame_view.LayoutViewportScrollableArea()->GetScrollOffset());

  RuntimeEnabledFeatures::SetSetRootScrollerEnabled(wasRootScrollerEnabled);
}

// Ensure that resize anchoring as happens when the device is rotated affects
// the scrollable area that's currently set as the root scroller.
TEST_P(VisualViewportTest, RotationAnchoringWithRootScroller) {
  bool wasRootScrollerEnabled =
      RuntimeEnabledFeatures::SetRootScrollerEnabled();
  RuntimeEnabledFeatures::SetSetRootScrollerEnabled(true);

  InitializeWithAndroidSettings();
  WebViewImpl()->Resize(IntSize(800, 600));

  RegisterMockedHttpURLLoad("root-scroller-div.html");
  NavigateTo(base_url_ + "root-scroller-div.html");

  LocalFrameView& frame_view = *WebViewImpl()->MainFrameImpl()->GetFrameView();

  Element* scroller = GetFrame()->GetDocument()->getElementById("rootScroller");
  NonThrowableExceptionState non_throw;
  GetFrame()->GetDocument()->setRootScroller(scroller, non_throw);
  WebViewImpl()->UpdateAllLifecyclePhases();

  scroller->setScrollTop(800);

  WebViewImpl()->Resize(IntSize(600, 800));

  EXPECT_SIZE_EQ(ScrollOffset(),
                 frame_view.LayoutViewportScrollableArea()->GetScrollOffset());
  EXPECT_EQ(600, scroller->scrollTop());

  RuntimeEnabledFeatures::SetSetRootScrollerEnabled(wasRootScrollerEnabled);
}

// Make sure a composited background-attachment:fixed background gets resized
// when using inert (non-layout affecting) browser controls.
TEST_P(VisualViewportTest, ResizeCompositedAndFixedBackground) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewBase* web_view_impl = web_view_helper.Initialize(
      nullptr, nullptr, nullptr, &configureAndroidCompositing);

  int page_width = 640;
  int page_height = 480;
  float browser_controls_height = 50.0f;
  int smallest_height = page_height - browser_controls_height;

  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, page_height),
                                           browser_controls_height, 0, false);

  RegisterMockedHttpURLLoad("http://example.com/foo.png", "white-1x1.png");
  WebURL base_url = URLTestHelpers::ToKURL("http://example.com/");
  FrameTestHelpers::LoadHTMLString(web_view_impl->MainFrameImpl(),
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
  web_view_impl->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(web_view_impl->GetPage()->MainFrame())->GetDocument();
  PaintLayerCompositor* compositor = document->GetLayoutView()->Compositor();

  GraphicsLayer* backgroundLayer = nullptr;
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    ASSERT_FALSE(compositor->NeedsFixedRootBackgroundLayer());
    backgroundLayer = document->GetLayoutView()
                          ->Layer()
                          ->GetCompositedLayerMapping()
                          ->MainGraphicsLayer();
  } else {
    ASSERT_TRUE(compositor->NeedsFixedRootBackgroundLayer());
    backgroundLayer = compositor->FixedRootBackgroundLayer();
  }
  ASSERT_TRUE(backgroundLayer);

  ASSERT_EQ(page_width, backgroundLayer->Size().Width());
  ASSERT_EQ(page_height, backgroundLayer->Size().Height());
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(smallest_height, document->View()->GetLayoutSize().Height());

  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, smallest_height),
                                           browser_controls_height, 0, true);

  // The layout size should not have changed.
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(smallest_height, document->View()->GetLayoutSize().Height());

  // The background layer's size should have changed though.
  EXPECT_EQ(page_width, backgroundLayer->Size().Width());
  EXPECT_EQ(smallest_height, backgroundLayer->Size().Height());

  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, page_height),
                                           browser_controls_height, 0, true);

  // The background layer's size should change again.
  EXPECT_EQ(page_width, backgroundLayer->Size().Width());
  EXPECT_EQ(page_height, backgroundLayer->Size().Height());
}

static void configureAndroidNonCompositing(WebSettings* settings) {
  settings->SetAcceleratedCompositingEnabled(true);
  settings->SetPreferCompositingToLCDTextEnabled(false);
  settings->SetViewportMetaEnabled(true);
  settings->SetViewportEnabled(true);
  settings->SetMainFrameResizesAreOrientationChanges(true);
  settings->SetShrinksViewportContentToFit(true);
}

// Make sure a non-composited background-attachment:fixed background gets
// resized when using inert (non-layout affecting) browser controls.
TEST_P(VisualViewportTest, ResizeNonCompositedAndFixedBackground) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewBase* web_view_impl = web_view_helper.Initialize(
      nullptr, nullptr, nullptr, &configureAndroidNonCompositing);

  int page_width = 640;
  int page_height = 480;
  float browser_controls_height = 50.0f;
  int smallest_height = page_height - browser_controls_height;

  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, page_height),
                                           browser_controls_height, 0, false);

  RegisterMockedHttpURLLoad("http://example.com/foo.png", "white-1x1.png");
  WebURL base_url = URLTestHelpers::ToKURL("http://example.com/");
  FrameTestHelpers::LoadHTMLString(web_view_impl->MainFrameImpl(),
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
  web_view_impl->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(web_view_impl->GetPage()->MainFrame())->GetDocument();
  PaintLayerCompositor* compositor = document->GetLayoutView()->Compositor();

  ASSERT_FALSE(compositor->NeedsFixedRootBackgroundLayer());
  ASSERT_FALSE(compositor->FixedRootBackgroundLayer());

  document->View()->SetTracksPaintInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, smallest_height),
                                           browser_controls_height, 0, true);

  // The layout size should not have changed.
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(smallest_height, document->View()->GetLayoutSize().Height());

  const RasterInvalidationTracking* invalidation_tracking =
      document->GetLayoutView()
          ->Layer()
          ->GraphicsLayerBacking(document->GetLayoutView())
          ->GetRasterInvalidationTracking();
  // If no invalidations occured, this will be a nullptr.
  ASSERT_TRUE(invalidation_tracking);

  const auto* raster_invalidations = &invalidation_tracking->invalidations;

  bool root_layer_scrolling = GetParam();

  // Without root-layer-scrolling, the LayoutView is the size of the document
  // content so invalidating it for background-attachment: fixed
  // overinvalidates as we should only need to invalidate the viewport size.
  // With root-layer-scrolling, we should invalidate the entire viewport height.
  int expectedHeight = root_layer_scrolling ? page_height : 1000;

  // The entire viewport should have been invalidated.
  EXPECT_EQ(1u, raster_invalidations->size());
  EXPECT_EQ(IntRect(0, 0, 640, expectedHeight),
            (*raster_invalidations)[0].rect);
  document->View()->SetTracksPaintInvalidations(false);

  invalidation_tracking = document->GetLayoutView()
                              ->Layer()
                              ->GraphicsLayerBacking(document->GetLayoutView())
                              ->GetRasterInvalidationTracking();
  ASSERT_FALSE(invalidation_tracking);

  document->View()->SetTracksPaintInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, page_height),
                                           browser_controls_height, 0, true);

  invalidation_tracking = document->GetLayoutView()
                              ->Layer()
                              ->GraphicsLayerBacking(document->GetLayoutView())
                              ->GetRasterInvalidationTracking();
  ASSERT_TRUE(invalidation_tracking);
  raster_invalidations = &invalidation_tracking->invalidations;

  // Once again, the entire page should have been invalidated.
  expectedHeight = root_layer_scrolling ? 480 : 1000;
  EXPECT_EQ(1u, raster_invalidations->size());
  EXPECT_EQ(IntRect(0, 0, 640, expectedHeight),
            (*raster_invalidations)[0].rect);

  document->View()->SetTracksPaintInvalidations(false);
}

// Make sure a browser control resize with background-attachment:not-fixed
// background doesn't cause invalidation or layout.
TEST_P(VisualViewportTest, ResizeNonFixedBackgroundNoLayoutOrInvalidation) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewBase* web_view_impl = web_view_helper.Initialize(
      nullptr, nullptr, nullptr, &configureAndroidCompositing);

  int page_width = 640;
  int page_height = 480;
  float browser_controls_height = 50.0f;
  int smallest_height = page_height - browser_controls_height;

  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, page_height),
                                           browser_controls_height, 0, false);

  RegisterMockedHttpURLLoad("http://example.com/foo.png", "white-1x1.png");
  WebURL base_url = URLTestHelpers::ToKURL("http://example.com/");
  // This time the background is the default attachment.
  FrameTestHelpers::LoadHTMLString(web_view_impl->MainFrameImpl(),
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
  web_view_impl->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(web_view_impl->GetPage()->MainFrame())->GetDocument();

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

  web_view_impl->UpdateAllLifecyclePhases();

  // Do a real resize to check for invalidations.
  document->View()->SetTracksPaintInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, smallest_height),
                                           browser_controls_height, 0, true);

  // The layout size should not have changed.
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(smallest_height, document->View()->GetLayoutSize().Height());

  const RasterInvalidationTracking* invalidation_tracking =
      document->GetLayoutView()
          ->Layer()
          ->GraphicsLayerBacking(document->GetLayoutView())
          ->GetRasterInvalidationTracking();

  // No invalidations should have occured in LocalFrameView scrolling. If
  // root-layer-scrolls is on, an invalidation is necessary for now, see the
  // comment and TODO in LocalFrameView::ViewportSizeChanged.
  // http://crbug.com/568847.
  bool root_layer_scrolling = GetParam();
  if (root_layer_scrolling)
    EXPECT_TRUE(invalidation_tracking);
  else
    EXPECT_FALSE(invalidation_tracking);

  document->View()->SetTracksPaintInvalidations(false);
}

TEST_P(VisualViewportTest, InvalidateLayoutViewWhenDocumentSmallerThanView) {
  FrameTestHelpers::WebViewHelper web_view_helper;
  WebViewBase* web_view_impl = web_view_helper.Initialize(
      nullptr, nullptr, nullptr, &configureAndroidCompositing);

  int page_width = 320;
  int page_height = 590;
  float browser_controls_height = 50.0f;
  int largest_height = page_height + browser_controls_height;

  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, page_height),
                                           browser_controls_height, 0, true);

  FrameTestHelpers::LoadFrame(web_view_impl->MainFrameImpl(), "about:blank");
  web_view_impl->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(web_view_impl->GetPage()->MainFrame())->GetDocument();

  // Do a resize to check for invalidations.
  document->View()->SetTracksPaintInvalidations(true);
  web_view_impl->ResizeWithBrowserControls(WebSize(page_width, largest_height),
                                           browser_controls_height, 0, false);

  // The layout size should not have changed.
  ASSERT_EQ(page_width, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(page_height, document->View()->GetLayoutSize().Height());

  // The entire viewport should have been invalidated.
  {
    const RasterInvalidationTracking* invalidation_tracking =
        document->GetLayoutView()
            ->Layer()
            ->GraphicsLayerBacking(document->GetLayoutView())
            ->GetRasterInvalidationTracking();
    ASSERT_TRUE(invalidation_tracking);
    const auto* raster_invalidations = &invalidation_tracking->invalidations;
    ASSERT_EQ(1u, raster_invalidations->size());
    EXPECT_EQ(IntRect(0, 0, page_width, largest_height),
              (*raster_invalidations)[0].rect);
  }

  document->View()->SetTracksPaintInvalidations(false);
}

// Make sure we don't crash when the visual viewport's height is 0. This can
// happen transiently in autoresize mode and cause a crash. This test passes if
// it doesn't crash.
TEST_P(VisualViewportTest, AutoResizeNoHeightUsesMinimumHeight) {
  InitializeWithDesktopSettings();
  WebViewImpl()->ResizeWithBrowserControls(WebSize(0, 0), 0, 0, false);
  WebViewImpl()->EnableAutoResizeMode(WebSize(25, 25), WebSize(100, 100));
  WebURL base_url = URLTestHelpers::ToKURL("http://example.com/");
  FrameTestHelpers::LoadHTMLString(WebViewImpl()->MainFrameImpl(),
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

}  // namespace
}  // namespace blink
