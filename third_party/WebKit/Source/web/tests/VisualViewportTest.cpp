// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/VisualViewport.h"

#include "core/dom/Document.h"
#include "core/frame/BrowserControls.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayer.h"
#include "platform/geometry/DoublePoint.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
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
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"

#include <string>

#define ASSERT_POINT_EQ(expected, actual)    \
  do {                                       \
    ASSERT_EQ((expected).x(), (actual).x()); \
    ASSERT_EQ((expected).y(), (actual).y()); \
  } while (false)

#define ASSERT_SIZE_EQ(expected, actual)               \
  do {                                                 \
    ASSERT_EQ((expected).Width(), (actual).Width());   \
    ASSERT_EQ((expected).Height(), (actual).Height()); \
  } while (false)

#define EXPECT_POINT_EQ(expected, actual)    \
  do {                                       \
    EXPECT_EQ((expected).X(), (actual).X()); \
    EXPECT_EQ((expected).Y(), (actual).Y()); \
  } while (false)

#define EXPECT_FLOAT_POINT_EQ(expected, actual)    \
  do {                                             \
    EXPECT_FLOAT_EQ((expected).X(), (actual).X()); \
    EXPECT_FLOAT_EQ((expected).Y(), (actual).Y()); \
  } while (false)

#define EXPECT_SIZE_EQ(expected, actual)               \
  do {                                                 \
    EXPECT_EQ((expected).Width(), (actual).Width());   \
    EXPECT_EQ((expected).Height(), (actual).Height()); \
  } while (false)

#define EXPECT_FLOAT_SIZE_EQ(expected, actual)               \
  do {                                                       \
    EXPECT_FLOAT_EQ((expected).Width(), (actual).Width());   \
    EXPECT_FLOAT_EQ((expected).Height(), (actual).Height()); \
  } while (false)

#define EXPECT_FLOAT_RECT_EQ(expected, actual)               \
  do {                                                       \
    EXPECT_FLOAT_EQ((expected).X(), (actual).X());           \
    EXPECT_FLOAT_EQ((expected).Y(), (actual).Y());           \
    EXPECT_FLOAT_EQ((expected).Width(), (actual).Width());   \
    EXPECT_FLOAT_EQ((expected).Height(), (actual).Height()); \
  } while (false)

using namespace blink;

using ::testing::_;
using ::testing::PrintToString;
using ::testing::Mock;
using blink::URLTestHelpers::ToKURL;

namespace blink {
::std::ostream& operator<<(::std::ostream& os, const WebContextMenuData& data) {
  return os << "Context menu location: [" << data.mouse_position.x << ", "
            << data.mouse_position.y << "]";
}
}

namespace {

typedef bool TestParamRootLayerScrolling;
class VisualViewportTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest {
 public:
  VisualViewportTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        m_baseURL("http://www.test.com/") {}

  void initializeWithDesktopSettings(
      void (*overrideSettingsFunc)(WebSettings*) = 0) {
    if (!overrideSettingsFunc)
      overrideSettingsFunc = &configureSettings;
    m_helper.Initialize(true, nullptr, &m_mockWebViewClient, nullptr,
                        overrideSettingsFunc);
    webViewImpl()->SetDefaultPageScaleLimits(1, 4);
  }

  void initializeWithAndroidSettings(
      void (*overrideSettingsFunc)(WebSettings*) = 0) {
    if (!overrideSettingsFunc)
      overrideSettingsFunc = &configureAndroidSettings;
    m_helper.Initialize(true, nullptr, &m_mockWebViewClient, nullptr,
                        overrideSettingsFunc);
    webViewImpl()->SetDefaultPageScaleLimits(0.25f, 5);
  }

  ~VisualViewportTest() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void navigateTo(const std::string& url) {
    FrameTestHelpers::LoadFrame(webViewImpl()->MainFrame(), url);
  }

  void forceFullCompositingUpdate() {
    webViewImpl()->UpdateAllLifecyclePhases();
  }

  void registerMockedHttpURLLoad(const std::string& fileName) {
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(m_baseURL), blink::testing::WebTestDataPath(),
        WebString::FromUTF8(fileName));
  }

  void registerMockedHttpURLLoad(const std::string& url,
                                 const std::string& fileName) {
    URLTestHelpers::RegisterMockedURLLoad(
        ToKURL(url),
        blink::testing::WebTestDataPath(WebString::FromUTF8(fileName)));
  }

  WebLayer* getRootScrollLayer() {
    PaintLayerCompositor* compositor =
        frame()->ContentLayoutItem().Compositor();
    DCHECK(compositor);
    DCHECK(compositor->ScrollLayer());

    WebLayer* webScrollLayer = compositor->ScrollLayer()->PlatformLayer();
    return webScrollLayer;
  }

  WebViewImpl* webViewImpl() const { return m_helper.WebView(); }
  LocalFrame* frame() const {
    return m_helper.WebView()->MainFrameImpl()->GetFrame();
  }

  static void configureSettings(WebSettings* settings) {
    settings->SetJavaScriptEnabled(true);
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  static void configureAndroidSettings(WebSettings* settings) {
    configureSettings(settings);
    settings->SetViewportEnabled(true);
    settings->SetViewportMetaEnabled(true);
    settings->SetShrinksViewportContentToFit(true);
    settings->SetMainFrameResizesAreOrientationChanges(true);
  }

 protected:
  std::string m_baseURL;
  FrameTestHelpers::TestWebViewClient m_mockWebViewClient;

 private:
  FrameTestHelpers::WebViewHelper m_helper;
};

INSTANTIATE_TEST_CASE_P(All, VisualViewportTest, ::testing::Bool());

// Test that resizing the VisualViewport works as expected and that resizing the
// WebView resizes the VisualViewport.
TEST_P(VisualViewportTest, TestResize) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(320, 240));

  navigateTo("about:blank");
  forceFullCompositingUpdate();

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();

  IntSize webViewSize = webViewImpl()->size();

  // Make sure the visual viewport was initialized.
  EXPECT_SIZE_EQ(webViewSize, visualViewport.size());

  // Resizing the WebView should change the VisualViewport.
  webViewSize = IntSize(640, 480);
  webViewImpl()->Resize(webViewSize);
  EXPECT_SIZE_EQ(webViewSize, IntSize(webViewImpl()->size()));
  EXPECT_SIZE_EQ(webViewSize, visualViewport.size());

  // Resizing the visual viewport shouldn't affect the WebView.
  IntSize newViewportSize = IntSize(320, 200);
  visualViewport.SetSize(newViewportSize);
  EXPECT_SIZE_EQ(webViewSize, IntSize(webViewImpl()->size()));
  EXPECT_SIZE_EQ(newViewportSize, visualViewport.size());
}

// Make sure that the visibleContentRect method acurately reflects the scale and
// scroll location of the viewport with and without scrollbars.
TEST_P(VisualViewportTest, TestVisibleContentRect) {
  RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(false);
  initializeWithDesktopSettings();

  registerMockedHttpURLLoad("200-by-300.html");
  navigateTo(m_baseURL + "200-by-300.html");

  IntSize size = IntSize(150, 100);
  // Vertical scrollbar width and horizontal scrollbar height.
  IntSize scrollbarSize = IntSize(15, 15);

  webViewImpl()->Resize(size);

  // Scroll layout viewport and verify visibleContentRect.
  webViewImpl()->MainFrame()->SetScrollOffset(WebSize(0, 50));

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  EXPECT_EQ(IntRect(IntPoint(0, 0), size - scrollbarSize),
            visualViewport.VisibleContentRect(kExcludeScrollbars));
  EXPECT_EQ(IntRect(IntPoint(0, 0), size),
            visualViewport.VisibleContentRect(kIncludeScrollbars));

  webViewImpl()->SetPageScaleFactor(2.0);

  // Scroll visual viewport and verify visibleContentRect.
  size.Scale(0.5);
  scrollbarSize.Scale(0.5);
  visualViewport.SetLocation(FloatPoint(10, 10));
  EXPECT_EQ(IntRect(IntPoint(10, 10), size - scrollbarSize),
            visualViewport.VisibleContentRect(kExcludeScrollbars));
  EXPECT_EQ(IntRect(IntPoint(10, 10), size),
            visualViewport.VisibleContentRect(kIncludeScrollbars));
}

// This tests that shrinking the WebView while the page is fully scrolled
// doesn't move the viewport up/left, it should keep the visible viewport
// unchanged from the user's perspective (shrinking the FrameView will clamp
// the VisualViewport so we need to counter scroll the FrameView to make it
// appear to stay still). This caused bugs like crbug.com/453859.
TEST_P(VisualViewportTest, TestResizeAtFullyScrolledPreservesViewportLocation) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(800, 600));

  registerMockedHttpURLLoad("content-width-1000.html");
  navigateTo(m_baseURL + "content-width-1000.html");

  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();
  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();

  visualViewport.SetScale(2);

  // Fully scroll both viewports.
  frameView.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(10000, 10000), kProgrammaticScroll);
  visualViewport.Move(FloatSize(10000, 10000));

  // Sanity check.
  ASSERT_SIZE_EQ(FloatSize(400, 300), visualViewport.GetScrollOffset());
  ASSERT_SIZE_EQ(ScrollOffset(200, 1400),
                 frameView.LayoutViewportScrollableArea()->GetScrollOffset());

  IntPoint expectedLocation =
      frameView.GetScrollableArea()->VisibleContentRect().Location();

  // Shrink the WebView, this should cause both viewports to shrink and
  // WebView should do whatever it needs to do to preserve the visible
  // location.
  webViewImpl()->Resize(IntSize(700, 550));

  EXPECT_POINT_EQ(
      expectedLocation,
      frameView.GetScrollableArea()->VisibleContentRect().Location());

  webViewImpl()->Resize(IntSize(800, 600));

  EXPECT_POINT_EQ(
      expectedLocation,
      frameView.GetScrollableArea()->VisibleContentRect().Location());
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
  initializeWithAndroidSettings();

  registerMockedHttpURLLoad("200-by-800-viewport.html");
  navigateTo(m_baseURL + "200-by-800-viewport.html");

  webViewImpl()->Resize(IntSize(100, 200));

  // Scroll main frame to the bottom of the document
  webViewImpl()->MainFrame()->SetScrollOffset(WebSize(0, 400));
  EXPECT_SIZE_EQ(
      ScrollOffset(0, 400),
      frame()->View()->LayoutViewportScrollableArea()->GetScrollOffset());

  webViewImpl()->SetPageScaleFactor(2.0);

  // Scroll visual viewport to the bottom of the main frame
  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  visualViewport.SetLocation(FloatPoint(0, 300));
  EXPECT_FLOAT_SIZE_EQ(FloatSize(0, 300), visualViewport.GetScrollOffset());

  // Verify the initial size of the visual viewport in the CSS pixels
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 100), visualViewport.VisibleRect().Size());

  // Perform the resizing
  webViewImpl()->Resize(IntSize(200, 100));

  // After resizing the scale changes 2.0 -> 4.0
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 25), visualViewport.VisibleRect().Size());

  EXPECT_SIZE_EQ(
      ScrollOffset(0, 625),
      frame()->View()->LayoutViewportScrollableArea()->GetScrollOffset());
  EXPECT_FLOAT_SIZE_EQ(FloatSize(0, 75), visualViewport.GetScrollOffset());
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
  initializeWithAndroidSettings();

  registerMockedHttpURLLoad("200-by-800-viewport.html");
  navigateTo(m_baseURL + "200-by-800-viewport.html");

  webViewImpl()->Resize(IntSize(100, 200));

  // Outer viewport takes the whole width of the document.

  webViewImpl()->SetPageScaleFactor(2.0);

  // Scroll visual viewport to the right edge of the frame
  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  visualViewport.SetLocation(FloatPoint(150, 0));
  EXPECT_FLOAT_SIZE_EQ(FloatSize(150, 0), visualViewport.GetScrollOffset());

  // Verify the initial size of the visual viewport in the CSS pixels
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 100), visualViewport.VisibleRect().Size());

  webViewImpl()->Resize(IntSize(200, 100));

  // After resizing the scale changes 2.0 -> 4.0
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 25), visualViewport.VisibleRect().Size());

  EXPECT_SIZE_EQ(ScrollOffset(0, 0), frame()->View()->GetScrollOffset());
  EXPECT_FLOAT_SIZE_EQ(FloatSize(150, 0), visualViewport.GetScrollOffset());
}

// Test that the container layer gets sized properly if the WebView is resized
// prior to the VisualViewport being attached to the layer tree.
TEST_P(VisualViewportTest, TestWebViewResizedBeforeAttachment) {
  initializeWithDesktopSettings();
  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();

  // Make sure that a resize that comes in while there's no root layer is
  // honoured when we attach to the layer tree.
  WebFrameWidgetBase* mainFrameWidget =
      webViewImpl()->MainFrameImpl()->FrameWidget();
  mainFrameWidget->SetRootGraphicsLayer(nullptr);
  webViewImpl()->Resize(IntSize(320, 240));

  navigateTo("about:blank");
  webViewImpl()->UpdateAllLifecyclePhases();
  mainFrameWidget->SetRootGraphicsLayer(
      frameView.GetLayoutViewItem().Compositor()->RootGraphicsLayer());

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  EXPECT_FLOAT_SIZE_EQ(FloatSize(320, 240),
                       visualViewport.ContainerLayer()->Size());
}

// Make sure that the visibleRect method acurately reflects the scale and scroll
// location of the viewport.
TEST_P(VisualViewportTest, TestVisibleRect) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(320, 240));

  navigateTo("about:blank");
  forceFullCompositingUpdate();

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();

  // Initial visible rect should be the whole frame.
  EXPECT_SIZE_EQ(IntSize(webViewImpl()->size()), visualViewport.size());

  // Viewport is whole frame.
  IntSize size = IntSize(400, 200);
  webViewImpl()->Resize(size);
  webViewImpl()->UpdateAllLifecyclePhases();
  visualViewport.SetSize(size);

  // Scale the viewport to 2X; size should not change.
  FloatRect expectedRect(FloatPoint(0, 0), FloatSize(size));
  expectedRect.Scale(0.5);
  visualViewport.SetScale(2);
  EXPECT_EQ(2, visualViewport.Scale());
  EXPECT_SIZE_EQ(size, visualViewport.size());
  EXPECT_FLOAT_RECT_EQ(expectedRect, visualViewport.VisibleRect());

  // Move the viewport.
  expectedRect.SetLocation(FloatPoint(5, 7));
  visualViewport.SetLocation(expectedRect.Location());
  EXPECT_FLOAT_RECT_EQ(expectedRect, visualViewport.VisibleRect());

  expectedRect.SetLocation(FloatPoint(200, 100));
  visualViewport.SetLocation(expectedRect.Location());
  EXPECT_FLOAT_RECT_EQ(expectedRect, visualViewport.VisibleRect());

  // Scale the viewport to 3X to introduce some non-int values.
  FloatPoint oldLocation = expectedRect.Location();
  expectedRect = FloatRect(FloatPoint(), FloatSize(size));
  expectedRect.Scale(1 / 3.0f);
  expectedRect.SetLocation(oldLocation);
  visualViewport.SetScale(3);
  EXPECT_FLOAT_RECT_EQ(expectedRect, visualViewport.VisibleRect());

  expectedRect.SetLocation(FloatPoint(0.25f, 0.333f));
  visualViewport.SetLocation(expectedRect.Location());
  EXPECT_FLOAT_RECT_EQ(expectedRect, visualViewport.VisibleRect());
}

// Make sure that the visibleRectInDocument method acurately reflects the scale
// and scroll location of the viewport relative to the document.
TEST_P(VisualViewportTest, TestVisibleRectInDocument) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(100, 400));

  registerMockedHttpURLLoad("200-by-800-viewport.html");
  navigateTo(m_baseURL + "200-by-800-viewport.html");

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();

  // Scale the viewport to 2X and move it.
  visualViewport.SetScale(2);
  visualViewport.SetLocation(FloatPoint(10, 15));
  EXPECT_FLOAT_RECT_EQ(FloatRect(10, 15, 50, 200),
                       visualViewport.VisibleRectInDocument());

  // Scroll the layout viewport. Ensure its offset is reflected in
  // visibleRectInDocument().
  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();
  frameView.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(40, 100), kProgrammaticScroll);
  EXPECT_FLOAT_RECT_EQ(FloatRect(50, 115, 50, 200),
                       visualViewport.VisibleRectInDocument());
}

TEST_P(VisualViewportTest, TestFractionalScrollOffsetIsNotOverwritten) {
  bool origFractionalOffsetsEnabled =
      RuntimeEnabledFeatures::fractionalScrollOffsetsEnabled();
  RuntimeEnabledFeatures::setFractionalScrollOffsetsEnabled(true);

  initializeWithAndroidSettings();
  webViewImpl()->Resize(IntSize(200, 250));

  registerMockedHttpURLLoad("200-by-800-viewport.html");
  navigateTo(m_baseURL + "200-by-800-viewport.html");

  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();
  frameView.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 10.5), kProgrammaticScroll);
  frameView.LayoutViewportScrollableArea()->ScrollableArea::SetScrollOffset(
      ScrollOffset(10, 30.5), kCompositorScroll);

  EXPECT_EQ(
      30.5,
      frameView.LayoutViewportScrollableArea()->GetScrollOffset().Height());

  RuntimeEnabledFeatures::setFractionalScrollOffsetsEnabled(
      origFractionalOffsetsEnabled);
}

// Test that the viewport's scroll offset is always appropriately bounded such
// that the visual viewport always stays within the bounds of the main frame.
TEST_P(VisualViewportTest, TestOffsetClamping) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(320, 240));

  navigateTo("about:blank");
  forceFullCompositingUpdate();

  // Visual viewport should be initialized to same size as frame so no scrolling
  // possible.
  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());

  visualViewport.SetLocation(FloatPoint(-1, -2));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());

  visualViewport.SetLocation(FloatPoint(100, 200));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());

  visualViewport.SetLocation(FloatPoint(-5, 10));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());

  // Scale by 2x. The viewport's visible rect should now have a size of 160x120.
  visualViewport.SetScale(2);
  FloatPoint location(10, 50);
  visualViewport.SetLocation(location);
  EXPECT_FLOAT_POINT_EQ(location, visualViewport.VisibleRect().Location());

  visualViewport.SetLocation(FloatPoint(1000, 2000));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(160, 120),
                        visualViewport.VisibleRect().Location());

  visualViewport.SetLocation(FloatPoint(-1000, -2000));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());

  // Make sure offset gets clamped on scale out. Scale to 1.25 so the viewport
  // is 256x192.
  visualViewport.SetLocation(FloatPoint(160, 120));
  visualViewport.SetScale(1.25);
  EXPECT_FLOAT_POINT_EQ(FloatPoint(64, 48),
                        visualViewport.VisibleRect().Location());

  // Scale out smaller than 1.
  visualViewport.SetScale(0.25);
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());
}

// Test that the viewport can be scrolled around only within the main frame in
// the presence of viewport resizes, as would be the case if the on screen
// keyboard came up.
TEST_P(VisualViewportTest, TestOffsetClampingWithResize) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(320, 240));

  navigateTo("about:blank");
  forceFullCompositingUpdate();

  // Visual viewport should be initialized to same size as frame so no scrolling
  // possible.
  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());

  // Shrink the viewport vertically. The resize shouldn't affect the location,
  // but it should allow vertical scrolling.
  visualViewport.SetSize(IntSize(320, 200));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());
  visualViewport.SetLocation(FloatPoint(10, 20));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 20),
                        visualViewport.VisibleRect().Location());
  visualViewport.SetLocation(FloatPoint(0, 100));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 40),
                        visualViewport.VisibleRect().Location());
  visualViewport.SetLocation(FloatPoint(0, 10));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 10),
                        visualViewport.VisibleRect().Location());
  visualViewport.SetLocation(FloatPoint(0, -100));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());

  // Repeat the above but for horizontal dimension.
  visualViewport.SetSize(IntSize(280, 240));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());
  visualViewport.SetLocation(FloatPoint(10, 20));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 0),
                        visualViewport.VisibleRect().Location());
  visualViewport.SetLocation(FloatPoint(100, 0));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(40, 0),
                        visualViewport.VisibleRect().Location());
  visualViewport.SetLocation(FloatPoint(10, 0));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 0),
                        visualViewport.VisibleRect().Location());
  visualViewport.SetLocation(FloatPoint(-100, 0));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());

  // Now with both dimensions.
  visualViewport.SetSize(IntSize(280, 200));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());
  visualViewport.SetLocation(FloatPoint(10, 20));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 20),
                        visualViewport.VisibleRect().Location());
  visualViewport.SetLocation(FloatPoint(100, 100));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(40, 40),
                        visualViewport.VisibleRect().Location());
  visualViewport.SetLocation(FloatPoint(10, 3));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(10, 3),
                        visualViewport.VisibleRect().Location());
  visualViewport.SetLocation(FloatPoint(-10, -4));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());
}

// Test that the viewport is scrollable but bounded appropriately within the
// main frame when we apply both scaling and resizes.
TEST_P(VisualViewportTest, TestOffsetClampingWithResizeAndScale) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(320, 240));

  navigateTo("about:blank");
  forceFullCompositingUpdate();

  // Visual viewport should be initialized to same size as WebView so no
  // scrolling possible.
  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  EXPECT_FLOAT_POINT_EQ(FloatPoint(0, 0),
                        visualViewport.VisibleRect().Location());

  // Zoom in to 2X so we can scroll the viewport to 160x120.
  visualViewport.SetScale(2);
  visualViewport.SetLocation(FloatPoint(200, 200));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(160, 120),
                        visualViewport.VisibleRect().Location());

  // Now resize the viewport to make it 10px smaller. Since we're zoomed in by
  // 2X it should allow us to scroll by 5px more.
  visualViewport.SetSize(IntSize(310, 230));
  visualViewport.SetLocation(FloatPoint(200, 200));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(165, 125),
                        visualViewport.VisibleRect().Location());

  // The viewport can be larger than the main frame (currently 320, 240) though
  // typically the scale will be clamped to prevent it from actually being
  // larger.
  visualViewport.SetSize(IntSize(330, 250));
  EXPECT_SIZE_EQ(IntSize(330, 250), visualViewport.size());

  // Resize both the viewport and the frame to be larger.
  webViewImpl()->Resize(IntSize(640, 480));
  webViewImpl()->UpdateAllLifecyclePhases();
  EXPECT_SIZE_EQ(IntSize(webViewImpl()->size()), visualViewport.size());
  EXPECT_SIZE_EQ(IntSize(webViewImpl()->size()),
                 frame()->View()->FrameRect().Size());
  visualViewport.SetLocation(FloatPoint(1000, 1000));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(320, 240),
                        visualViewport.VisibleRect().Location());

  // Make sure resizing the viewport doesn't change its offset if the resize
  // doesn't make the viewport go out of bounds.
  visualViewport.SetLocation(FloatPoint(200, 200));
  visualViewport.SetSize(IntSize(880, 560));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(200, 200),
                        visualViewport.VisibleRect().Location());
}

// The main FrameView's size should be set such that its the size of the visual
// viewport at minimum scale. If there's no explicit minimum scale set, the
// FrameView should be set to the content width and height derived by the aspect
// ratio.
TEST_P(VisualViewportTest, TestFrameViewSizedToContent) {
  initializeWithAndroidSettings();
  webViewImpl()->Resize(IntSize(320, 240));

  registerMockedHttpURLLoad("200-by-300-viewport.html");
  navigateTo(m_baseURL + "200-by-300-viewport.html");

  webViewImpl()->Resize(IntSize(600, 800));
  webViewImpl()->UpdateAllLifecyclePhases();

  // Note: the size is ceiled and should match the behavior in CC's
  // LayerImpl::bounds().
  EXPECT_SIZE_EQ(
      IntSize(200, 267),
      webViewImpl()->MainFrameImpl()->GetFrameView()->FrameRect().Size());
}

// The main FrameView's size should be set such that its the size of the visual
// viewport at minimum scale. On Desktop, the minimum scale is set at 1 so make
// sure the FrameView is sized to the viewport.
TEST_P(VisualViewportTest, TestFrameViewSizedToMinimumScale) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(320, 240));

  registerMockedHttpURLLoad("200-by-300.html");
  navigateTo(m_baseURL + "200-by-300.html");

  webViewImpl()->Resize(IntSize(100, 160));
  webViewImpl()->UpdateAllLifecyclePhases();

  EXPECT_SIZE_EQ(
      IntSize(100, 160),
      webViewImpl()->MainFrameImpl()->GetFrameView()->FrameRect().Size());
}

// Test that attaching a new frame view resets the size of the inner viewport
// scroll layer. crbug.com/423189.
TEST_P(VisualViewportTest, TestAttachingNewFrameSetsInnerScrollLayerSize) {
  initializeWithAndroidSettings();
  webViewImpl()->Resize(IntSize(320, 240));

  // Load a wider page first, the navigation should resize the scroll layer to
  // the smaller size on the second navigation.
  registerMockedHttpURLLoad("content-width-1000.html");
  navigateTo(m_baseURL + "content-width-1000.html");
  webViewImpl()->UpdateAllLifecyclePhases();

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  visualViewport.SetScale(2);
  visualViewport.Move(ScrollOffset(50, 60));

  // Move and scale the viewport to make sure it gets reset in the navigation.
  EXPECT_SIZE_EQ(FloatSize(50, 60), visualViewport.GetScrollOffset());
  EXPECT_EQ(2, visualViewport.Scale());

  // Navigate again, this time the FrameView should be smaller.
  registerMockedHttpURLLoad("viewport-device-width.html");
  navigateTo(m_baseURL + "viewport-device-width.html");

  // Ensure the scroll layer matches the frame view's size.
  EXPECT_SIZE_EQ(FloatSize(320, 240), visualViewport.ScrollLayer()->Size());

  EXPECT_EQ(static_cast<int>(CompositorSubElementId::kViewport),
            visualViewport.ScrollLayer()->GetElementId().secondaryId);

  // Ensure the location and scale were reset.
  EXPECT_SIZE_EQ(FloatSize(), visualViewport.GetScrollOffset());
  EXPECT_EQ(1, visualViewport.Scale());
}

// The main FrameView's size should be set such that its the size of the visual
// viewport at minimum scale. Test that the FrameView is appropriately sized in
// the presence of a viewport <meta> tag.
TEST_P(VisualViewportTest, TestFrameViewSizedToViewportMetaMinimumScale) {
  initializeWithAndroidSettings();
  webViewImpl()->Resize(IntSize(320, 240));

  registerMockedHttpURLLoad("200-by-300-min-scale-2.html");
  navigateTo(m_baseURL + "200-by-300-min-scale-2.html");

  webViewImpl()->Resize(IntSize(100, 160));
  webViewImpl()->UpdateAllLifecyclePhases();

  EXPECT_SIZE_EQ(
      IntSize(50, 80),
      webViewImpl()->MainFrameImpl()->GetFrameView()->FrameRect().Size());
}

// Test that the visual viewport still gets sized in AutoSize/AutoResize mode.
TEST_P(VisualViewportTest, TestVisualViewportGetsSizeInAutoSizeMode) {
  initializeWithDesktopSettings();

  EXPECT_SIZE_EQ(IntSize(0, 0), IntSize(webViewImpl()->size()));
  EXPECT_SIZE_EQ(IntSize(0, 0), frame()->GetPage()->GetVisualViewport().size());

  webViewImpl()->EnableAutoResizeMode(WebSize(10, 10), WebSize(1000, 1000));

  registerMockedHttpURLLoad("200-by-300.html");
  navigateTo(m_baseURL + "200-by-300.html");

  EXPECT_SIZE_EQ(IntSize(200, 300),
                 frame()->GetPage()->GetVisualViewport().size());
}

// Test that the text selection handle's position accounts for the visual
// viewport.
TEST_P(VisualViewportTest, TestTextSelectionHandles) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(500, 800));

  registerMockedHttpURLLoad("pinch-viewport-input-field.html");
  navigateTo(m_baseURL + "pinch-viewport-input-field.html");

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  webViewImpl()->SetInitialFocus(false);

  WebRect originalAnchor;
  WebRect originalFocus;
  webViewImpl()->SelectionBounds(originalAnchor, originalFocus);

  webViewImpl()->SetPageScaleFactor(2);
  visualViewport.SetLocation(FloatPoint(100, 400));

  WebRect anchor;
  WebRect focus;
  webViewImpl()->SelectionBounds(anchor, focus);

  IntPoint expected(IntRect(originalAnchor).Location());
  expected.MoveBy(-FlooredIntPoint(visualViewport.VisibleRect().Location()));
  expected.Scale(visualViewport.Scale(), visualViewport.Scale());

  EXPECT_POINT_EQ(expected, IntRect(anchor).Location());
  EXPECT_POINT_EQ(expected, IntRect(focus).Location());

  // FIXME(bokan) - http://crbug.com/364154 - Figure out how to test text
  // selection as well rather than just carret.
}

// Test that the HistoryItem for the page stores the visual viewport's offset
// and scale.
TEST_P(VisualViewportTest, TestSavedToHistoryItem) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(200, 300));
  webViewImpl()->UpdateAllLifecyclePhases();

  registerMockedHttpURLLoad("200-by-300.html");
  navigateTo(m_baseURL + "200-by-300.html");

  EXPECT_SIZE_EQ(ScrollOffset(0, 0),
                 ToLocalFrame(webViewImpl()->GetPage()->MainFrame())
                     ->Loader()
                     .GetDocumentLoader()
                     ->GetHistoryItem()
                     ->VisualViewportScrollOffset());

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  visualViewport.SetScale(2);

  EXPECT_EQ(2, ToLocalFrame(webViewImpl()->GetPage()->MainFrame())
                   ->Loader()
                   .GetDocumentLoader()
                   ->GetHistoryItem()
                   ->PageScaleFactor());

  visualViewport.SetLocation(FloatPoint(10, 20));

  EXPECT_SIZE_EQ(ScrollOffset(10, 20),
                 ToLocalFrame(webViewImpl()->GetPage()->MainFrame())
                     ->Loader()
                     .GetDocumentLoader()
                     ->GetHistoryItem()
                     ->VisualViewportScrollOffset());
}

// Test restoring a HistoryItem properly restores the visual viewport's state.
TEST_P(VisualViewportTest, TestRestoredFromHistoryItem) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(200, 300));

  registerMockedHttpURLLoad("200-by-300.html");

  WebHistoryItem item;
  item.Initialize();
  WebURL destinationURL(URLTestHelpers::ToKURL(m_baseURL + "200-by-300.html"));
  item.SetURLString(destinationURL.GetString());
  item.SetVisualViewportScrollOffset(WebFloatPoint(100, 120));
  item.SetPageScaleFactor(2);

  FrameTestHelpers::LoadHistoryItem(webViewImpl()->MainFrame(), item,
                                    kWebHistoryDifferentDocumentLoad,
                                    WebCachePolicy::kUseProtocolCachePolicy);

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  EXPECT_EQ(2, visualViewport.Scale());

  EXPECT_FLOAT_POINT_EQ(FloatPoint(100, 120),
                        visualViewport.VisibleRect().Location());
}

// Test restoring a HistoryItem without the visual viewport offset falls back to
// distributing the scroll offset between the main frame and the visual
// viewport.
TEST_P(VisualViewportTest, TestRestoredFromLegacyHistoryItem) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(100, 150));

  registerMockedHttpURLLoad("200-by-300-viewport.html");

  WebHistoryItem item;
  item.Initialize();
  WebURL destinationURL(
      URLTestHelpers::ToKURL(m_baseURL + "200-by-300-viewport.html"));
  item.SetURLString(destinationURL.GetString());
  // (-1, -1) will be used if the HistoryItem is an older version prior to
  // having visual viewport scroll offset.
  item.SetVisualViewportScrollOffset(WebFloatPoint(-1, -1));
  item.SetScrollOffset(WebPoint(120, 180));
  item.SetPageScaleFactor(2);

  FrameTestHelpers::LoadHistoryItem(webViewImpl()->MainFrame(), item,
                                    kWebHistoryDifferentDocumentLoad,
                                    WebCachePolicy::kUseProtocolCachePolicy);

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  EXPECT_EQ(2, visualViewport.Scale());
  EXPECT_SIZE_EQ(
      ScrollOffset(100, 150),
      frame()->View()->LayoutViewportScrollableArea()->GetScrollOffset());
  EXPECT_FLOAT_POINT_EQ(FloatPoint(20, 30),
                        visualViewport.VisibleRect().Location());
}

// Test that navigation to a new page with a different sized main frame doesn't
// clobber the history item's main frame scroll offset. crbug.com/371867
TEST_P(VisualViewportTest,
       TestNavigateToSmallerFrameViewHistoryItemClobberBug) {
  initializeWithAndroidSettings();
  webViewImpl()->Resize(IntSize(400, 400));
  webViewImpl()->UpdateAllLifecyclePhases();

  registerMockedHttpURLLoad("content-width-1000.html");
  navigateTo(m_baseURL + "content-width-1000.html");

  FrameView* frameView = webViewImpl()->MainFrameImpl()->GetFrameView();
  frameView->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 1000), kProgrammaticScroll);

  EXPECT_SIZE_EQ(IntSize(1000, 1000), frameView->FrameRect().Size());

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  visualViewport.SetScale(2);
  visualViewport.SetLocation(FloatPoint(350, 350));

  Persistent<HistoryItem> firstItem = webViewImpl()
                                          ->MainFrameImpl()
                                          ->GetFrame()
                                          ->Loader()
                                          .GetDocumentLoader()
                                          ->GetHistoryItem();
  EXPECT_SIZE_EQ(ScrollOffset(0, 1000), firstItem->GetScrollOffset());

  // Now navigate to a page which causes a smaller frameView. Make sure that
  // navigating doesn't cause the history item to set a new scroll offset
  // before the item was replaced.
  navigateTo("about:blank");
  frameView = webViewImpl()->MainFrameImpl()->GetFrameView();

  EXPECT_NE(firstItem, webViewImpl()
                           ->MainFrameImpl()
                           ->GetFrame()
                           ->Loader()
                           .GetDocumentLoader()
                           ->GetHistoryItem());
  EXPECT_LT(frameView->FrameRect().Size().Width(), 1000);
  EXPECT_SIZE_EQ(ScrollOffset(0, 1000), firstItem->GetScrollOffset());
}

// Test that the coordinates sent into moveRangeSelection are offset by the
// visual viewport's location.
TEST_P(VisualViewportTest,
       DISABLED_TestWebFrameRangeAccountsForVisualViewportScroll) {
  initializeWithDesktopSettings();
  webViewImpl()->GetSettings()->SetDefaultFontSize(12);
  webViewImpl()->Resize(WebSize(640, 480));
  registerMockedHttpURLLoad("move_range.html");
  navigateTo(m_baseURL + "move_range.html");

  WebRect baseRect;
  WebRect extentRect;

  webViewImpl()->SetPageScaleFactor(2);
  WebFrame* mainFrame = webViewImpl()->MainFrame();

  // Select some text and get the base and extent rects (that's the start of
  // the range and its end). Do a sanity check that the expected text is
  // selected
  mainFrame->ExecuteScript(WebScriptSource("selectRange();"));
  EXPECT_EQ("ir", mainFrame->ToWebLocalFrame()->SelectionAsText().Utf8());

  webViewImpl()->SelectionBounds(baseRect, extentRect);
  WebPoint initialPoint(baseRect.x, baseRect.y);
  WebPoint endPoint(extentRect.x, extentRect.y);

  // Move the visual viewport over and make the selection in the same
  // screen-space location. The selection should change to two characters to the
  // right and down one line.
  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  visualViewport.Move(ScrollOffset(60, 25));
  mainFrame->ToWebLocalFrame()->MoveRangeSelection(initialPoint, endPoint);
  EXPECT_EQ("t ", mainFrame->ToWebLocalFrame()->SelectionAsText().Utf8());
}

// Test that the scrollFocusedEditableElementIntoRect method works with the
// visual viewport.
TEST_P(VisualViewportTest, DISABLED_TestScrollFocusedEditableElementIntoRect) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(500, 300));

  registerMockedHttpURLLoad("pinch-viewport-input-field.html");
  navigateTo(m_baseURL + "pinch-viewport-input-field.html");

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  webViewImpl()->ResizeVisualViewport(IntSize(200, 100));
  webViewImpl()->SetInitialFocus(false);
  visualViewport.SetLocation(FloatPoint());
  webViewImpl()->ScrollFocusedEditableElementIntoRect(IntRect(0, 0, 500, 200));

  EXPECT_SIZE_EQ(
      ScrollOffset(0, frame()->View()->MaximumScrollOffset().Height()),
      frame()->View()->GetScrollOffset());
  EXPECT_FLOAT_POINT_EQ(FloatPoint(150, 200),
                        visualViewport.VisibleRect().Location());

  // Try it again but with the page zoomed in
  frame()->View()->SetScrollOffset(ScrollOffset(0, 0), kProgrammaticScroll);
  webViewImpl()->ResizeVisualViewport(IntSize(500, 300));
  visualViewport.SetLocation(FloatPoint(0, 0));

  webViewImpl()->SetPageScaleFactor(2);
  webViewImpl()->ScrollFocusedEditableElementIntoRect(IntRect(0, 0, 500, 200));
  EXPECT_SIZE_EQ(
      ScrollOffset(0, frame()->View()->MaximumScrollOffset().Height()),
      frame()->View()->GetScrollOffset());
  EXPECT_FLOAT_POINT_EQ(FloatPoint(125, 150),
                        visualViewport.VisibleRect().Location());

  // Once more but make sure that we don't move the visual viewport unless
  // necessary.
  registerMockedHttpURLLoad("pinch-viewport-input-field-long-and-wide.html");
  navigateTo(m_baseURL + "pinch-viewport-input-field-long-and-wide.html");
  webViewImpl()->SetInitialFocus(false);
  visualViewport.SetLocation(FloatPoint());
  frame()->View()->SetScrollOffset(ScrollOffset(0, 0), kProgrammaticScroll);
  webViewImpl()->ResizeVisualViewport(IntSize(500, 300));
  visualViewport.SetLocation(FloatPoint(30, 50));

  webViewImpl()->SetPageScaleFactor(2);
  webViewImpl()->ScrollFocusedEditableElementIntoRect(IntRect(0, 0, 500, 200));
  EXPECT_SIZE_EQ(ScrollOffset(200 - 30 - 75, 600 - 50 - 65),
                 frame()->View()->GetScrollOffset());
  EXPECT_FLOAT_POINT_EQ(FloatPoint(30, 50),
                        visualViewport.VisibleRect().Location());
}

// Test that resizing the WebView causes ViewportConstrained objects to
// relayout.
TEST_P(VisualViewportTest, TestWebViewResizeCausesViewportConstrainedLayout) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(500, 300));

  registerMockedHttpURLLoad("pinch-viewport-fixed-pos.html");
  navigateTo(m_baseURL + "pinch-viewport-fixed-pos.html");

  LayoutObject* navbar =
      frame()->GetDocument()->GetElementById("navbar")->GetLayoutObject();

  EXPECT_FALSE(navbar->NeedsLayout());

  frame()->View()->Resize(IntSize(500, 200));

  EXPECT_TRUE(navbar->NeedsLayout());
}

class MockWebFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  MOCK_METHOD1(ShowContextMenu, void(const WebContextMenuData&));
  MOCK_METHOD1(DidChangeScrollOffset, void(WebLocalFrame*));
};

MATCHER_P2(ContextMenuAtLocation,
           x,
           y,
           std::string(negation ? "is" : "isn't") + " at expected location [" +
               PrintToString(x) +
               ", " +
               PrintToString(y) +
               "]") {
  return arg.mouse_position.x == x && arg.mouse_position.y == y;
}

// Test that the context menu's location is correct in the presence of visual
// viewport offset.
TEST_P(VisualViewportTest, TestContextMenuShownInCorrectLocation) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(200, 300));

  registerMockedHttpURLLoad("200-by-300.html");
  navigateTo(m_baseURL + "200-by-300.html");

  WebMouseEvent mouseDownEvent(WebInputEvent::kMouseDown,
                               WebInputEvent::kNoModifiers,
                               WebInputEvent::kTimeStampForTesting);
  mouseDownEvent.SetPositionInWidget(10, 10);
  mouseDownEvent.SetPositionInScreen(110, 210);
  mouseDownEvent.click_count = 1;
  mouseDownEvent.button = WebMouseEvent::Button::kRight;

  // Corresponding release event (Windows shows context menu on release).
  WebMouseEvent mouseUpEvent(mouseDownEvent);
  mouseUpEvent.SetType(WebInputEvent::kMouseUp);

  WebFrameClient* oldClient = webViewImpl()->MainFrameImpl()->Client();
  MockWebFrameClient mockWebFrameClient;
  EXPECT_CALL(mockWebFrameClient, ShowContextMenu(ContextMenuAtLocation(
                                      mouseDownEvent.PositionInWidget().x,
                                      mouseDownEvent.PositionInWidget().y)));

  // Do a sanity check with no scale applied.
  webViewImpl()->MainFrameImpl()->SetClient(&mockWebFrameClient);
  webViewImpl()->HandleInputEvent(WebCoalescedInputEvent(mouseDownEvent));
  webViewImpl()->HandleInputEvent(WebCoalescedInputEvent(mouseUpEvent));

  Mock::VerifyAndClearExpectations(&mockWebFrameClient);
  mouseDownEvent.button = WebMouseEvent::Button::kLeft;
  webViewImpl()->HandleInputEvent(WebCoalescedInputEvent(mouseDownEvent));

  // Now pinch zoom into the page and move the visual viewport. The context menu
  // should still appear at the location of the event, relative to the WebView.
  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  webViewImpl()->SetPageScaleFactor(2);
  EXPECT_CALL(mockWebFrameClient, DidChangeScrollOffset(_));
  visualViewport.SetLocation(FloatPoint(60, 80));
  EXPECT_CALL(mockWebFrameClient, ShowContextMenu(ContextMenuAtLocation(
                                      mouseDownEvent.PositionInWidget().x,
                                      mouseDownEvent.PositionInWidget().y)));

  mouseDownEvent.button = WebMouseEvent::Button::kRight;
  webViewImpl()->HandleInputEvent(WebCoalescedInputEvent(mouseDownEvent));
  webViewImpl()->HandleInputEvent(WebCoalescedInputEvent(mouseUpEvent));

  // Reset the old client so destruction can occur naturally.
  webViewImpl()->MainFrameImpl()->SetClient(oldClient);
}

// Test that the client is notified if page scroll events.
TEST_P(VisualViewportTest, TestClientNotifiedOfScrollEvents) {
  initializeWithAndroidSettings();
  webViewImpl()->Resize(IntSize(200, 300));

  registerMockedHttpURLLoad("200-by-300.html");
  navigateTo(m_baseURL + "200-by-300.html");

  WebFrameClient* oldClient = webViewImpl()->MainFrameImpl()->Client();
  MockWebFrameClient mockWebFrameClient;
  webViewImpl()->MainFrameImpl()->SetClient(&mockWebFrameClient);

  webViewImpl()->SetPageScaleFactor(2);
  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();

  EXPECT_CALL(mockWebFrameClient, DidChangeScrollOffset(_));
  visualViewport.SetLocation(FloatPoint(60, 80));
  Mock::VerifyAndClearExpectations(&mockWebFrameClient);

  // Scroll vertically.
  EXPECT_CALL(mockWebFrameClient, DidChangeScrollOffset(_));
  visualViewport.SetLocation(FloatPoint(60, 90));
  Mock::VerifyAndClearExpectations(&mockWebFrameClient);

  // Scroll horizontally.
  EXPECT_CALL(mockWebFrameClient, DidChangeScrollOffset(_));
  visualViewport.SetLocation(FloatPoint(70, 90));

  // Reset the old client so destruction can occur naturally.
  webViewImpl()->MainFrameImpl()->SetClient(oldClient);
}

// Tests that calling scroll into view on a visible element doesn't cause
// a scroll due to a fractional offset. Bug crbug.com/463356.
TEST_P(VisualViewportTest, ScrollIntoViewFractionalOffset) {
  initializeWithAndroidSettings();

  webViewImpl()->Resize(IntSize(1000, 1000));

  registerMockedHttpURLLoad("scroll-into-view.html");
  navigateTo(m_baseURL + "scroll-into-view.html");

  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();
  ScrollableArea* layoutViewportScrollableArea =
      frameView.LayoutViewportScrollableArea();
  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  Element* inputBox = frame()->GetDocument()->GetElementById("box");

  webViewImpl()->SetPageScaleFactor(2);

  // The element is already in the view so the scrollIntoView shouldn't move
  // the viewport at all.
  webViewImpl()->SetVisualViewportOffset(WebFloatPoint(250.25f, 100.25f));
  layoutViewportScrollableArea->SetScrollOffset(ScrollOffset(0, 900.75),
                                                kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  EXPECT_SIZE_EQ(ScrollOffset(0, 900),
                 layoutViewportScrollableArea->GetScrollOffset());
  EXPECT_SIZE_EQ(FloatSize(250.25f, 100.25f), visualViewport.GetScrollOffset());

  // Change the fractional part of the frameview to one that would round down.
  layoutViewportScrollableArea->SetScrollOffset(ScrollOffset(0, 900.125),
                                                kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  EXPECT_SIZE_EQ(ScrollOffset(0, 900),
                 layoutViewportScrollableArea->GetScrollOffset());
  EXPECT_SIZE_EQ(FloatSize(250.25f, 100.25f), visualViewport.GetScrollOffset());

  // Repeat both tests above with the visual viewport at a high fractional.
  webViewImpl()->SetVisualViewportOffset(WebFloatPoint(250.875f, 100.875f));
  layoutViewportScrollableArea->SetScrollOffset(ScrollOffset(0, 900.75),
                                                kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  EXPECT_SIZE_EQ(ScrollOffset(0, 900),
                 layoutViewportScrollableArea->GetScrollOffset());
  EXPECT_SIZE_EQ(FloatSize(250.875f, 100.875f),
                 visualViewport.GetScrollOffset());

  // Change the fractional part of the frameview to one that would round down.
  layoutViewportScrollableArea->SetScrollOffset(ScrollOffset(0, 900.125),
                                                kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  EXPECT_SIZE_EQ(ScrollOffset(0, 900),
                 layoutViewportScrollableArea->GetScrollOffset());
  EXPECT_SIZE_EQ(FloatSize(250.875f, 100.875f),
                 visualViewport.GetScrollOffset());

  // Both viewports with a 0.5 fraction.
  webViewImpl()->SetVisualViewportOffset(WebFloatPoint(250.5f, 100.5f));
  layoutViewportScrollableArea->SetScrollOffset(ScrollOffset(0, 900.5),
                                                kProgrammaticScroll);
  inputBox->scrollIntoViewIfNeeded(false);

  EXPECT_SIZE_EQ(ScrollOffset(0, 900),
                 layoutViewportScrollableArea->GetScrollOffset());
  EXPECT_SIZE_EQ(FloatSize(250.5f, 100.5f), visualViewport.GetScrollOffset());
}

static ScrollOffset expectedMaxFrameViewScrollOffset(
    VisualViewport& visualViewport,
    FrameView& frameView) {
  float aspectRatio = visualViewport.VisibleRect().Width() /
                      visualViewport.VisibleRect().Height();
  float newHeight = frameView.FrameRect().Width() / aspectRatio;
  return ScrollOffset(
      frameView.ContentsSize().Width() - frameView.FrameRect().Width(),
      frameView.ContentsSize().Height() - newHeight);
}

TEST_P(VisualViewportTest, TestBrowserControlsAdjustment) {
  initializeWithAndroidSettings();
  webViewImpl()->ResizeWithBrowserControls(IntSize(500, 450), 20, false);

  registerMockedHttpURLLoad("content-width-1000.html");
  navigateTo(m_baseURL + "content-width-1000.html");

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();

  visualViewport.SetScale(1);
  EXPECT_SIZE_EQ(IntSize(500, 450), visualViewport.VisibleRect().Size());
  EXPECT_SIZE_EQ(IntSize(1000, 900), frameView.FrameRect().Size());

  // Simulate bringing down the browser controls by 20px.
  webViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 1, 1);
  EXPECT_SIZE_EQ(IntSize(500, 430), visualViewport.VisibleRect().Size());

  // Test that the scroll bounds are adjusted appropriately: the visual viewport
  // should be shrunk by 20px to 430px. The outer viewport was shrunk to
  // maintain the
  // aspect ratio so it's height is 860px.
  visualViewport.Move(ScrollOffset(10000, 10000));
  EXPECT_SIZE_EQ(FloatSize(500, 860 - 430), visualViewport.GetScrollOffset());

  // The outer viewport (FrameView) should be affected as well.
  frameView.LayoutViewportScrollableArea()->ScrollBy(ScrollOffset(10000, 10000),
                                                     kUserScroll);
  EXPECT_SIZE_EQ(expectedMaxFrameViewScrollOffset(visualViewport, frameView),
                 frameView.LayoutViewportScrollableArea()->GetScrollOffset());

  // Simulate bringing up the browser controls by 10.5px.
  webViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 1, -10.5f / 20);
  EXPECT_FLOAT_SIZE_EQ(FloatSize(500, 440.5f),
                       visualViewport.VisibleRect().Size());

  // maximumScrollPosition |ceil|s the browser controls adjustment.
  visualViewport.Move(ScrollOffset(10000, 10000));
  EXPECT_FLOAT_SIZE_EQ(FloatSize(500, 881 - 441),
                       visualViewport.GetScrollOffset());

  // The outer viewport (FrameView) should be affected as well.
  frameView.LayoutViewportScrollableArea()->ScrollBy(ScrollOffset(10000, 10000),
                                                     kUserScroll);
  EXPECT_SIZE_EQ(expectedMaxFrameViewScrollOffset(visualViewport, frameView),
                 frameView.LayoutViewportScrollableArea()->GetScrollOffset());
}

TEST_P(VisualViewportTest, TestBrowserControlsAdjustmentWithScale) {
  initializeWithAndroidSettings();
  webViewImpl()->ResizeWithBrowserControls(IntSize(500, 450), 20, false);

  registerMockedHttpURLLoad("content-width-1000.html");
  navigateTo(m_baseURL + "content-width-1000.html");

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();

  visualViewport.SetScale(2);
  EXPECT_SIZE_EQ(IntSize(250, 225), visualViewport.VisibleRect().Size());
  EXPECT_SIZE_EQ(IntSize(1000, 900), frameView.FrameRect().Size());

  // Simulate bringing down the browser controls by 20px. Since we're zoomed in,
  // the browser controls take up half as much space (in document-space) than
  // they do at an unzoomed level.
  webViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 1, 1);
  EXPECT_SIZE_EQ(IntSize(250, 215), visualViewport.VisibleRect().Size());

  // Test that the scroll bounds are adjusted appropriately.
  visualViewport.Move(ScrollOffset(10000, 10000));
  EXPECT_SIZE_EQ(FloatSize(750, 860 - 215), visualViewport.GetScrollOffset());

  // The outer viewport (FrameView) should be affected as well.
  frameView.LayoutViewportScrollableArea()->ScrollBy(ScrollOffset(10000, 10000),
                                                     kUserScroll);
  ScrollOffset expected =
      expectedMaxFrameViewScrollOffset(visualViewport, frameView);
  EXPECT_SIZE_EQ(expected,
                 frameView.LayoutViewportScrollableArea()->GetScrollOffset());

  // Scale back out, FrameView max scroll shouldn't have changed. Visual
  // viewport should be moved up to accomodate larger view.
  webViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 0.5f, 0);
  EXPECT_EQ(1, visualViewport.Scale());
  EXPECT_SIZE_EQ(expected,
                 frameView.LayoutViewportScrollableArea()->GetScrollOffset());
  frameView.LayoutViewportScrollableArea()->ScrollBy(ScrollOffset(10000, 10000),
                                                     kUserScroll);
  EXPECT_SIZE_EQ(expected,
                 frameView.LayoutViewportScrollableArea()->GetScrollOffset());

  EXPECT_SIZE_EQ(FloatSize(500, 860 - 430), visualViewport.GetScrollOffset());
  visualViewport.Move(ScrollOffset(10000, 10000));
  EXPECT_SIZE_EQ(FloatSize(500, 860 - 430), visualViewport.GetScrollOffset());

  // Scale out, use a scale that causes fractional rects.
  webViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 0.8f, -1);
  EXPECT_SIZE_EQ(FloatSize(625, 562.5), visualViewport.VisibleRect().Size());

  // Bring out the browser controls by 11
  webViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 1, 11 / 20.f);
  EXPECT_SIZE_EQ(FloatSize(625, 548.75), visualViewport.VisibleRect().Size());

  // Ensure max scroll offsets are updated properly.
  visualViewport.Move(ScrollOffset(10000, 10000));
  EXPECT_FLOAT_SIZE_EQ(FloatSize(375, 877.5 - 548.75),
                       visualViewport.GetScrollOffset());

  frameView.LayoutViewportScrollableArea()->ScrollBy(ScrollOffset(10000, 10000),
                                                     kUserScroll);
  EXPECT_SIZE_EQ(expectedMaxFrameViewScrollOffset(visualViewport, frameView),
                 frameView.LayoutViewportScrollableArea()->GetScrollOffset());
}

// Tests that a scroll all the way to the bottom of the page, while hiding the
// browser controls doesn't cause a clamp in the viewport scroll offset when the
// top controls initiated resize occurs.
TEST_P(VisualViewportTest, TestBrowserControlsAdjustmentAndResize) {
  int browserControlsHeight = 20;
  int visualViewportHeight = 450;
  int layoutViewportHeight = 900;
  float pageScale = 2;
  float minPageScale = 0.5;

  initializeWithAndroidSettings();

  // Initialize with browser controls showing and shrinking the Blink size.
  webViewImpl()->ResizeWithBrowserControls(
      WebSize(500, visualViewportHeight - browserControlsHeight), 20, true);
  webViewImpl()->GetBrowserControls().SetShownRatio(1);

  registerMockedHttpURLLoad("content-width-1000.html");
  navigateTo(m_baseURL + "content-width-1000.html");

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();

  visualViewport.SetScale(pageScale);
  EXPECT_SIZE_EQ(
      IntSize(250, (visualViewportHeight - browserControlsHeight) / pageScale),
      visualViewport.VisibleRect().Size());
  EXPECT_SIZE_EQ(IntSize(1000, layoutViewportHeight -
                                   browserControlsHeight / minPageScale),
                 frameView.FrameRect().Size());
  EXPECT_SIZE_EQ(IntSize(500, visualViewportHeight - browserControlsHeight),
                 visualViewport.size());

  // Scroll all the way to the bottom, hiding the browser controls in the
  // process.
  visualViewport.Move(ScrollOffset(10000, 10000));
  frameView.LayoutViewportScrollableArea()->ScrollBy(ScrollOffset(10000, 10000),
                                                     kUserScroll);
  webViewImpl()->GetBrowserControls().SetShownRatio(0);

  EXPECT_SIZE_EQ(IntSize(250, visualViewportHeight / pageScale),
                 visualViewport.VisibleRect().Size());

  ScrollOffset frameViewExpected =
      expectedMaxFrameViewScrollOffset(visualViewport, frameView);
  ScrollOffset visualViewportExpected = ScrollOffset(
      750, layoutViewportHeight - visualViewportHeight / pageScale);

  EXPECT_SIZE_EQ(visualViewportExpected, visualViewport.GetScrollOffset());
  EXPECT_SIZE_EQ(frameViewExpected,
                 frameView.LayoutViewportScrollableArea()->GetScrollOffset());

  ScrollOffset totalExpected = visualViewportExpected + frameViewExpected;

  // Resize the widget to match the browser controls adjustment. Ensure that the
  // total offset (i.e. what the user sees) doesn't change because of clamping
  // the offsets to valid values.
  webViewImpl()->ResizeWithBrowserControls(WebSize(500, visualViewportHeight),
                                           20, false);

  EXPECT_SIZE_EQ(IntSize(500, visualViewportHeight), visualViewport.size());
  EXPECT_SIZE_EQ(IntSize(250, visualViewportHeight / pageScale),
                 visualViewport.VisibleRect().Size());
  EXPECT_SIZE_EQ(IntSize(1000, layoutViewportHeight),
                 frameView.FrameRect().Size());
  EXPECT_SIZE_EQ(
      totalExpected,
      visualViewport.GetScrollOffset() +
          frameView.LayoutViewportScrollableArea()->GetScrollOffset());
}

// Tests that a scroll all the way to the bottom while showing the browser
// controls doesn't cause a clamp to the viewport scroll offset when the browser
// controls initiated resize occurs.
TEST_P(VisualViewportTest, TestBrowserControlsShrinkAdjustmentAndResize) {
  int browserControlsHeight = 20;
  int visualViewportHeight = 500;
  int layoutViewportHeight = 1000;
  int contentHeight = 2000;
  float pageScale = 2;
  float minPageScale = 0.5;

  initializeWithAndroidSettings();

  // Initialize with browser controls hidden and not shrinking the Blink size.
  webViewImpl()->ResizeWithBrowserControls(IntSize(500, visualViewportHeight),
                                           20, false);
  webViewImpl()->GetBrowserControls().SetShownRatio(0);

  registerMockedHttpURLLoad("content-width-1000.html");
  navigateTo(m_baseURL + "content-width-1000.html");

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();

  visualViewport.SetScale(pageScale);
  EXPECT_SIZE_EQ(IntSize(250, visualViewportHeight / pageScale),
                 visualViewport.VisibleRect().Size());
  EXPECT_SIZE_EQ(IntSize(1000, layoutViewportHeight),
                 frameView.FrameRect().Size());
  EXPECT_SIZE_EQ(IntSize(500, visualViewportHeight), visualViewport.size());

  // Scroll all the way to the bottom, showing the the browser controls in the
  // process. (This could happen via window.scrollTo during a scroll, for
  // example).
  webViewImpl()->GetBrowserControls().SetShownRatio(1);
  visualViewport.Move(ScrollOffset(10000, 10000));
  frameView.LayoutViewportScrollableArea()->ScrollBy(ScrollOffset(10000, 10000),
                                                     kUserScroll);

  EXPECT_SIZE_EQ(
      IntSize(250, (visualViewportHeight - browserControlsHeight) / pageScale),
      visualViewport.VisibleRect().Size());

  ScrollOffset frameViewExpected(
      0, contentHeight -
             (layoutViewportHeight - browserControlsHeight / minPageScale));
  ScrollOffset visualViewportExpected = ScrollOffset(
      750, (layoutViewportHeight - browserControlsHeight / minPageScale -
            visualViewport.VisibleRect().Height()));

  EXPECT_SIZE_EQ(visualViewportExpected, visualViewport.GetScrollOffset());
  EXPECT_SIZE_EQ(frameViewExpected,
                 frameView.LayoutViewportScrollableArea()->GetScrollOffset());

  ScrollOffset totalExpected = visualViewportExpected + frameViewExpected;

  // Resize the widget to match the browser controls adjustment. Ensure that the
  // total offset (i.e. what the user sees) doesn't change because of clamping
  // the offsets to valid values.
  webViewImpl()->ResizeWithBrowserControls(
      WebSize(500, visualViewportHeight - browserControlsHeight), 20, true);

  EXPECT_SIZE_EQ(IntSize(500, visualViewportHeight - browserControlsHeight),
                 visualViewport.size());
  EXPECT_SIZE_EQ(
      IntSize(250, (visualViewportHeight - browserControlsHeight) / pageScale),
      visualViewport.VisibleRect().Size());
  EXPECT_SIZE_EQ(IntSize(1000, layoutViewportHeight -
                                   browserControlsHeight / minPageScale),
                 frameView.FrameRect().Size());
  EXPECT_SIZE_EQ(
      totalExpected,
      visualViewport.GetScrollOffset() +
          frameView.LayoutViewportScrollableArea()->GetScrollOffset());
}

// Tests that a resize due to browser controls hiding doesn't incorrectly clamp
// the main frame's scroll offset. crbug.com/428193.
TEST_P(VisualViewportTest, TestTopControlHidingResizeDoesntClampMainFrame) {
  initializeWithAndroidSettings();
  webViewImpl()->ResizeWithBrowserControls(webViewImpl()->size(), 500, false);
  webViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 1, 1);
  webViewImpl()->ResizeWithBrowserControls(WebSize(1000, 1000), 500, true);

  registerMockedHttpURLLoad("content-width-1000.html");
  navigateTo(m_baseURL + "content-width-1000.html");

  // Scroll the FrameView to the bottom of the page but "hide" the browser
  // controls on the compositor side so the max scroll position should account
  // for the full viewport height.
  webViewImpl()->ApplyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                     WebFloatSize(), 1, -1);
  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();
  frameView.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 10000), kProgrammaticScroll);
  EXPECT_EQ(
      500,
      frameView.LayoutViewportScrollableArea()->GetScrollOffset().Height());

  // Now send the resize, make sure the scroll offset doesn't change.
  webViewImpl()->ResizeWithBrowserControls(WebSize(1000, 1500), 500, false);
  EXPECT_EQ(
      500,
      frameView.LayoutViewportScrollableArea()->GetScrollOffset().Height());
}

static void configureHiddenScrollbarsSettings(WebSettings* settings) {
  VisualViewportTest::configureAndroidSettings(settings);
  settings->SetHideScrollbars(true);
}

// Tests that scrollbar layers are not attached to the inner viewport container
// layer when hideScrollbars WebSetting is true.
TEST_P(VisualViewportTest,
       TestScrollbarsNotAttachedWhenHideScrollbarsSettingIsTrue) {
  initializeWithAndroidSettings(configureHiddenScrollbarsSettings);
  webViewImpl()->Resize(IntSize(100, 150));
  navigateTo("about:blank");

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  EXPECT_FALSE(visualViewport.LayerForHorizontalScrollbar()->Parent());
  EXPECT_FALSE(visualViewport.LayerForVerticalScrollbar()->Parent());
}

// Tests that scrollbar layers are attached to the inner viewport container
// layer when hideScrollbars WebSetting is false.
TEST_P(VisualViewportTest,
       TestScrollbarsAttachedWhenHideScrollbarsSettingIsFalse) {
  initializeWithAndroidSettings();
  webViewImpl()->Resize(IntSize(100, 150));
  navigateTo("about:blank");

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  EXPECT_TRUE(visualViewport.LayerForHorizontalScrollbar()->Parent());
  EXPECT_TRUE(visualViewport.LayerForVerticalScrollbar()->Parent());
}

// Tests that the layout viewport's scroll layer bounds are updated in a
// compositing change update. crbug.com/423188.
TEST_P(VisualViewportTest, TestChangingContentSizeAffectsScrollBounds) {
  initializeWithAndroidSettings();
  webViewImpl()->Resize(IntSize(100, 150));

  registerMockedHttpURLLoad("content-width-1000.html");
  navigateTo(m_baseURL + "content-width-1000.html");

  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();

  webViewImpl()->MainFrame()->ExecuteScript(
      WebScriptSource("var content = document.getElementById(\"content\");"
                      "content.style.width = \"1500px\";"
                      "content.style.height = \"2400px\";"));
  frameView.UpdateAllLifecyclePhases();
  WebLayer* scrollLayer = frameView.LayoutViewportScrollableArea()
                              ->LayerForScrolling()
                              ->PlatformLayer();

  EXPECT_SIZE_EQ(IntSize(1500, 2400), IntSize(scrollLayer->Bounds()));
}

// Tests that resizing the visual viepwort keeps its bounds within the outer
// viewport.
TEST_P(VisualViewportTest, ResizeVisualViewportStaysWithinOuterViewport) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(100, 200));

  navigateTo("about:blank");
  webViewImpl()->UpdateAllLifecyclePhases();

  webViewImpl()->ResizeVisualViewport(IntSize(100, 100));

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  visualViewport.Move(ScrollOffset(0, 100));

  EXPECT_EQ(100, visualViewport.GetScrollOffset().Height());

  webViewImpl()->ResizeVisualViewport(IntSize(100, 200));

  EXPECT_EQ(0, visualViewport.GetScrollOffset().Height());
}

TEST_P(VisualViewportTest, ElementBoundsInViewportSpaceAccountsForViewport) {
  initializeWithAndroidSettings();

  webViewImpl()->Resize(IntSize(500, 800));

  registerMockedHttpURLLoad("pinch-viewport-input-field.html");
  navigateTo(m_baseURL + "pinch-viewport-input-field.html");

  webViewImpl()->SetInitialFocus(false);
  Element* inputElement = webViewImpl()->FocusedElement();

  IntRect bounds = inputElement->GetLayoutObject()->AbsoluteBoundingBoxRect();

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  IntPoint scrollDelta(250, 400);
  visualViewport.SetScale(2);
  visualViewport.SetLocation(scrollDelta);

  const IntRect boundsInViewport = inputElement->BoundsInViewport();
  IntRect expectedBounds = bounds;
  expectedBounds.Scale(2.f);
  IntPoint expectedScrollDelta = scrollDelta;
  expectedScrollDelta.Scale(2.f, 2.f);

  EXPECT_POINT_EQ(IntPoint(expectedBounds.Location() - expectedScrollDelta),
                  boundsInViewport.Location());
  EXPECT_SIZE_EQ(expectedBounds.Size(), boundsInViewport.Size());
}

TEST_P(VisualViewportTest, ElementVisibleBoundsInVisualViewport) {
  initializeWithAndroidSettings();
  webViewImpl()->Resize(IntSize(640, 1080));
  registerMockedHttpURLLoad("viewport-select.html");
  navigateTo(m_baseURL + "viewport-select.html");

  ASSERT_EQ(2.0f, webViewImpl()->PageScaleFactor());
  webViewImpl()->SetInitialFocus(false);
  Element* element = webViewImpl()->FocusedElement();
  EXPECT_FALSE(element->VisibleBoundsInVisualViewport().IsEmpty());

  webViewImpl()->SetPageScaleFactor(4.0);
  EXPECT_TRUE(element->VisibleBoundsInVisualViewport().IsEmpty());
}

// Test that the various window.scroll and document.body.scroll properties and
// methods work unchanged from the pre-virtual viewport mode.
TEST_P(VisualViewportTest, bodyAndWindowScrollPropertiesAccountForViewport) {
  initializeWithAndroidSettings();

  webViewImpl()->Resize(IntSize(200, 300));

  // Load page with no main frame scrolling.
  registerMockedHttpURLLoad("200-by-300-viewport.html");
  navigateTo(m_baseURL + "200-by-300-viewport.html");

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  visualViewport.SetScale(2);

  // Chrome's quirky behavior regarding viewport scrolling means we treat the
  // body element as the viewport and don't apply scrolling to the HTML element.
  RuntimeEnabledFeatures::setScrollTopLeftInteropEnabled(false);

  LocalDOMWindow* window =
      webViewImpl()->MainFrameImpl()->GetFrame()->DomWindow();
  window->scrollTo(100, 150);
  EXPECT_EQ(100, window->scrollX());
  EXPECT_EQ(150, window->scrollY());
  EXPECT_FLOAT_SIZE_EQ(FloatSize(100, 150), visualViewport.GetScrollOffset());

  HTMLElement* body = toHTMLBodyElement(window->document()->body());
  body->setScrollLeft(50);
  body->setScrollTop(130);
  EXPECT_EQ(50, body->scrollLeft());
  EXPECT_EQ(130, body->scrollTop());
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 130), visualViewport.GetScrollOffset());

  HTMLElement* documentElement =
      ToHTMLElement(window->document()->documentElement());
  documentElement->setScrollLeft(40);
  documentElement->setScrollTop(50);
  EXPECT_EQ(0, documentElement->scrollLeft());
  EXPECT_EQ(0, documentElement->scrollTop());
  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 130), visualViewport.GetScrollOffset());

  visualViewport.SetLocation(FloatPoint(10, 20));
  EXPECT_EQ(10, body->scrollLeft());
  EXPECT_EQ(20, body->scrollTop());
  EXPECT_EQ(0, documentElement->scrollLeft());
  EXPECT_EQ(0, documentElement->scrollTop());
  EXPECT_EQ(10, window->scrollX());
  EXPECT_EQ(20, window->scrollY());

  // Turning on the standards-compliant viewport scrolling impl should make the
  // document element the viewport and not body.
  RuntimeEnabledFeatures::setScrollTopLeftInteropEnabled(true);

  window->scrollTo(100, 150);
  EXPECT_EQ(100, window->scrollX());
  EXPECT_EQ(150, window->scrollY());
  EXPECT_FLOAT_SIZE_EQ(FloatSize(100, 150), visualViewport.GetScrollOffset());

  body->setScrollLeft(50);
  body->setScrollTop(130);
  EXPECT_EQ(0, body->scrollLeft());
  EXPECT_EQ(0, body->scrollTop());
  EXPECT_FLOAT_SIZE_EQ(FloatSize(100, 150), visualViewport.GetScrollOffset());

  documentElement->setScrollLeft(40);
  documentElement->setScrollTop(50);
  EXPECT_EQ(40, documentElement->scrollLeft());
  EXPECT_EQ(50, documentElement->scrollTop());
  EXPECT_FLOAT_SIZE_EQ(FloatSize(40, 50), visualViewport.GetScrollOffset());

  visualViewport.SetLocation(FloatPoint(10, 20));
  EXPECT_EQ(0, body->scrollLeft());
  EXPECT_EQ(0, body->scrollTop());
  EXPECT_EQ(10, documentElement->scrollLeft());
  EXPECT_EQ(20, documentElement->scrollTop());
  EXPECT_EQ(10, window->scrollX());
  EXPECT_EQ(20, window->scrollY());
}

// Tests that when a new frame is created, it is created with the intended size
// (i.e. viewport at minimum scale, 100x200 / 0.5).
TEST_P(VisualViewportTest, TestMainFrameInitializationSizing) {
  initializeWithAndroidSettings();

  webViewImpl()->Resize(IntSize(100, 200));

  registerMockedHttpURLLoad("content-width-1000-min-scale.html");
  navigateTo(m_baseURL + "content-width-1000-min-scale.html");

  WebLocalFrameImpl* localFrame = webViewImpl()->MainFrameImpl();
  // The shutdown() calls are a hack to prevent this test from violating
  // invariants about frame state during navigation/detach.
  localFrame->GetFrame()->GetDocument()->Shutdown();
  localFrame->CreateFrameView();

  FrameView& frameView = *localFrame->GetFrameView();
  EXPECT_SIZE_EQ(IntSize(200, 400), frameView.FrameRect().Size());
  frameView.Dispose();
}

// Tests that the maximum scroll offset of the viewport can be fractional.
TEST_P(VisualViewportTest, FractionalMaxScrollOffset) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(101, 201));
  navigateTo("about:blank");

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();
  ScrollableArea* scrollableArea = &visualViewport;

  webViewImpl()->SetPageScaleFactor(1.0);
  EXPECT_SIZE_EQ(ScrollOffset(), scrollableArea->MaximumScrollOffset());

  webViewImpl()->SetPageScaleFactor(2);
  EXPECT_SIZE_EQ(ScrollOffset(101. / 2., 201. / 2.),
                 scrollableArea->MaximumScrollOffset());
}

// Tests that the slow scrolling after an impl scroll on the visual viewport is
// continuous. crbug.com/453460 was caused by the impl-path not updating the
// ScrollAnimatorBase class.
TEST_P(VisualViewportTest, SlowScrollAfterImplScroll) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(800, 600));
  navigateTo("about:blank");

  VisualViewport& visualViewport = frame()->GetPage()->GetVisualViewport();

  // Apply some scroll and scale from the impl-side.
  webViewImpl()->ApplyViewportDeltas(WebFloatSize(300, 200), WebFloatSize(0, 0),
                                     WebFloatSize(0, 0), 2, 0);

  EXPECT_SIZE_EQ(FloatSize(300, 200), visualViewport.GetScrollOffset());

  // Send a scroll event on the main thread path.
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

  frame()->GetEventHandler().HandleGestureEvent(gsu);

  // The scroll sent from the impl-side must not be overwritten.
  EXPECT_SIZE_EQ(FloatSize(350, 260), visualViewport.GetScrollOffset());
}

static void accessibilitySettings(WebSettings* settings) {
  VisualViewportTest::configureSettings(settings);
  settings->SetAccessibilityEnabled(true);
}

TEST_P(VisualViewportTest, AccessibilityHitTestWhileZoomedIn) {
  initializeWithDesktopSettings(accessibilitySettings);

  registerMockedHttpURLLoad("hit-test.html");
  navigateTo(m_baseURL + "hit-test.html");

  webViewImpl()->Resize(IntSize(500, 500));
  webViewImpl()->UpdateAllLifecyclePhases();

  WebDocument webDoc = webViewImpl()->MainFrame()->GetDocument();
  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();

  webViewImpl()->SetPageScaleFactor(2);
  webViewImpl()->SetVisualViewportOffset(WebFloatPoint(200, 230));
  frameView.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(400, 1100), kProgrammaticScroll);

  // FIXME(504057): PaintLayerScrollableArea dirties the compositing state.
  forceFullCompositingUpdate();

  // Because of where the visual viewport is located, this should hit the bottom
  // right target (target 4).
  WebAXObject hitNode =
      webDoc.AccessibilityObject().HitTest(WebPoint(154, 165));
  WebAXNameFrom nameFrom;
  WebVector<WebAXObject> nameObjects;
  EXPECT_EQ(std::string("Target4"),
            hitNode.GetName(nameFrom, nameObjects).Utf8());
}

// Tests that the maximum scroll offset of the viewport can be fractional.
TEST_P(VisualViewportTest, TestCoordinateTransforms) {
  initializeWithAndroidSettings();
  webViewImpl()->Resize(IntSize(800, 600));
  registerMockedHttpURLLoad("content-width-1000.html");
  navigateTo(m_baseURL + "content-width-1000.html");

  VisualViewport& visualViewport =
      webViewImpl()->GetPage()->GetVisualViewport();
  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();

  // At scale = 1 the transform should be a no-op.
  visualViewport.SetScale(1);
  EXPECT_FLOAT_POINT_EQ(
      FloatPoint(314, 273),
      visualViewport.ViewportToRootFrame(FloatPoint(314, 273)));
  EXPECT_FLOAT_POINT_EQ(
      FloatPoint(314, 273),
      visualViewport.RootFrameToViewport(FloatPoint(314, 273)));

  // At scale = 2.
  visualViewport.SetScale(2);
  EXPECT_FLOAT_POINT_EQ(FloatPoint(55, 75), visualViewport.ViewportToRootFrame(
                                                FloatPoint(110, 150)));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(110, 150),
                        visualViewport.RootFrameToViewport(FloatPoint(55, 75)));

  // At scale = 2 and with the visual viewport offset.
  visualViewport.SetLocation(FloatPoint(10, 12));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(50, 62), visualViewport.ViewportToRootFrame(
                                                FloatPoint(80, 100)));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(80, 100),
                        visualViewport.RootFrameToViewport(FloatPoint(50, 62)));

  // Test points that will cause non-integer values.
  EXPECT_FLOAT_POINT_EQ(
      FloatPoint(50.5, 62.4),
      visualViewport.ViewportToRootFrame(FloatPoint(81, 100.8)));
  EXPECT_FLOAT_POINT_EQ(
      FloatPoint(81, 100.8),
      visualViewport.RootFrameToViewport(FloatPoint(50.5, 62.4)));

  // Scrolling the main frame should have no effect.
  frameView.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(100, 120), kProgrammaticScroll);
  EXPECT_FLOAT_POINT_EQ(FloatPoint(50, 62), visualViewport.ViewportToRootFrame(
                                                FloatPoint(80, 100)));
  EXPECT_FLOAT_POINT_EQ(FloatPoint(80, 100),
                        visualViewport.RootFrameToViewport(FloatPoint(50, 62)));
}

// Tests that the window dimensions are available before a full layout occurs.
// More specifically, it checks that the innerWidth and innerHeight window
// properties will trigger a layout which will cause an update to viewport
// constraints and a refreshed initial scale. crbug.com/466718
TEST_P(VisualViewportTest, WindowDimensionsOnLoad) {
  initializeWithAndroidSettings();
  registerMockedHttpURLLoad("window_dimensions.html");
  webViewImpl()->Resize(IntSize(800, 600));
  navigateTo(m_baseURL + "window_dimensions.html");

  Element* output = frame()->GetDocument()->GetElementById("output");
  DCHECK(output);
  EXPECT_EQ(std::string("1600x1200"),
            std::string(output->innerHTML().Ascii().Data()));
}

// Similar to above but make sure the initial scale is updated with the content
// width for a very wide page. That is, make that innerWidth/Height actually
// trigger a layout of the content, and not just an update of the viepwort.
// crbug.com/466718
TEST_P(VisualViewportTest, WindowDimensionsOnLoadWideContent) {
  initializeWithAndroidSettings();
  registerMockedHttpURLLoad("window_dimensions_wide_div.html");
  webViewImpl()->Resize(IntSize(800, 600));
  navigateTo(m_baseURL + "window_dimensions_wide_div.html");

  Element* output = frame()->GetDocument()->GetElementById("output");
  DCHECK(output);
  EXPECT_EQ(std::string("2000x1500"),
            std::string(output->innerHTML().Ascii().Data()));
}

TEST_P(VisualViewportTest, PinchZoomGestureScrollsVisualViewportOnly) {
  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(100, 100));

  registerMockedHttpURLLoad("200-by-800-viewport.html");
  navigateTo(m_baseURL + "200-by-800-viewport.html");

  WebGestureEvent pinchUpdate(WebInputEvent::kGesturePinchUpdate,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::kTimeStampForTesting);
  pinchUpdate.source_device = kWebGestureDeviceTouchpad;
  pinchUpdate.x = 100;
  pinchUpdate.y = 100;
  pinchUpdate.data.pinch_update.scale = 2;
  pinchUpdate.data.pinch_update.zoom_disabled = false;

  webViewImpl()->HandleInputEvent(WebCoalescedInputEvent(pinchUpdate));

  VisualViewport& visualViewport =
      webViewImpl()->GetPage()->GetVisualViewport();
  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();

  EXPECT_FLOAT_SIZE_EQ(FloatSize(50, 50), visualViewport.GetScrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 0),
                 frameView.LayoutViewportScrollableArea()->GetScrollOffset());
}

TEST_P(VisualViewportTest, ResizeWithScrollAnchoring) {
  bool wasScrollAnchoringEnabled =
      RuntimeEnabledFeatures::scrollAnchoringEnabled();
  RuntimeEnabledFeatures::setScrollAnchoringEnabled(true);

  initializeWithDesktopSettings();
  webViewImpl()->Resize(IntSize(800, 600));

  registerMockedHttpURLLoad("icb-relative-content.html");
  navigateTo(m_baseURL + "icb-relative-content.html");

  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();
  frameView.LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(700, 500), kProgrammaticScroll);
  webViewImpl()->UpdateAllLifecyclePhases();

  webViewImpl()->Resize(IntSize(800, 300));
  EXPECT_SIZE_EQ(ScrollOffset(700, 200),
                 frameView.LayoutViewportScrollableArea()->GetScrollOffset());

  RuntimeEnabledFeatures::setScrollAnchoringEnabled(wasScrollAnchoringEnabled);
}

// Ensure that resize anchoring as happens when browser controls hide/show
// affects the scrollable area that's currently set as the root scroller.
TEST_P(VisualViewportTest, ResizeAnchoringWithRootScroller) {
  bool wasRootScrollerEnabled =
      RuntimeEnabledFeatures::setRootScrollerEnabled();
  RuntimeEnabledFeatures::setSetRootScrollerEnabled(true);

  initializeWithAndroidSettings();
  webViewImpl()->Resize(IntSize(800, 600));

  registerMockedHttpURLLoad("root-scroller-div.html");
  navigateTo(m_baseURL + "root-scroller-div.html");

  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();

  Element* scroller = frame()->GetDocument()->GetElementById("rootScroller");
  NonThrowableExceptionState nonThrow;
  frame()->GetDocument()->setRootScroller(scroller, nonThrow);

  webViewImpl()->SetPageScaleFactor(3.f);
  frameView.GetScrollableArea()->SetScrollOffset(ScrollOffset(0, 400),
                                                 kProgrammaticScroll);

  VisualViewport& visualViewport =
      webViewImpl()->GetPage()->GetVisualViewport();
  visualViewport.SetScrollOffset(ScrollOffset(0, 400), kProgrammaticScroll);

  webViewImpl()->Resize(IntSize(800, 500));

  EXPECT_SIZE_EQ(ScrollOffset(),
                 frameView.LayoutViewportScrollableArea()->GetScrollOffset());

  RuntimeEnabledFeatures::setSetRootScrollerEnabled(wasRootScrollerEnabled);
}

// Ensure that resize anchoring as happens when the device is rotated affects
// the scrollable area that's currently set as the root scroller.
TEST_P(VisualViewportTest, RotationAnchoringWithRootScroller) {
  bool wasRootScrollerEnabled =
      RuntimeEnabledFeatures::setRootScrollerEnabled();
  RuntimeEnabledFeatures::setSetRootScrollerEnabled(true);

  initializeWithAndroidSettings();
  webViewImpl()->Resize(IntSize(800, 600));

  registerMockedHttpURLLoad("root-scroller-div.html");
  navigateTo(m_baseURL + "root-scroller-div.html");

  FrameView& frameView = *webViewImpl()->MainFrameImpl()->GetFrameView();

  Element* scroller = frame()->GetDocument()->GetElementById("rootScroller");
  NonThrowableExceptionState nonThrow;
  frame()->GetDocument()->setRootScroller(scroller, nonThrow);
  webViewImpl()->UpdateAllLifecyclePhases();

  scroller->setScrollTop(800);

  webViewImpl()->Resize(IntSize(600, 800));

  EXPECT_SIZE_EQ(ScrollOffset(),
                 frameView.LayoutViewportScrollableArea()->GetScrollOffset());
  EXPECT_EQ(600, scroller->scrollTop());

  RuntimeEnabledFeatures::setSetRootScrollerEnabled(wasRootScrollerEnabled);
}

static void configureAndroidCompositing(WebSettings* settings) {
  settings->SetAcceleratedCompositingEnabled(true);
  settings->SetPreferCompositingToLCDTextEnabled(true);
  settings->SetViewportMetaEnabled(true);
  settings->SetViewportEnabled(true);
  settings->SetMainFrameResizesAreOrientationChanges(true);
  settings->SetShrinksViewportContentToFit(true);
}

// Make sure a composited background-attachment:fixed background gets resized
// when using inert (non-layout affecting) browser controls.
TEST_P(VisualViewportTest, ResizeCompositedAndFixedBackground) {
  bool originalInertTopControls =
      RuntimeEnabledFeatures::inertTopControlsEnabled();
  RuntimeEnabledFeatures::setInertTopControlsEnabled(true);

  std::unique_ptr<FrameTestHelpers::TestWebViewClient>
      fakeCompositingWebViewClient =
          WTF::MakeUnique<FrameTestHelpers::TestWebViewClient>();
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.Initialize(
      true, nullptr, fakeCompositingWebViewClient.get(), nullptr,
      &configureAndroidCompositing);

  int pageWidth = 640;
  int pageHeight = 480;
  float browserControlsHeight = 50.0f;
  int smallestHeight = pageHeight - browserControlsHeight;

  webViewImpl->ResizeWithBrowserControls(WebSize(pageWidth, pageHeight),
                                         browserControlsHeight, false);

  registerMockedHttpURLLoad("http://example.com/foo.png", "white-1x1.png");
  WebURL baseURL = URLTestHelpers::ToKURL("http://example.com/");
  FrameTestHelpers::LoadHTMLString(webViewImpl->MainFrame(),
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
                                   baseURL);
  webViewImpl->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(webViewImpl->GetPage()->MainFrame())->GetDocument();
  PaintLayerCompositor* compositor = document->GetLayoutView()->Compositor();

  ASSERT_TRUE(compositor->NeedsFixedRootBackgroundLayer(
      document->GetLayoutView()->Layer()));
  ASSERT_TRUE(compositor->FixedRootBackgroundLayer());

  ASSERT_EQ(pageWidth, compositor->FixedRootBackgroundLayer()->Size().Width());
  ASSERT_EQ(pageHeight,
            compositor->FixedRootBackgroundLayer()->Size().Height());
  ASSERT_EQ(pageWidth, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(smallestHeight, document->View()->GetLayoutSize().Height());

  webViewImpl->ResizeWithBrowserControls(WebSize(pageWidth, smallestHeight),
                                         browserControlsHeight, true);

  // The layout size should not have changed.
  ASSERT_EQ(pageWidth, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(smallestHeight, document->View()->GetLayoutSize().Height());

  // The background layer's size should have changed though.
  EXPECT_EQ(pageWidth, compositor->FixedRootBackgroundLayer()->Size().Width());
  EXPECT_EQ(smallestHeight,
            compositor->FixedRootBackgroundLayer()->Size().Height());

  webViewImpl->ResizeWithBrowserControls(WebSize(pageWidth, pageHeight),
                                         browserControlsHeight, true);

  // The background layer's size should change again.
  EXPECT_EQ(pageWidth, compositor->FixedRootBackgroundLayer()->Size().Width());
  EXPECT_EQ(pageHeight,
            compositor->FixedRootBackgroundLayer()->Size().Height());

  RuntimeEnabledFeatures::setInertTopControlsEnabled(originalInertTopControls);
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
  bool originalInertTopControls =
      RuntimeEnabledFeatures::inertTopControlsEnabled();
  RuntimeEnabledFeatures::setInertTopControlsEnabled(true);

  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.Initialize(
      true, nullptr, nullptr, nullptr, &configureAndroidNonCompositing);

  int pageWidth = 640;
  int pageHeight = 480;
  float browserControlsHeight = 50.0f;
  int smallestHeight = pageHeight - browserControlsHeight;

  webViewImpl->ResizeWithBrowserControls(WebSize(pageWidth, pageHeight),
                                         browserControlsHeight, false);

  registerMockedHttpURLLoad("http://example.com/foo.png", "white-1x1.png");
  WebURL baseURL = URLTestHelpers::ToKURL("http://example.com/");
  FrameTestHelpers::LoadHTMLString(webViewImpl->MainFrame(),
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
                                   baseURL);
  webViewImpl->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(webViewImpl->GetPage()->MainFrame())->GetDocument();
  PaintLayerCompositor* compositor = document->GetLayoutView()->Compositor();

  ASSERT_FALSE(compositor->NeedsFixedRootBackgroundLayer(
      document->GetLayoutView()->Layer()));
  ASSERT_FALSE(compositor->FixedRootBackgroundLayer());

  document->View()->SetTracksPaintInvalidations(true);
  webViewImpl->ResizeWithBrowserControls(WebSize(pageWidth, smallestHeight),
                                         browserControlsHeight, true);

  // The layout size should not have changed.
  ASSERT_EQ(pageWidth, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(smallestHeight, document->View()->GetLayoutSize().Height());

  const RasterInvalidationTracking* invalidationTracking =
      document->GetLayoutView()
          ->Layer()
          ->GraphicsLayerBacking(document->GetLayoutView())
          ->GetRasterInvalidationTracking();
  // If no invalidations occured, this will be a nullptr.
  ASSERT_TRUE(invalidationTracking);

  const auto* rasterInvalidations =
      &invalidationTracking->tracked_raster_invalidations;

  bool rootLayerScrolling = GetParam();

  // Without root-layer-scrolling, the LayoutView is the size of the document
  // content so invalidating it for background-attachment: fixed
  // overinvalidates as we should only need to invalidate the viewport size.
  // With root-layer-scrolling, we should invalidate the entire viewport height.
  int expectedHeight = rootLayerScrolling ? pageHeight : 1000;

  // The entire viewport should have been invalidated.
  EXPECT_EQ(1u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(0, 0, 640, expectedHeight), (*rasterInvalidations)[0].rect);
  document->View()->SetTracksPaintInvalidations(false);

  invalidationTracking = document->GetLayoutView()
                             ->Layer()
                             ->GraphicsLayerBacking(document->GetLayoutView())
                             ->GetRasterInvalidationTracking();
  ASSERT_FALSE(invalidationTracking);

  document->View()->SetTracksPaintInvalidations(true);
  webViewImpl->ResizeWithBrowserControls(WebSize(pageWidth, pageHeight),
                                         browserControlsHeight, true);

  invalidationTracking = document->GetLayoutView()
                             ->Layer()
                             ->GraphicsLayerBacking(document->GetLayoutView())
                             ->GetRasterInvalidationTracking();
  ASSERT_TRUE(invalidationTracking);
  rasterInvalidations = &invalidationTracking->tracked_raster_invalidations;

  // Once again, the entire page should have been invalidated.
  expectedHeight = rootLayerScrolling ? 480 : 1000;
  EXPECT_EQ(1u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(0, 0, 640, expectedHeight), (*rasterInvalidations)[0].rect);

  document->View()->SetTracksPaintInvalidations(false);
  RuntimeEnabledFeatures::setInertTopControlsEnabled(originalInertTopControls);
}

// Make sure a browser control resize with background-attachment:not-fixed
// background doesn't cause invalidation or layout.
TEST_P(VisualViewportTest, ResizeNonFixedBackgroundNoLayoutOrInvalidation) {
  bool originalInertTopControls =
      RuntimeEnabledFeatures::inertTopControlsEnabled();
  RuntimeEnabledFeatures::setInertTopControlsEnabled(true);

  std::unique_ptr<FrameTestHelpers::TestWebViewClient>
      fakeCompositingWebViewClient =
          WTF::MakeUnique<FrameTestHelpers::TestWebViewClient>();
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.Initialize(
      true, nullptr, fakeCompositingWebViewClient.get(), nullptr,
      &configureAndroidCompositing);

  int pageWidth = 640;
  int pageHeight = 480;
  float browserControlsHeight = 50.0f;
  int smallestHeight = pageHeight - browserControlsHeight;

  webViewImpl->ResizeWithBrowserControls(WebSize(pageWidth, pageHeight),
                                         browserControlsHeight, false);

  registerMockedHttpURLLoad("http://example.com/foo.png", "white-1x1.png");
  WebURL baseURL = URLTestHelpers::ToKURL("http://example.com/");
  // This time the background is the default attachment.
  FrameTestHelpers::LoadHTMLString(webViewImpl->MainFrame(),
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
                                   baseURL);
  webViewImpl->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(webViewImpl->GetPage()->MainFrame())->GetDocument();

  // A resize will do a layout synchronously so manually check that we don't
  // setNeedsLayout from viewportSizeChanged.
  document->View()->ViewportSizeChanged(false, true);
  unsigned needsLayoutObjects = 0;
  unsigned totalObjects = 0;
  bool isSubtree = false;
  EXPECT_FALSE(document->View()->NeedsLayout());
  document->View()->CountObjectsNeedingLayout(needsLayoutObjects, totalObjects,
                                              isSubtree);
  EXPECT_EQ(0u, needsLayoutObjects);

  webViewImpl->UpdateAllLifecyclePhases();

  // Do a real resize to check for invalidations.
  document->View()->SetTracksPaintInvalidations(true);
  webViewImpl->ResizeWithBrowserControls(WebSize(pageWidth, smallestHeight),
                                         browserControlsHeight, true);

  // The layout size should not have changed.
  ASSERT_EQ(pageWidth, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(smallestHeight, document->View()->GetLayoutSize().Height());

  const RasterInvalidationTracking* invalidationTracking =
      document->GetLayoutView()
          ->Layer()
          ->GraphicsLayerBacking(document->GetLayoutView())
          ->GetRasterInvalidationTracking();

  // No invalidations should have occured in FrameView scrolling. If
  // root-layer-scrolls is on, an invalidation is necessary for now, see the
  // comment and TODO in FrameView::viewportSizeChanged.
  // http://crbug.com/568847.
  bool rootLayerScrolling = GetParam();
  if (rootLayerScrolling)
    EXPECT_TRUE(invalidationTracking);
  else
    EXPECT_FALSE(invalidationTracking);

  document->View()->SetTracksPaintInvalidations(false);
  RuntimeEnabledFeatures::setInertTopControlsEnabled(originalInertTopControls);
}

TEST_P(VisualViewportTest, InvalidateLayoutViewWhenDocumentSmallerThanView) {
  bool originalInertTopControls =
      RuntimeEnabledFeatures::inertTopControlsEnabled();
  RuntimeEnabledFeatures::setInertTopControlsEnabled(true);

  std::unique_ptr<FrameTestHelpers::TestWebViewClient>
      fakeCompositingWebViewClient =
          WTF::MakeUnique<FrameTestHelpers::TestWebViewClient>();
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.Initialize(
      true, nullptr, fakeCompositingWebViewClient.get(), nullptr,
      &configureAndroidCompositing);

  int pageWidth = 320;
  int pageHeight = 590;
  float browserControlsHeight = 50.0f;
  int largestHeight = pageHeight + browserControlsHeight;

  webViewImpl->ResizeWithBrowserControls(WebSize(pageWidth, pageHeight),
                                         browserControlsHeight, true);

  FrameTestHelpers::LoadFrame(webViewImpl->MainFrame(), "about:blank");
  webViewImpl->UpdateAllLifecyclePhases();

  Document* document =
      ToLocalFrame(webViewImpl->GetPage()->MainFrame())->GetDocument();

  // Do a resize to check for invalidations.
  document->View()->SetTracksPaintInvalidations(true);
  webViewImpl->ResizeWithBrowserControls(WebSize(pageWidth, largestHeight),
                                         browserControlsHeight, false);

  // The layout size should not have changed.
  ASSERT_EQ(pageWidth, document->View()->GetLayoutSize().Width());
  ASSERT_EQ(pageHeight, document->View()->GetLayoutSize().Height());

  // The entire viewport should have been invalidated.
  {
    const RasterInvalidationTracking* invalidationTracking =
        document->GetLayoutView()
            ->Layer()
            ->GraphicsLayerBacking()
            ->GetRasterInvalidationTracking();
    ASSERT_TRUE(invalidationTracking);
    const auto* rasterInvalidations =
        &invalidationTracking->tracked_raster_invalidations;
    ASSERT_EQ(1u, rasterInvalidations->size());
    EXPECT_EQ(IntRect(0, 0, pageWidth, largestHeight),
              (*rasterInvalidations)[0].rect);
  }

  document->View()->SetTracksPaintInvalidations(false);
  RuntimeEnabledFeatures::setInertTopControlsEnabled(originalInertTopControls);
}

// Make sure we don't crash when the visual viewport's height is 0. This can
// happen transiently in autoresize mode and cause a crash. This test passes if
// it doesn't crash.
TEST_P(VisualViewportTest, AutoResizeNoHeightUsesMinimumHeight) {
  initializeWithDesktopSettings();
  webViewImpl()->ResizeWithBrowserControls(WebSize(0, 0), 0, false);
  webViewImpl()->EnableAutoResizeMode(WebSize(25, 25), WebSize(100, 100));
  WebURL baseURL = URLTestHelpers::ToKURL("http://example.com/");
  FrameTestHelpers::LoadHTMLString(webViewImpl()->MainFrame(),
                                   "<!DOCTYPE html>"
                                   "<style>"
                                   "  body {"
                                   "    margin: 0px;"
                                   "  }"
                                   "  div { height:110vh; width: 110vw; }"
                                   "</style>"
                                   "<div></div>",
                                   baseURL);
}

}  // namespace
