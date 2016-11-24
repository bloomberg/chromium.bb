/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/web/WebView.h"

#include "bindings/core/v8/V8Document.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/Page.h"
#include "core/page/ScopedPageSuspender.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerPainter.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "platform/KeyboardCodes.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDisplayMode.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebDragOperation.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebMockClipboard.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebCache.h"
#include "public/web/WebDateTimeChooserCompletion.h"
#include "public/web/WebDeviceEmulationParams.h"
#include "public/web/WebDocument.h"
#include "public/web/WebElement.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebFrameContentDumper.h"
#include "public/web/WebHitTestResult.h"
#include "public/web/WebInputMethodController.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebSettings.h"
#include "public/web/WebTreeScopeType.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebWidget.h"
#include "public/web/WebWidgetClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "web/DevToolsEmulator.h"
#include "web/WebInputMethodControllerImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "wtf/PtrUtil.h"
#include <memory>

#if OS(MACOSX)
#include "public/web/mac/WebSubstringUtil.h"
#endif

using blink::FrameTestHelpers::loadFrame;
using blink::URLTestHelpers::toKURL;
using blink::URLTestHelpers::registerMockedURLLoad;
using blink::testing::runPendingTasks;

namespace blink {

enum HorizontalScrollbarState {
  NoHorizontalScrollbar,
  VisibleHorizontalScrollbar,
};

enum VerticalScrollbarState {
  NoVerticalScrollbar,
  VisibleVerticalScrollbar,
};

class TestData {
 public:
  void setWebView(WebView* webView) { m_webView = toWebViewImpl(webView); }
  void setSize(const WebSize& newSize) { m_size = newSize; }
  HorizontalScrollbarState getHorizontalScrollbarState() const {
    return m_webView->hasHorizontalScrollbar() ? VisibleHorizontalScrollbar
                                               : NoHorizontalScrollbar;
  }
  VerticalScrollbarState getVerticalScrollbarState() const {
    return m_webView->hasVerticalScrollbar() ? VisibleVerticalScrollbar
                                             : NoVerticalScrollbar;
  }
  int width() const { return m_size.width; }
  int height() const { return m_size.height; }

 private:
  WebSize m_size;
  WebViewImpl* m_webView;
};

class AutoResizeWebViewClient : public FrameTestHelpers::TestWebViewClient {
 public:
  // WebViewClient methods
  void didAutoResize(const WebSize& newSize) override {
    m_testData.setSize(newSize);
  }

  // Local methods
  TestData& testData() { return m_testData; }

 private:
  TestData m_testData;
};

class TapHandlingWebViewClient : public FrameTestHelpers::TestWebViewClient {
 public:
  // WebViewClient methods
  void didHandleGestureEvent(const WebGestureEvent& event,
                             bool eventCancelled) override {
    if (event.type == WebInputEvent::GestureTap) {
      m_tapX = event.x;
      m_tapY = event.y;
    } else if (event.type == WebInputEvent::GestureLongPress) {
      m_longpressX = event.x;
      m_longpressY = event.y;
    }
  }

  // Local methods
  void reset() {
    m_tapX = -1;
    m_tapY = -1;
    m_longpressX = -1;
    m_longpressY = -1;
  }
  int tapX() { return m_tapX; }
  int tapY() { return m_tapY; }
  int longpressX() { return m_longpressX; }
  int longpressY() { return m_longpressY; }

 private:
  int m_tapX;
  int m_tapY;
  int m_longpressX;
  int m_longpressY;
};

class DateTimeChooserWebViewClient
    : public FrameTestHelpers::TestWebViewClient {
 public:
  WebDateTimeChooserCompletion* chooserCompletion() {
    return m_chooserCompletion;
  }

  void clearChooserCompletion() { m_chooserCompletion = 0; }

  // WebViewClient methods
  bool openDateTimeChooser(
      const WebDateTimeChooserParams&,
      WebDateTimeChooserCompletion* chooser_completion) override {
    m_chooserCompletion = chooser_completion;
    return true;
  }

 private:
  WebDateTimeChooserCompletion* m_chooserCompletion;
};

typedef bool TestParamRootLayerScrolling;
class WebViewTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest {
 public:
  WebViewTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        m_baseURL("http://www.test.com/") {}

  void TearDown() override {
    Platform::current()->getURLLoaderMockFactory()->unregisterAllURLs();
    WebCache::clear();
  }

 protected:
  void registerMockedHttpURLLoad(const std::string& fileName) {
    URLTestHelpers::registerMockedURLFromBaseURL(
        WebString::fromUTF8(m_baseURL.c_str()),
        WebString::fromUTF8(fileName.c_str()));
  }

  void testAutoResize(const WebSize& minAutoResize,
                      const WebSize& maxAutoResize,
                      const std::string& pageWidth,
                      const std::string& pageHeight,
                      int expectedWidth,
                      int expectedHeight,
                      HorizontalScrollbarState expectedHorizontalState,
                      VerticalScrollbarState expectedVerticalState);

  void testTextInputType(WebTextInputType expectedType,
                         const std::string& htmlFile);
  void testInputMode(WebTextInputMode expectedInputMode,
                     const std::string& htmlFile);
  bool tapElement(WebInputEvent::Type, Element*);
  bool tapElementById(WebInputEvent::Type, const WebString& id);

  std::string m_baseURL;
  FrameTestHelpers::WebViewHelper m_webViewHelper;
};

static bool hitTestIsContentEditable(WebView* view, int x, int y) {
  WebPoint hitPoint(x, y);
  WebHitTestResult hitTestResult = view->hitTestResultAt(hitPoint);
  return hitTestResult.isContentEditable();
}

static std::string hitTestElementId(WebView* view, int x, int y) {
  WebPoint hitPoint(x, y);
  WebHitTestResult hitTestResult = view->hitTestResultAt(hitPoint);
  return hitTestResult.node().to<WebElement>().getAttribute("id").utf8();
}

INSTANTIATE_TEST_CASE_P(All, WebViewTest, ::testing::Bool());

TEST_P(WebViewTest, HitTestContentEditableImageMaps) {
  std::string url = m_baseURL + "content-editable-image-maps.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url),
                                        "content-editable-image-maps.html");
  WebView* webView = m_webViewHelper.initializeAndLoad(url, true, 0);
  webView->resize(WebSize(500, 500));

  EXPECT_EQ("areaANotEditable", hitTestElementId(webView, 25, 25));
  EXPECT_FALSE(hitTestIsContentEditable(webView, 25, 25));
  EXPECT_EQ("imageANotEditable", hitTestElementId(webView, 75, 25));
  EXPECT_FALSE(hitTestIsContentEditable(webView, 75, 25));

  EXPECT_EQ("areaBNotEditable", hitTestElementId(webView, 25, 125));
  EXPECT_FALSE(hitTestIsContentEditable(webView, 25, 125));
  EXPECT_EQ("imageBEditable", hitTestElementId(webView, 75, 125));
  EXPECT_TRUE(hitTestIsContentEditable(webView, 75, 125));

  EXPECT_EQ("areaCNotEditable", hitTestElementId(webView, 25, 225));
  EXPECT_FALSE(hitTestIsContentEditable(webView, 25, 225));
  EXPECT_EQ("imageCNotEditable", hitTestElementId(webView, 75, 225));
  EXPECT_FALSE(hitTestIsContentEditable(webView, 75, 225));

  EXPECT_EQ("areaDEditable", hitTestElementId(webView, 25, 325));
  EXPECT_TRUE(hitTestIsContentEditable(webView, 25, 325));
  EXPECT_EQ("imageDNotEditable", hitTestElementId(webView, 75, 325));
  EXPECT_FALSE(hitTestIsContentEditable(webView, 75, 325));
}

static std::string hitTestAbsoluteUrl(WebView* view, int x, int y) {
  WebPoint hitPoint(x, y);
  WebHitTestResult hitTestResult = view->hitTestResultAt(hitPoint);
  return hitTestResult.absoluteImageURL().string().utf8();
}

static WebElement hitTestUrlElement(WebView* view, int x, int y) {
  WebPoint hitPoint(x, y);
  WebHitTestResult hitTestResult = view->hitTestResultAt(hitPoint);
  return hitTestResult.urlElement();
}

TEST_P(WebViewTest, ImageMapUrls) {
  std::string url = m_baseURL + "image-map.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "image-map.html");
  WebView* webView = m_webViewHelper.initializeAndLoad(url, true, 0);
  webView->resize(WebSize(400, 400));

  std::string imageUrl =
      "data:image/gif;base64,R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs=";

  EXPECT_EQ("area", hitTestElementId(webView, 25, 25));
  EXPECT_EQ("area",
            hitTestUrlElement(webView, 25, 25).getAttribute("id").utf8());
  EXPECT_EQ(imageUrl, hitTestAbsoluteUrl(webView, 25, 25));

  EXPECT_EQ("image", hitTestElementId(webView, 75, 25));
  EXPECT_TRUE(hitTestUrlElement(webView, 75, 25).isNull());
  EXPECT_EQ(imageUrl, hitTestAbsoluteUrl(webView, 75, 25));
}

TEST_P(WebViewTest, BrokenImage) {
  URLTestHelpers::registerMockedErrorURLLoad(
      KURL(toKURL(m_baseURL), "non_existent.png"));
  std::string url = m_baseURL + "image-broken.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "image-broken.html");

  WebView* webView = m_webViewHelper.initialize();
  webView->settings()->setLoadsImagesAutomatically(true);
  loadFrame(webView->mainFrame(), url);
  webView->resize(WebSize(400, 400));

  std::string imageUrl = "http://www.test.com/non_existent.png";

  EXPECT_EQ("image", hitTestElementId(webView, 25, 25));
  EXPECT_TRUE(hitTestUrlElement(webView, 25, 25).isNull());
  EXPECT_EQ(imageUrl, hitTestAbsoluteUrl(webView, 25, 25));
}

TEST_P(WebViewTest, BrokenInputImage) {
  URLTestHelpers::registerMockedErrorURLLoad(
      KURL(toKURL(m_baseURL), "non_existent.png"));
  std::string url = m_baseURL + "input-image-broken.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "input-image-broken.html");

  WebView* webView = m_webViewHelper.initialize();
  webView->settings()->setLoadsImagesAutomatically(true);
  loadFrame(webView->mainFrame(), url);
  webView->resize(WebSize(400, 400));

  std::string imageUrl = "http://www.test.com/non_existent.png";

  EXPECT_EQ("image", hitTestElementId(webView, 25, 25));
  EXPECT_TRUE(hitTestUrlElement(webView, 25, 25).isNull());
  EXPECT_EQ(imageUrl, hitTestAbsoluteUrl(webView, 25, 25));
}

TEST_P(WebViewTest, SetBaseBackgroundColor) {
  const WebColor kWhite = 0xFFFFFFFF;
  const WebColor kBlue = 0xFF0000FF;
  const WebColor kDarkCyan = 0xFF227788;
  const WebColor kTranslucentPutty = 0x80BFB196;
  const WebColor kTransparent = 0x00000000;

  WebViewImpl* webView = m_webViewHelper.initialize();
  EXPECT_EQ(kWhite, webView->backgroundColor());

  webView->setBaseBackgroundColor(kBlue);
  EXPECT_EQ(kBlue, webView->backgroundColor());

  WebURL baseURL = URLTestHelpers::toKURL("http://example.com/");
  FrameTestHelpers::loadHTMLString(webView->mainFrame(),
                                   "<html><head><style>body "
                                   "{background-color:#227788}</style></head></"
                                   "html>",
                                   baseURL);
  EXPECT_EQ(kDarkCyan, webView->backgroundColor());

  FrameTestHelpers::loadHTMLString(webView->mainFrame(),
                                   "<html><head><style>body "
                                   "{background-color:rgba(255,0,0,0.5)}</"
                                   "style></head></html>",
                                   baseURL);
  // Expected: red (50% alpha) blended atop base of kBlue.
  EXPECT_EQ(0xFF7F0080, webView->backgroundColor());

  webView->setBaseBackgroundColor(kTranslucentPutty);
  // Expected: red (50% alpha) blended atop kTranslucentPutty. Note the alpha.
  EXPECT_EQ(0xBFE93B32, webView->backgroundColor());

  webView->setBaseBackgroundColor(kTransparent);
  FrameTestHelpers::loadHTMLString(webView->mainFrame(),
                                   "<html><head><style>body "
                                   "{background-color:transparent}</style></"
                                   "head></html>",
                                   baseURL);
  // Expected: transparent on top of kTransparent will still be transparent.
  EXPECT_EQ(kTransparent, webView->backgroundColor());

  LocalFrame* frame = webView->mainFrameImpl()->frame();
  // The shutdown() calls are a hack to prevent this test
  // from violating invariants about frame state during navigation/detach.
  frame->document()->shutdown();

  // Creating a new frame view with the background color having 0 alpha.
  frame->createView(IntSize(1024, 768), Color::transparent, true);
  EXPECT_EQ(kTransparent, frame->view()->baseBackgroundColor());
  frame->view()->dispose();

  const Color transparentRed(100, 0, 0, 0);
  frame->createView(IntSize(1024, 768), transparentRed, true);
  EXPECT_EQ(transparentRed, frame->view()->baseBackgroundColor());
  frame->view()->dispose();
}

TEST_P(WebViewTest, SetBaseBackgroundColorBeforeMainFrame) {
  const WebColor kBlue = 0xFF0000FF;
  FrameTestHelpers::TestWebViewClient webViewClient;
  WebViewImpl* webView =
      WebViewImpl::create(&webViewClient, WebPageVisibilityStateVisible);
  EXPECT_NE(kBlue, webView->backgroundColor());
  // webView does not have a frame yet, but we should still be able to set the
  // background color.
  webView->setBaseBackgroundColor(kBlue);
  EXPECT_EQ(kBlue, webView->backgroundColor());
  FrameTestHelpers::TestWebFrameClient webFrameClient;
  WebLocalFrame* frame =
      WebLocalFrame::create(WebTreeScopeType::Document, &webFrameClient);
  webView->setMainFrame(frame);
  webView->close();
}

TEST_P(WebViewTest, SetBaseBackgroundColorAndBlendWithExistingContent) {
  const WebColor kAlphaRed = 0x80FF0000;
  const WebColor kAlphaGreen = 0x8000FF00;
  const int kWidth = 100;
  const int kHeight = 100;

  WebViewImpl* webView = m_webViewHelper.initialize();

  // Set WebView background to green with alpha.
  webView->setBaseBackgroundColor(kAlphaGreen);
  webView->settings()->setShouldClearDocumentBackground(false);
  webView->resize(WebSize(kWidth, kHeight));
  webView->updateAllLifecyclePhases();

  // Set canvas background to red with alpha.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kWidth, kHeight);
  SkCanvas canvas(bitmap);
  canvas.clear(kAlphaRed);

  SkPictureBuilder pictureBuilder(FloatRect(0, 0, kWidth, kHeight));

  // Paint the root of the main frame in the way that CompositedLayerMapping
  // would.
  FrameView* view = m_webViewHelper.webView()->mainFrameImpl()->frameView();
  PaintLayer* rootLayer = view->layoutViewItem().layer();
  LayoutRect paintRect(0, 0, kWidth, kHeight);
  PaintLayerPaintingInfo paintingInfo(rootLayer, paintRect,
                                      GlobalPaintNormalPhase, LayoutSize());
  PaintLayerPainter(*rootLayer)
      .paintLayerContents(pictureBuilder.context(), paintingInfo,
                          PaintLayerPaintingCompositingAllPhases);

  pictureBuilder.endRecording()->playback(&canvas);

  // The result should be a blend of red and green.
  SkColor color = bitmap.getColor(kWidth / 2, kHeight / 2);
  EXPECT_TRUE(redChannel(color));
  EXPECT_TRUE(greenChannel(color));
}

TEST_P(WebViewTest, FocusIsInactive) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), "visible_iframe.html");
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "visible_iframe.html");

  webView->setFocus(true);
  webView->setIsActive(true);
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  EXPECT_TRUE(frame->frame()->document()->isHTMLDocument());

  Document* document = frame->frame()->document();
  EXPECT_TRUE(document->hasFocus());
  webView->setFocus(false);
  webView->setIsActive(false);
  EXPECT_FALSE(document->hasFocus());
  webView->setFocus(true);
  webView->setIsActive(true);
  EXPECT_TRUE(document->hasFocus());
  webView->setFocus(true);
  webView->setIsActive(false);
  EXPECT_FALSE(document->hasFocus());
  webView->setFocus(false);
  webView->setIsActive(true);
  EXPECT_FALSE(document->hasFocus());
}

TEST_P(WebViewTest, ActiveState) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), "visible_iframe.html");
  WebView* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "visible_iframe.html");

  ASSERT_TRUE(webView);

  webView->setIsActive(true);
  EXPECT_TRUE(webView->isActive());

  webView->setIsActive(false);
  EXPECT_FALSE(webView->isActive());

  webView->setIsActive(true);
  EXPECT_TRUE(webView->isActive());
}

TEST_P(WebViewTest, HitTestResultAtWithPageScale) {
  std::string url = m_baseURL + "specify_size.html?" + "50px" + ":" + "50px";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "specify_size.html");
  WebView* webView = m_webViewHelper.initializeAndLoad(url, true, 0);
  webView->resize(WebSize(100, 100));
  WebPoint hitPoint(75, 75);

  // Image is at top left quandrant, so should not hit it.
  WebHitTestResult negativeResult = webView->hitTestResultAt(hitPoint);
  EXPECT_FALSE(negativeResult.node().to<WebElement>().hasHTMLTagName("img"));
  negativeResult.reset();

  // Scale page up 2x so image should occupy the whole viewport.
  webView->setPageScaleFactor(2.0f);
  WebHitTestResult positiveResult = webView->hitTestResultAt(hitPoint);
  EXPECT_TRUE(positiveResult.node().to<WebElement>().hasHTMLTagName("img"));
  positiveResult.reset();
}

TEST_P(WebViewTest, HitTestResultAtWithPageScaleAndPan) {
  std::string url = m_baseURL + "specify_size.html?" + "50px" + ":" + "50px";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "specify_size.html");
  WebView* webView = m_webViewHelper.initialize(true);
  loadFrame(webView->mainFrame(), url);
  webView->resize(WebSize(100, 100));
  WebPoint hitPoint(75, 75);

  // Image is at top left quandrant, so should not hit it.
  WebHitTestResult negativeResult = webView->hitTestResultAt(hitPoint);
  EXPECT_FALSE(negativeResult.node().to<WebElement>().hasHTMLTagName("img"));
  negativeResult.reset();

  // Scale page up 2x so image should occupy the whole viewport.
  webView->setPageScaleFactor(2.0f);
  WebHitTestResult positiveResult = webView->hitTestResultAt(hitPoint);
  EXPECT_TRUE(positiveResult.node().to<WebElement>().hasHTMLTagName("img"));
  positiveResult.reset();

  // Pan around the zoomed in page so the image is not visible in viewport.
  webView->setVisualViewportOffset(WebFloatPoint(100, 100));
  WebHitTestResult negativeResult2 = webView->hitTestResultAt(hitPoint);
  EXPECT_FALSE(negativeResult2.node().to<WebElement>().hasHTMLTagName("img"));
  negativeResult2.reset();
}

TEST_P(WebViewTest, HitTestResultForTapWithTapArea) {
  std::string url = m_baseURL + "hit_test.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "hit_test.html");
  WebView* webView = m_webViewHelper.initializeAndLoad(url, true, 0);
  webView->resize(WebSize(100, 100));
  WebPoint hitPoint(55, 55);

  // Image is at top left quandrant, so should not hit it.
  WebHitTestResult negativeResult = webView->hitTestResultAt(hitPoint);
  EXPECT_FALSE(negativeResult.node().to<WebElement>().hasHTMLTagName("img"));
  negativeResult.reset();

  // The tap area is 20 by 20 square, centered at 55, 55.
  WebSize tapArea(20, 20);
  WebHitTestResult positiveResult =
      webView->hitTestResultForTap(hitPoint, tapArea);
  EXPECT_TRUE(positiveResult.node().to<WebElement>().hasHTMLTagName("img"));
  positiveResult.reset();

  // Move the hit point the image is just outside the tapped area now.
  hitPoint = WebPoint(61, 61);
  WebHitTestResult negativeResult2 =
      webView->hitTestResultForTap(hitPoint, tapArea);
  EXPECT_FALSE(negativeResult2.node().to<WebElement>().hasHTMLTagName("img"));
  negativeResult2.reset();
}

TEST_P(WebViewTest, HitTestResultForTapWithTapAreaPageScaleAndPan) {
  std::string url = m_baseURL + "hit_test.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "hit_test.html");
  WebView* webView = m_webViewHelper.initialize(true);
  loadFrame(webView->mainFrame(), url);
  webView->resize(WebSize(100, 100));
  WebPoint hitPoint(55, 55);

  // Image is at top left quandrant, so should not hit it.
  WebHitTestResult negativeResult = webView->hitTestResultAt(hitPoint);
  EXPECT_FALSE(negativeResult.node().to<WebElement>().hasHTMLTagName("img"));
  negativeResult.reset();

  // The tap area is 20 by 20 square, centered at 55, 55.
  WebSize tapArea(20, 20);
  WebHitTestResult positiveResult =
      webView->hitTestResultForTap(hitPoint, tapArea);
  EXPECT_TRUE(positiveResult.node().to<WebElement>().hasHTMLTagName("img"));
  positiveResult.reset();

  // Zoom in and pan around the page so the image is not visible in viewport.
  webView->setPageScaleFactor(2.0f);
  webView->setVisualViewportOffset(WebFloatPoint(100, 100));
  WebHitTestResult negativeResult2 =
      webView->hitTestResultForTap(hitPoint, tapArea);
  EXPECT_FALSE(negativeResult2.node().to<WebElement>().hasHTMLTagName("img"));
  negativeResult2.reset();
}

void WebViewTest::testAutoResize(
    const WebSize& minAutoResize,
    const WebSize& maxAutoResize,
    const std::string& pageWidth,
    const std::string& pageHeight,
    int expectedWidth,
    int expectedHeight,
    HorizontalScrollbarState expectedHorizontalState,
    VerticalScrollbarState expectedVerticalState) {
  AutoResizeWebViewClient client;
  std::string url =
      m_baseURL + "specify_size.html?" + pageWidth + ":" + pageHeight;
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "specify_size.html");
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(url, true, 0, &client);
  client.testData().setWebView(webView);

  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  FrameView* frameView = frame->frame()->view();
  frameView->layout();
  EXPECT_FALSE(frameView->layoutPending());
  EXPECT_FALSE(frameView->needsLayout());

  webView->enableAutoResizeMode(minAutoResize, maxAutoResize);
  EXPECT_TRUE(frameView->layoutPending());
  EXPECT_TRUE(frameView->needsLayout());
  frameView->layout();

  EXPECT_TRUE(frame->frame()->document()->isHTMLDocument());

  EXPECT_EQ(expectedWidth, client.testData().width());
  EXPECT_EQ(expectedHeight, client.testData().height());

// Android disables main frame scrollbars.
#if !OS(ANDROID)
  EXPECT_EQ(expectedHorizontalState,
            client.testData().getHorizontalScrollbarState());
  EXPECT_EQ(expectedVerticalState,
            client.testData().getVerticalScrollbarState());
#endif

  // Explicitly reset to break dependency on locally scoped client.
  m_webViewHelper.reset();
}

TEST_P(WebViewTest, AutoResizeMinimumSize) {
  WebSize minAutoResize(91, 56);
  WebSize maxAutoResize(403, 302);
  std::string pageWidth = "91px";
  std::string pageHeight = "56px";
  int expectedWidth = 91;
  int expectedHeight = 56;
  testAutoResize(minAutoResize, maxAutoResize, pageWidth, pageHeight,
                 expectedWidth, expectedHeight, NoHorizontalScrollbar,
                 NoVerticalScrollbar);
}

TEST_P(WebViewTest, AutoResizeHeightOverflowAndFixedWidth) {
  WebSize minAutoResize(90, 95);
  WebSize maxAutoResize(90, 100);
  std::string pageWidth = "60px";
  std::string pageHeight = "200px";
  int expectedWidth = 90;
  int expectedHeight = 100;
  testAutoResize(minAutoResize, maxAutoResize, pageWidth, pageHeight,
                 expectedWidth, expectedHeight, NoHorizontalScrollbar,
                 VisibleVerticalScrollbar);
}

TEST_P(WebViewTest, AutoResizeFixedHeightAndWidthOverflow) {
  WebSize minAutoResize(90, 100);
  WebSize maxAutoResize(200, 100);
  std::string pageWidth = "300px";
  std::string pageHeight = "80px";
  int expectedWidth = 200;
  int expectedHeight = 100;
  testAutoResize(minAutoResize, maxAutoResize, pageWidth, pageHeight,
                 expectedWidth, expectedHeight, VisibleHorizontalScrollbar,
                 NoVerticalScrollbar);
}

// Next three tests disabled for https://bugs.webkit.org/show_bug.cgi?id=92318 .
// It seems we can run three AutoResize tests, then the next one breaks.
TEST_P(WebViewTest, AutoResizeInBetweenSizes) {
  WebSize minAutoResize(90, 95);
  WebSize maxAutoResize(200, 300);
  std::string pageWidth = "100px";
  std::string pageHeight = "200px";
  int expectedWidth = 100;
  int expectedHeight = 200;
  testAutoResize(minAutoResize, maxAutoResize, pageWidth, pageHeight,
                 expectedWidth, expectedHeight, NoHorizontalScrollbar,
                 NoVerticalScrollbar);
}

TEST_P(WebViewTest, AutoResizeOverflowSizes) {
  WebSize minAutoResize(90, 95);
  WebSize maxAutoResize(200, 300);
  std::string pageWidth = "300px";
  std::string pageHeight = "400px";
  int expectedWidth = 200;
  int expectedHeight = 300;
  testAutoResize(minAutoResize, maxAutoResize, pageWidth, pageHeight,
                 expectedWidth, expectedHeight, VisibleHorizontalScrollbar,
                 VisibleVerticalScrollbar);
}

TEST_P(WebViewTest, AutoResizeMaxSize) {
  WebSize minAutoResize(90, 95);
  WebSize maxAutoResize(200, 300);
  std::string pageWidth = "200px";
  std::string pageHeight = "300px";
  int expectedWidth = 200;
  int expectedHeight = 300;
  testAutoResize(minAutoResize, maxAutoResize, pageWidth, pageHeight,
                 expectedWidth, expectedHeight, NoHorizontalScrollbar,
                 NoVerticalScrollbar);
}

void WebViewTest::testTextInputType(WebTextInputType expectedType,
                                    const std::string& htmlFile) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8(htmlFile.c_str()));
  WebView* webView = m_webViewHelper.initializeAndLoad(m_baseURL + htmlFile);
  EXPECT_EQ(WebTextInputTypeNone, webView->textInputType());
  EXPECT_EQ(WebTextInputTypeNone, webView->textInputInfo().type);
  webView->setInitialFocus(false);
  EXPECT_EQ(expectedType, webView->textInputType());
  EXPECT_EQ(expectedType, webView->textInputInfo().type);
  webView->clearFocusedElement();
  EXPECT_EQ(WebTextInputTypeNone, webView->textInputType());
  EXPECT_EQ(WebTextInputTypeNone, webView->textInputInfo().type);
}

TEST_P(WebViewTest, TextInputType) {
  testTextInputType(WebTextInputTypeText, "input_field_default.html");
  testTextInputType(WebTextInputTypePassword, "input_field_password.html");
  testTextInputType(WebTextInputTypeEmail, "input_field_email.html");
  testTextInputType(WebTextInputTypeSearch, "input_field_search.html");
  testTextInputType(WebTextInputTypeNumber, "input_field_number.html");
  testTextInputType(WebTextInputTypeTelephone, "input_field_tel.html");
  testTextInputType(WebTextInputTypeURL, "input_field_url.html");
}

TEST_P(WebViewTest, TextInputInfoUpdateStyleAndLayout) {
  FrameTestHelpers::TestWebViewClient client;
  FrameTestHelpers::WebViewHelper m_webViewHelper;
  WebViewImpl* webViewImpl = m_webViewHelper.initialize(true, 0, &client);

  WebURL baseURL = URLTestHelpers::toKURL("http://example.com/");
  // Here, we need to construct a document that has a special property:
  // Adding id="foo" to the <path> element will trigger creation of an SVG
  // instance tree for the use <use> element.
  // This is significant, because SVG instance trees are actually created lazily
  // during Document::updateStyleAndLayout code, thus incrementing the DOM tree
  // version and freaking out the EphemeralRange (invalidating it).
  FrameTestHelpers::loadHTMLString(
      webViewImpl->mainFrame(),
      "<svg height='100%' version='1.1' viewBox='0 0 14 14' width='100%'>"
      "<use xmlns:xlink='http://www.w3.org/1999/xlink' xlink:href='#foo'></use>"
      "<path d='M 100 100 L 300 100 L 200 300 z' fill='#000'></path>"
      "</svg>"
      "<input>",
      baseURL);
  webViewImpl->setInitialFocus(false);

  // Add id="foo" to <path>, thus triggering the condition described above.
  Document* document = webViewImpl->mainFrameImpl()->frame()->document();
  document->body()
      ->querySelector("path", ASSERT_NO_EXCEPTION)
      ->setIdAttribute("foo");

  // This should not DCHECK.
  EXPECT_EQ(WebTextInputTypeText, webViewImpl->textInputInfo().type);
}

void WebViewTest::testInputMode(WebTextInputMode expectedInputMode,
                                const std::string& htmlFile) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8(htmlFile.c_str()));
  WebView* webView = m_webViewHelper.initializeAndLoad(m_baseURL + htmlFile);
  webView->setInitialFocus(false);
  EXPECT_EQ(expectedInputMode, webView->textInputInfo().inputMode);
}

TEST_P(WebViewTest, InputMode) {
  testInputMode(WebTextInputMode::kWebTextInputModeDefault,
                "input_mode_default.html");
  testInputMode(WebTextInputMode::kWebTextInputModeDefault,
                "input_mode_default_unknown.html");
  testInputMode(WebTextInputMode::kWebTextInputModeVerbatim,
                "input_mode_default_verbatim.html");
  testInputMode(WebTextInputMode::kWebTextInputModeVerbatim,
                "input_mode_type_text_verbatim.html");
  testInputMode(WebTextInputMode::kWebTextInputModeVerbatim,
                "input_mode_type_search_verbatim.html");
  testInputMode(WebTextInputMode::kWebTextInputModeDefault,
                "input_mode_type_url_verbatim.html");
  testInputMode(WebTextInputMode::kWebTextInputModeLatin,
                "input_mode_type_latin.html");
  testInputMode(WebTextInputMode::kWebTextInputModeLatinName,
                "input_mode_type_latin_name.html");
  testInputMode(WebTextInputMode::kWebTextInputModeLatinProse,
                "input_mode_type_latin_prose.html");
  testInputMode(WebTextInputMode::kWebTextInputModeFullWidthLatin,
                "input_mode_type_full_width_latin.html");
  testInputMode(WebTextInputMode::kWebTextInputModeKana,
                "input_mode_type_kana.html");
  testInputMode(WebTextInputMode::kWebTextInputModeKanaName,
                "input_mode_type_kana_name.html");
  testInputMode(WebTextInputMode::kWebTextInputModeKataKana,
                "input_mode_type_kata_kana.html");
  testInputMode(WebTextInputMode::kWebTextInputModeNumeric,
                "input_mode_type_numeric.html");
  testInputMode(WebTextInputMode::kWebTextInputModeTel,
                "input_mode_type_tel.html");
  testInputMode(WebTextInputMode::kWebTextInputModeEmail,
                "input_mode_type_email.html");
  testInputMode(WebTextInputMode::kWebTextInputModeUrl,
                "input_mode_type_url.html");
}

TEST_P(WebViewTest, TextInputInfoWithReplacedElements) {
  std::string url = m_baseURL + "div_with_image.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "div_with_image.html");
  URLTestHelpers::registerMockedURLLoad(toKURL("http://www.test.com/foo.png"),
                                        "white-1x1.png");
  WebView* webView = m_webViewHelper.initializeAndLoad(url);
  webView->setInitialFocus(false);
  WebTextInputInfo info = webView->textInputInfo();

  EXPECT_EQ("foo\xef\xbf\xbc", info.value.utf8());
}

TEST_P(WebViewTest, SetEditableSelectionOffsetsAndTextInputInfo) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  webView->setInitialFocus(false);
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->setEditableSelectionOffsets(5, 13);
  EXPECT_EQ("56789abc", frame->selectionAsText());
  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopqrstuvwxyz", info.value);
  EXPECT_EQ(5, info.selectionStart);
  EXPECT_EQ(13, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("content_editable_populated.html"));
  webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "content_editable_populated.html");
  webView->setInitialFocus(false);
  frame = webView->mainFrameImpl();
  frame->setEditableSelectionOffsets(8, 19);
  EXPECT_EQ("89abcdefghi", frame->selectionAsText());
  info = webView->textInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopqrstuvwxyz", info.value);
  EXPECT_EQ(8, info.selectionStart);
  EXPECT_EQ(19, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);
}

// Regression test for crbug.com/663645
TEST_P(WebViewTest, FinishComposingTextDoesNotAssert) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_default.html"));
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "input_field_default.html");
  webView->setInitialFocus(false);

  WebInputMethodController* activeInputMethodController =
      webView->mainFrameImpl()
          ->frameWidget()
          ->getActiveWebInputMethodController();

  // The test requires non-empty composition.
  std::string compositionText("hello");
  WebVector<WebCompositionUnderline> emptyUnderlines;
  activeInputMethodController->setComposition(
      WebString::fromUTF8(compositionText.c_str()), emptyUnderlines, 5, 5);

  // Do arbitrary change to make layout dirty.
  Document& document = *webView->mainFrameImpl()->frame()->document();
  Element* br = document.createElement("br");
  document.body()->appendChild(br);

  // Should not hit assertion when calling
  // WebInputMethodController::finishComposingText with non-empty composition
  // and dirty layout.
  activeInputMethodController->finishComposingText(
      WebInputMethodController::KeepSelection);
}

TEST_P(WebViewTest, FinishComposingTextCursorPositionChange) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  webView->setInitialFocus(false);

  // Set up a composition that needs to be committed.
  std::string compositionText("hello");

  WebInputMethodController* activeInputMethodController =
      webView->mainFrameImpl()
          ->frameWidget()
          ->getActiveWebInputMethodController();
  WebVector<WebCompositionUnderline> emptyUnderlines;
  activeInputMethodController->setComposition(
      WebString::fromUTF8(compositionText.c_str()), emptyUnderlines, 3, 3);

  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ("hello", std::string(info.value.utf8().data()));
  EXPECT_EQ(3, info.selectionStart);
  EXPECT_EQ(3, info.selectionEnd);
  EXPECT_EQ(0, info.compositionStart);
  EXPECT_EQ(5, info.compositionEnd);

  activeInputMethodController->finishComposingText(
      WebInputMethodController::KeepSelection);
  info = webView->textInputInfo();
  EXPECT_EQ(3, info.selectionStart);
  EXPECT_EQ(3, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  activeInputMethodController->setComposition(
      WebString::fromUTF8(compositionText.c_str()), emptyUnderlines, 3, 3);
  info = webView->textInputInfo();
  EXPECT_EQ("helhellolo", std::string(info.value.utf8().data()));
  EXPECT_EQ(6, info.selectionStart);
  EXPECT_EQ(6, info.selectionEnd);
  EXPECT_EQ(3, info.compositionStart);
  EXPECT_EQ(8, info.compositionEnd);

  activeInputMethodController->finishComposingText(
      WebInputMethodController::DoNotKeepSelection);
  info = webView->textInputInfo();
  EXPECT_EQ(8, info.selectionStart);
  EXPECT_EQ(8, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);
}

TEST_P(WebViewTest, SetCompositionForNewCaretPositions) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  webView->setInitialFocus(false);
  WebInputMethodController* activeInputMethodController =
      webView->mainFrameImpl()
          ->frameWidget()
          ->getActiveWebInputMethodController();

  activeInputMethodController->commitText("hello", 0);
  activeInputMethodController->commitText("world", -5);
  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ("helloworld", std::string(info.value.utf8().data()));
  EXPECT_EQ(5, info.selectionStart);
  EXPECT_EQ(5, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  WebVector<WebCompositionUnderline> emptyUnderlines;
  // Set up a composition that needs to be committed.
  std::string compositionText("ABC");

  // Caret is on the left of composing text.
  activeInputMethodController->setComposition(
      WebString::fromUTF8(compositionText.c_str()), emptyUnderlines, 0, 0);
  info = webView->textInputInfo();
  EXPECT_EQ("helloABCworld", std::string(info.value.utf8().data()));
  EXPECT_EQ(5, info.selectionStart);
  EXPECT_EQ(5, info.selectionEnd);
  EXPECT_EQ(5, info.compositionStart);
  EXPECT_EQ(8, info.compositionEnd);

  // Caret is on the right of composing text.
  activeInputMethodController->setComposition(
      WebString::fromUTF8(compositionText.c_str()), emptyUnderlines, 3, 3);
  info = webView->textInputInfo();
  EXPECT_EQ("helloABCworld", std::string(info.value.utf8().data()));
  EXPECT_EQ(8, info.selectionStart);
  EXPECT_EQ(8, info.selectionEnd);
  EXPECT_EQ(5, info.compositionStart);
  EXPECT_EQ(8, info.compositionEnd);

  // Caret is between composing text and left boundary.
  activeInputMethodController->setComposition(
      WebString::fromUTF8(compositionText.c_str()), emptyUnderlines, -2, -2);
  info = webView->textInputInfo();
  EXPECT_EQ("helloABCworld", std::string(info.value.utf8().data()));
  EXPECT_EQ(3, info.selectionStart);
  EXPECT_EQ(3, info.selectionEnd);
  EXPECT_EQ(5, info.compositionStart);
  EXPECT_EQ(8, info.compositionEnd);

  // Caret is between composing text and right boundary.
  activeInputMethodController->setComposition(
      WebString::fromUTF8(compositionText.c_str()), emptyUnderlines, 5, 5);
  info = webView->textInputInfo();
  EXPECT_EQ("helloABCworld", std::string(info.value.utf8().data()));
  EXPECT_EQ(10, info.selectionStart);
  EXPECT_EQ(10, info.selectionEnd);
  EXPECT_EQ(5, info.compositionStart);
  EXPECT_EQ(8, info.compositionEnd);

  // Caret is on the left boundary.
  activeInputMethodController->setComposition(
      WebString::fromUTF8(compositionText.c_str()), emptyUnderlines, -5, -5);
  info = webView->textInputInfo();
  EXPECT_EQ("helloABCworld", std::string(info.value.utf8().data()));
  EXPECT_EQ(0, info.selectionStart);
  EXPECT_EQ(0, info.selectionEnd);
  EXPECT_EQ(5, info.compositionStart);
  EXPECT_EQ(8, info.compositionEnd);

  // Caret is on the right boundary.
  activeInputMethodController->setComposition(
      WebString::fromUTF8(compositionText.c_str()), emptyUnderlines, 8, 8);
  info = webView->textInputInfo();
  EXPECT_EQ("helloABCworld", std::string(info.value.utf8().data()));
  EXPECT_EQ(13, info.selectionStart);
  EXPECT_EQ(13, info.selectionEnd);
  EXPECT_EQ(5, info.compositionStart);
  EXPECT_EQ(8, info.compositionEnd);

  // Caret exceeds the left boundary.
  activeInputMethodController->setComposition(
      WebString::fromUTF8(compositionText.c_str()), emptyUnderlines, -100,
      -100);
  info = webView->textInputInfo();
  EXPECT_EQ("helloABCworld", std::string(info.value.utf8().data()));
  EXPECT_EQ(0, info.selectionStart);
  EXPECT_EQ(0, info.selectionEnd);
  EXPECT_EQ(5, info.compositionStart);
  EXPECT_EQ(8, info.compositionEnd);

  // Caret exceeds the right boundary.
  activeInputMethodController->setComposition(
      WebString::fromUTF8(compositionText.c_str()), emptyUnderlines, 100, 100);
  info = webView->textInputInfo();
  EXPECT_EQ("helloABCworld", std::string(info.value.utf8().data()));
  EXPECT_EQ(13, info.selectionStart);
  EXPECT_EQ(13, info.selectionEnd);
  EXPECT_EQ(5, info.compositionStart);
  EXPECT_EQ(8, info.compositionEnd);
}

TEST_P(WebViewTest, SetCompositionWithEmptyText) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  webView->setInitialFocus(false);
  WebInputMethodController* activeInputMethodController =
      webView->mainFrameImpl()
          ->frameWidget()
          ->getActiveWebInputMethodController();

  activeInputMethodController->commitText("hello", 0);
  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ("hello", std::string(info.value.utf8().data()));
  EXPECT_EQ(5, info.selectionStart);
  EXPECT_EQ(5, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  WebVector<WebCompositionUnderline> emptyUnderlines;

  activeInputMethodController->setComposition(WebString::fromUTF8(""),
                                              emptyUnderlines, 0, 0);
  info = webView->textInputInfo();
  EXPECT_EQ("hello", std::string(info.value.utf8().data()));
  EXPECT_EQ(5, info.selectionStart);
  EXPECT_EQ(5, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  activeInputMethodController->setComposition(WebString::fromUTF8(""),
                                              emptyUnderlines, -2, -2);
  info = webView->textInputInfo();
  EXPECT_EQ("hello", std::string(info.value.utf8().data()));
  EXPECT_EQ(3, info.selectionStart);
  EXPECT_EQ(3, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);
}

TEST_P(WebViewTest, CommitTextForNewCaretPositions) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  webView->setInitialFocus(false);
  WebInputMethodController* activeInputMethodController =
      webView->mainFrameImpl()
          ->frameWidget()
          ->getActiveWebInputMethodController();

  // Caret is on the left of composing text.
  activeInputMethodController->commitText("ab", -2);
  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ("ab", std::string(info.value.utf8().data()));
  EXPECT_EQ(0, info.selectionStart);
  EXPECT_EQ(0, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  // Caret is on the right of composing text.
  activeInputMethodController->commitText("c", 1);
  info = webView->textInputInfo();
  EXPECT_EQ("cab", std::string(info.value.utf8().data()));
  EXPECT_EQ(2, info.selectionStart);
  EXPECT_EQ(2, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  // Caret is on the left boundary.
  activeInputMethodController->commitText("def", -5);
  info = webView->textInputInfo();
  EXPECT_EQ("cadefb", std::string(info.value.utf8().data()));
  EXPECT_EQ(0, info.selectionStart);
  EXPECT_EQ(0, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  // Caret is on the right boundary.
  activeInputMethodController->commitText("g", 6);
  info = webView->textInputInfo();
  EXPECT_EQ("gcadefb", std::string(info.value.utf8().data()));
  EXPECT_EQ(7, info.selectionStart);
  EXPECT_EQ(7, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  // Caret exceeds the left boundary.
  activeInputMethodController->commitText("hi", -100);
  info = webView->textInputInfo();
  EXPECT_EQ("gcadefbhi", std::string(info.value.utf8().data()));
  EXPECT_EQ(0, info.selectionStart);
  EXPECT_EQ(0, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  // Caret exceeds the right boundary.
  activeInputMethodController->commitText("jk", 100);
  info = webView->textInputInfo();
  EXPECT_EQ("jkgcadefbhi", std::string(info.value.utf8().data()));
  EXPECT_EQ(11, info.selectionStart);
  EXPECT_EQ(11, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);
}

TEST_P(WebViewTest, CommitTextWhileComposing) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  webView->setInitialFocus(false);
  WebInputMethodController* activeInputMethodController =
      webView->mainFrameImpl()
          ->frameWidget()
          ->getActiveWebInputMethodController();

  WebVector<WebCompositionUnderline> emptyUnderlines;
  activeInputMethodController->setComposition(WebString::fromUTF8("abc"),
                                              emptyUnderlines, 0, 0);
  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ("abc", std::string(info.value.utf8().data()));
  EXPECT_EQ(0, info.selectionStart);
  EXPECT_EQ(0, info.selectionEnd);
  EXPECT_EQ(0, info.compositionStart);
  EXPECT_EQ(3, info.compositionEnd);

  // Deletes ongoing composition, inserts the specified text and moves the
  // caret.
  activeInputMethodController->commitText("hello", -2);
  info = webView->textInputInfo();
  EXPECT_EQ("hello", std::string(info.value.utf8().data()));
  EXPECT_EQ(3, info.selectionStart);
  EXPECT_EQ(3, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  activeInputMethodController->setComposition(WebString::fromUTF8("abc"),
                                              emptyUnderlines, 0, 0);
  info = webView->textInputInfo();
  EXPECT_EQ("helabclo", std::string(info.value.utf8().data()));
  EXPECT_EQ(3, info.selectionStart);
  EXPECT_EQ(3, info.selectionEnd);
  EXPECT_EQ(3, info.compositionStart);
  EXPECT_EQ(6, info.compositionEnd);

  // Deletes ongoing composition and moves the caret.
  activeInputMethodController->commitText("", 2);
  info = webView->textInputInfo();
  EXPECT_EQ("hello", std::string(info.value.utf8().data()));
  EXPECT_EQ(5, info.selectionStart);
  EXPECT_EQ(5, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  // Inserts the specified text and moves the caret.
  activeInputMethodController->commitText("world", -5);
  info = webView->textInputInfo();
  EXPECT_EQ("helloworld", std::string(info.value.utf8().data()));
  EXPECT_EQ(5, info.selectionStart);
  EXPECT_EQ(5, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  // Only moves the caret.
  activeInputMethodController->commitText("", 5);
  info = webView->textInputInfo();
  EXPECT_EQ("helloworld", std::string(info.value.utf8().data()));
  EXPECT_EQ(10, info.selectionStart);
  EXPECT_EQ(10, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);
}

TEST_P(WebViewTest, FinishCompositionDoesNotRevealSelection) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("form_with_input.html"));
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "form_with_input.html");
  webView->resize(WebSize(800, 600));
  webView->setInitialFocus(false);
  EXPECT_EQ(0, webView->mainFrame()->scrollOffset().width);
  EXPECT_EQ(0, webView->mainFrame()->scrollOffset().height);

  // Set up a composition from existing text that needs to be committed.
  Vector<CompositionUnderline> emptyUnderlines;
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->frame()->inputMethodController().setCompositionFromExistingText(
      emptyUnderlines, 3, 3);

  // Scroll the input field out of the viewport.
  Element* element = static_cast<Element*>(
      webView->mainFrame()->document().getElementById("btn"));
  element->scrollIntoView();
  float offsetHeight = webView->mainFrame()->scrollOffset().height;
  EXPECT_EQ(0, webView->mainFrame()->scrollOffset().width);
  EXPECT_LT(0, offsetHeight);

  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ("hello", std::string(info.value.utf8().data()));

  // Verify that the input field is not scrolled back into the viewport.
  frame->frameWidget()
      ->getActiveWebInputMethodController()
      ->finishComposingText(WebInputMethodController::DoNotKeepSelection);
  EXPECT_EQ(0, webView->mainFrame()->scrollOffset().width);
  EXPECT_EQ(offsetHeight, webView->mainFrame()->scrollOffset().height);
}

TEST_P(WebViewTest, InsertNewLinePlacementAfterFinishComposingText) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("text_area_populated.html"));
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "text_area_populated.html");
  webView->setInitialFocus(false);

  WebVector<WebCompositionUnderline> emptyUnderlines;

  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->setEditableSelectionOffsets(4, 4);
  frame->setCompositionFromExistingText(8, 12, emptyUnderlines);

  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopqrstuvwxyz",
            std::string(info.value.utf8().data()));
  EXPECT_EQ(4, info.selectionStart);
  EXPECT_EQ(4, info.selectionEnd);
  EXPECT_EQ(8, info.compositionStart);
  EXPECT_EQ(12, info.compositionEnd);

  WebInputMethodController* activeInputMethodController =
      frame->frameWidget()->getActiveWebInputMethodController();
  activeInputMethodController->finishComposingText(
      WebInputMethodController::KeepSelection);
  info = webView->textInputInfo();
  EXPECT_EQ(4, info.selectionStart);
  EXPECT_EQ(4, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);

  std::string compositionText("\n");
  activeInputMethodController->commitText(
      WebString::fromUTF8(compositionText.c_str()), 0);
  info = webView->textInputInfo();
  EXPECT_EQ(5, info.selectionStart);
  EXPECT_EQ(5, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);
  EXPECT_EQ("0123\n456789abcdefghijklmnopqrstuvwxyz",
            std::string(info.value.utf8().data()));
}

TEST_P(WebViewTest, ExtendSelectionAndDelete) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  webView->setInitialFocus(false);
  frame->setEditableSelectionOffsets(10, 10);
  frame->extendSelectionAndDelete(5, 8);
  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ("01234ijklmnopqrstuvwxyz", std::string(info.value.utf8().data()));
  EXPECT_EQ(5, info.selectionStart);
  EXPECT_EQ(5, info.selectionEnd);
  frame->extendSelectionAndDelete(10, 0);
  info = webView->textInputInfo();
  EXPECT_EQ("ijklmnopqrstuvwxyz", std::string(info.value.utf8().data()));
}

TEST_P(WebViewTest, DeleteSurroundingText) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  WebView* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  WebLocalFrameImpl* frame = toWebLocalFrameImpl(webView->mainFrame());
  webView->setInitialFocus(false);

  frame->setEditableSelectionOffsets(10, 10);
  frame->deleteSurroundingText(5, 8);
  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ("01234ijklmnopqrstuvwxyz", std::string(info.value.utf8().data()));
  EXPECT_EQ(5, info.selectionStart);
  EXPECT_EQ(5, info.selectionEnd);

  frame->setEditableSelectionOffsets(5, 10);
  frame->deleteSurroundingText(3, 5);
  info = webView->textInputInfo();
  EXPECT_EQ("01ijklmstuvwxyz", std::string(info.value.utf8().data()));
  EXPECT_EQ(2, info.selectionStart);
  EXPECT_EQ(7, info.selectionEnd);

  frame->setEditableSelectionOffsets(5, 5);
  frame->deleteSurroundingText(10, 0);
  info = webView->textInputInfo();
  EXPECT_EQ("lmstuvwxyz", std::string(info.value.utf8().data()));
  EXPECT_EQ(0, info.selectionStart);
  EXPECT_EQ(0, info.selectionEnd);

  frame->deleteSurroundingText(0, 20);
  info = webView->textInputInfo();
  EXPECT_EQ("", std::string(info.value.utf8().data()));
  EXPECT_EQ(0, info.selectionStart);
  EXPECT_EQ(0, info.selectionEnd);

  frame->deleteSurroundingText(10, 10);
  info = webView->textInputInfo();
  EXPECT_EQ("", std::string(info.value.utf8().data()));
  EXPECT_EQ(0, info.selectionStart);
  EXPECT_EQ(0, info.selectionEnd);
}

TEST_P(WebViewTest, SetCompositionFromExistingText) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  webView->setInitialFocus(false);
  WebVector<WebCompositionUnderline> underlines(static_cast<size_t>(1));
  underlines[0] = WebCompositionUnderline(0, 4, 0, false, 0);
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->setEditableSelectionOffsets(4, 10);
  frame->setCompositionFromExistingText(8, 12, underlines);
  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ(4, info.selectionStart);
  EXPECT_EQ(10, info.selectionEnd);
  EXPECT_EQ(8, info.compositionStart);
  EXPECT_EQ(12, info.compositionEnd);
  WebVector<WebCompositionUnderline> emptyUnderlines;
  frame->setCompositionFromExistingText(0, 0, emptyUnderlines);
  info = webView->textInputInfo();
  EXPECT_EQ(4, info.selectionStart);
  EXPECT_EQ(10, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);
}

TEST_P(WebViewTest, SetCompositionFromExistingTextInTextArea) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("text_area_populated.html"));
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "text_area_populated.html");
  webView->setInitialFocus(false);
  WebVector<WebCompositionUnderline> underlines(static_cast<size_t>(1));
  underlines[0] = WebCompositionUnderline(0, 4, 0, false, 0);
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  WebInputMethodController* activeInputMethodController =
      frame->frameWidget()->getActiveWebInputMethodController();
  frame->setEditableSelectionOffsets(27, 27);
  std::string newLineText("\n");
  activeInputMethodController->commitText(
      WebString::fromUTF8(newLineText.c_str()), 0);
  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopq\nrstuvwxyz",
            std::string(info.value.utf8().data()));

  frame->setEditableSelectionOffsets(31, 31);
  frame->setCompositionFromExistingText(30, 34, underlines);
  info = webView->textInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopq\nrstuvwxyz",
            std::string(info.value.utf8().data()));
  EXPECT_EQ(31, info.selectionStart);
  EXPECT_EQ(31, info.selectionEnd);
  EXPECT_EQ(30, info.compositionStart);
  EXPECT_EQ(34, info.compositionEnd);

  std::string compositionText("yolo");
  activeInputMethodController->commitText(
      WebString::fromUTF8(compositionText.c_str()), 0);
  info = webView->textInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopq\nrsyoloxyz",
            std::string(info.value.utf8().data()));
  EXPECT_EQ(34, info.selectionStart);
  EXPECT_EQ(34, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);
}

TEST_P(WebViewTest, SetCompositionFromExistingTextInRichText) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("content_editable_rich_text.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "content_editable_rich_text.html");
  webView->setInitialFocus(false);
  WebVector<WebCompositionUnderline> underlines(static_cast<size_t>(1));
  underlines[0] = WebCompositionUnderline(0, 4, 0, false, 0);
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->setEditableSelectionOffsets(1, 1);
  WebDocument document = webView->mainFrame()->document();
  EXPECT_FALSE(document.getElementById("bold").isNull());
  frame->setCompositionFromExistingText(0, 4, underlines);
  EXPECT_FALSE(document.getElementById("bold").isNull());
}

TEST_P(WebViewTest, SetEditableSelectionOffsetsKeepsComposition) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  webView->setInitialFocus(false);

  std::string compositionTextFirst("hello ");
  std::string compositionTextSecond("world");
  WebVector<WebCompositionUnderline> emptyUnderlines;
  WebInputMethodController* activeInputMethodController =
      webView->mainFrameImpl()
          ->frameWidget()
          ->getActiveWebInputMethodController();
  activeInputMethodController->commitText(
      WebString::fromUTF8(compositionTextFirst.c_str()), 0);
  activeInputMethodController->setComposition(
      WebString::fromUTF8(compositionTextSecond.c_str()), emptyUnderlines, 5,
      5);

  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ("hello world", std::string(info.value.utf8().data()));
  EXPECT_EQ(11, info.selectionStart);
  EXPECT_EQ(11, info.selectionEnd);
  EXPECT_EQ(6, info.compositionStart);
  EXPECT_EQ(11, info.compositionEnd);

  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->setEditableSelectionOffsets(6, 6);
  info = webView->textInputInfo();
  EXPECT_EQ("hello world", std::string(info.value.utf8().data()));
  EXPECT_EQ(6, info.selectionStart);
  EXPECT_EQ(6, info.selectionEnd);
  EXPECT_EQ(6, info.compositionStart);
  EXPECT_EQ(11, info.compositionEnd);

  frame->setEditableSelectionOffsets(8, 8);
  info = webView->textInputInfo();
  EXPECT_EQ("hello world", std::string(info.value.utf8().data()));
  EXPECT_EQ(8, info.selectionStart);
  EXPECT_EQ(8, info.selectionEnd);
  EXPECT_EQ(6, info.compositionStart);
  EXPECT_EQ(11, info.compositionEnd);

  frame->setEditableSelectionOffsets(11, 11);
  info = webView->textInputInfo();
  EXPECT_EQ("hello world", std::string(info.value.utf8().data()));
  EXPECT_EQ(11, info.selectionStart);
  EXPECT_EQ(11, info.selectionEnd);
  EXPECT_EQ(6, info.compositionStart);
  EXPECT_EQ(11, info.compositionEnd);

  frame->setEditableSelectionOffsets(6, 11);
  info = webView->textInputInfo();
  EXPECT_EQ("hello world", std::string(info.value.utf8().data()));
  EXPECT_EQ(6, info.selectionStart);
  EXPECT_EQ(11, info.selectionEnd);
  EXPECT_EQ(6, info.compositionStart);
  EXPECT_EQ(11, info.compositionEnd);

  frame->setEditableSelectionOffsets(2, 2);
  info = webView->textInputInfo();
  EXPECT_EQ("hello world", std::string(info.value.utf8().data()));
  EXPECT_EQ(2, info.selectionStart);
  EXPECT_EQ(2, info.selectionEnd);
  EXPECT_EQ(-1, info.compositionStart);
  EXPECT_EQ(-1, info.compositionEnd);
}

TEST_P(WebViewTest, IsSelectionAnchorFirst) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  WebLocalFrame* frame = webView->mainFrameImpl();

  webView->setInitialFocus(false);
  frame->setEditableSelectionOffsets(4, 10);
  EXPECT_TRUE(webView->isSelectionAnchorFirst());
  WebRect anchor;
  WebRect focus;
  webView->selectionBounds(anchor, focus);
  frame->selectRange(WebPoint(focus.x, focus.y), WebPoint(anchor.x, anchor.y));
  EXPECT_FALSE(webView->isSelectionAnchorFirst());
}

TEST_P(WebViewTest, ExitingDeviceEmulationResetsPageScale) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("200-by-300.html"));
  WebViewImpl* webViewImpl =
      m_webViewHelper.initializeAndLoad(m_baseURL + "200-by-300.html");
  webViewImpl->resize(WebSize(200, 300));

  float pageScaleExpected = webViewImpl->pageScaleFactor();

  WebDeviceEmulationParams params;
  params.screenPosition = WebDeviceEmulationParams::Desktop;
  params.deviceScaleFactor = 0;
  params.fitToView = false;
  params.offset = WebFloatPoint();
  params.scale = 1;

  webViewImpl->enableDeviceEmulation(params);

  webViewImpl->setPageScaleFactor(2);

  webViewImpl->disableDeviceEmulation();

  EXPECT_EQ(pageScaleExpected, webViewImpl->pageScaleFactor());
}

TEST_P(WebViewTest, HistoryResetScrollAndScaleState) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("200-by-300.html"));
  WebViewImpl* webViewImpl =
      m_webViewHelper.initializeAndLoad(m_baseURL + "200-by-300.html");
  webViewImpl->resize(WebSize(100, 150));
  webViewImpl->updateAllLifecyclePhases();
  EXPECT_EQ(0, webViewImpl->mainFrame()->scrollOffset().width);
  EXPECT_EQ(0, webViewImpl->mainFrame()->scrollOffset().height);

  // Make the page scale and scroll with the given paremeters.
  webViewImpl->setPageScaleFactor(2.0f);
  webViewImpl->mainFrame()->setScrollOffset(WebSize(94, 111));
  EXPECT_EQ(2.0f, webViewImpl->pageScaleFactor());
  EXPECT_EQ(94, webViewImpl->mainFrame()->scrollOffset().width);
  EXPECT_EQ(111, webViewImpl->mainFrame()->scrollOffset().height);
  LocalFrame* mainFrameLocal = toLocalFrame(webViewImpl->page()->mainFrame());
  mainFrameLocal->loader().saveScrollState();
  EXPECT_EQ(2.0f, mainFrameLocal->loader().currentItem()->pageScaleFactor());
  EXPECT_EQ(94, mainFrameLocal->loader().currentItem()->scrollOffset().width());
  EXPECT_EQ(111,
            mainFrameLocal->loader().currentItem()->scrollOffset().height());

  // Confirm that resetting the page state resets the saved scroll position.
  webViewImpl->resetScrollAndScaleState();
  EXPECT_EQ(1.0f, webViewImpl->pageScaleFactor());
  EXPECT_EQ(0, webViewImpl->mainFrame()->scrollOffset().width);
  EXPECT_EQ(0, webViewImpl->mainFrame()->scrollOffset().height);
  EXPECT_EQ(1.0f, mainFrameLocal->loader().currentItem()->pageScaleFactor());
  EXPECT_EQ(0, mainFrameLocal->loader().currentItem()->scrollOffset().width());
  EXPECT_EQ(0, mainFrameLocal->loader().currentItem()->scrollOffset().height());
}

TEST_P(WebViewTest, BackForwardRestoreScroll) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("back_forward_restore_scroll.html"));
  WebViewImpl* webViewImpl = m_webViewHelper.initializeAndLoad(
      m_baseURL + "back_forward_restore_scroll.html");
  webViewImpl->resize(WebSize(640, 480));
  webViewImpl->updateAllLifecyclePhases();

  // Emulate a user scroll
  webViewImpl->mainFrame()->setScrollOffset(WebSize(0, 900));
  LocalFrame* mainFrameLocal = toLocalFrame(webViewImpl->page()->mainFrame());
  Persistent<HistoryItem> item1 = mainFrameLocal->loader().currentItem();

  // Click an anchor
  mainFrameLocal->loader().load(FrameLoadRequest(
      mainFrameLocal->document(),
      ResourceRequest(mainFrameLocal->document()->completeURL("#a"))));
  Persistent<HistoryItem> item2 = mainFrameLocal->loader().currentItem();

  // Go back, then forward, then back again.
  mainFrameLocal->loader().load(
      FrameLoadRequest(
          nullptr, FrameLoader::resourceRequestFromHistoryItem(
                       item1.get(), WebCachePolicy::UseProtocolCachePolicy)),
      FrameLoadTypeBackForward, item1.get(), HistorySameDocumentLoad);
  mainFrameLocal->loader().load(
      FrameLoadRequest(
          nullptr, FrameLoader::resourceRequestFromHistoryItem(
                       item2.get(), WebCachePolicy::UseProtocolCachePolicy)),
      FrameLoadTypeBackForward, item2.get(), HistorySameDocumentLoad);
  mainFrameLocal->loader().load(
      FrameLoadRequest(
          nullptr, FrameLoader::resourceRequestFromHistoryItem(
                       item1.get(), WebCachePolicy::UseProtocolCachePolicy)),
      FrameLoadTypeBackForward, item1.get(), HistorySameDocumentLoad);

  // Click a different anchor
  mainFrameLocal->loader().load(FrameLoadRequest(
      mainFrameLocal->document(),
      ResourceRequest(mainFrameLocal->document()->completeURL("#b"))));
  Persistent<HistoryItem> item3 = mainFrameLocal->loader().currentItem();

  // Go back, then forward. The scroll position should be properly set on the
  // forward navigation.
  mainFrameLocal->loader().load(
      FrameLoadRequest(
          nullptr, FrameLoader::resourceRequestFromHistoryItem(
                       item1.get(), WebCachePolicy::UseProtocolCachePolicy)),
      FrameLoadTypeBackForward, item1.get(), HistorySameDocumentLoad);
  mainFrameLocal->loader().load(
      FrameLoadRequest(
          nullptr, FrameLoader::resourceRequestFromHistoryItem(
                       item3.get(), WebCachePolicy::UseProtocolCachePolicy)),
      FrameLoadTypeBackForward, item3.get(), HistorySameDocumentLoad);
  EXPECT_EQ(0, webViewImpl->mainFrame()->scrollOffset().width);
  EXPECT_GT(webViewImpl->mainFrame()->scrollOffset().height, 2000);
}

// Tests that we restore scroll and scale *after* the fullscreen styles are
// removed and the page is laid out. http://crbug.com/625683.
TEST_P(WebViewTest, FullscreenResetScrollAndScaleFullscreenStyles) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("fullscreen_style.html"));
  WebViewImpl* webViewImpl =
      m_webViewHelper.initializeAndLoad(m_baseURL + "fullscreen_style.html");
  webViewImpl->resize(WebSize(800, 600));
  webViewImpl->updateAllLifecyclePhases();

  // Scroll the page down.
  webViewImpl->mainFrame()->setScrollOffset(WebSize(0, 2000));
  ASSERT_EQ(2000, webViewImpl->mainFrame()->scrollOffset().height);

  // Enter fullscreen.
  Element* element = static_cast<Element*>(
      webViewImpl->mainFrame()->document().getElementById("fullscreenElement"));
  webViewImpl->enterFullscreenForElement(element);
  webViewImpl->didEnterFullscreen();
  webViewImpl->updateAllLifecyclePhases();

  // Sanity-check. There should be no scrolling possible.
  ASSERT_EQ(0, webViewImpl->mainFrame()->scrollOffset().height);
  ASSERT_EQ(0, webViewImpl->mainFrameImpl()
                   ->frameView()
                   ->maximumScrollOffset()
                   .height());

  // Confirm that after exiting and doing a layout, the scroll and scale
  // parameters are reset. The page sets display: none on overflowing elements
  // while in fullscreen so if we try to restore before the style and layout
  // is applied the offsets will be clamped.
  webViewImpl->didExitFullscreen();
  EXPECT_TRUE(webViewImpl->mainFrameImpl()->frameView()->needsLayout());
  webViewImpl->updateAllLifecyclePhases();

  EXPECT_EQ(2000, webViewImpl->mainFrame()->scrollOffset().height);
}

// Tests that exiting and immediately reentering fullscreen doesn't cause the
// scroll and scale restoration to occur when we enter fullscreen again.
TEST_P(WebViewTest, FullscreenResetScrollAndScaleExitAndReenter) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("fullscreen_style.html"));
  WebViewImpl* webViewImpl =
      m_webViewHelper.initializeAndLoad(m_baseURL + "fullscreen_style.html");
  webViewImpl->resize(WebSize(800, 600));
  webViewImpl->updateAllLifecyclePhases();

  // Scroll the page down.
  webViewImpl->mainFrame()->setScrollOffset(WebSize(0, 2000));
  ASSERT_EQ(2000, webViewImpl->mainFrame()->scrollOffset().height);

  // Enter fullscreen.
  Element* element = static_cast<Element*>(
      webViewImpl->mainFrame()->document().getElementById("fullscreenElement"));
  webViewImpl->enterFullscreenForElement(element);
  webViewImpl->didEnterFullscreen();
  webViewImpl->updateAllLifecyclePhases();

  // Sanity-check. There should be no scrolling possible.
  ASSERT_EQ(0, webViewImpl->mainFrame()->scrollOffset().height);
  ASSERT_EQ(0, webViewImpl->mainFrameImpl()
                   ->frameView()
                   ->maximumScrollOffset()
                   .height());

  // Exit and, without performing a layout, reenter fullscreen again. We
  // shouldn't try to restore the scroll and scale values when we layout to
  // enter fullscreen.
  webViewImpl->exitFullscreen(element->document().frame());
  webViewImpl->didExitFullscreen();
  webViewImpl->enterFullscreenForElement(element);
  webViewImpl->didEnterFullscreen();
  webViewImpl->updateAllLifecyclePhases();

  // Sanity-check. There should be no scrolling possible.
  ASSERT_EQ(0, webViewImpl->mainFrame()->scrollOffset().height);
  ASSERT_EQ(0, webViewImpl->mainFrameImpl()
                   ->frameView()
                   ->maximumScrollOffset()
                   .height());

  // When we exit now, we should restore the original scroll value.
  webViewImpl->exitFullscreen(element->document().frame());
  webViewImpl->didExitFullscreen();
  webViewImpl->updateAllLifecyclePhases();

  EXPECT_EQ(2000, webViewImpl->mainFrame()->scrollOffset().height);
}

TEST_P(WebViewTest, EnterFullscreenResetScrollAndScaleState) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("200-by-300.html"));
  WebViewImpl* webViewImpl =
      m_webViewHelper.initializeAndLoad(m_baseURL + "200-by-300.html");
  webViewImpl->resize(WebSize(100, 150));
  webViewImpl->updateAllLifecyclePhases();
  EXPECT_EQ(0, webViewImpl->mainFrame()->scrollOffset().width);
  EXPECT_EQ(0, webViewImpl->mainFrame()->scrollOffset().height);

  // Make the page scale and scroll with the given paremeters.
  webViewImpl->setPageScaleFactor(2.0f);
  webViewImpl->mainFrame()->setScrollOffset(WebSize(94, 111));
  webViewImpl->setVisualViewportOffset(WebFloatPoint(12, 20));
  EXPECT_EQ(2.0f, webViewImpl->pageScaleFactor());
  EXPECT_EQ(94, webViewImpl->mainFrame()->scrollOffset().width);
  EXPECT_EQ(111, webViewImpl->mainFrame()->scrollOffset().height);
  EXPECT_EQ(12, webViewImpl->visualViewportOffset().x);
  EXPECT_EQ(20, webViewImpl->visualViewportOffset().y);

  Element* element =
      static_cast<Element*>(webViewImpl->mainFrame()->document().body());
  webViewImpl->enterFullscreenForElement(element);
  webViewImpl->didEnterFullscreen();

  // Page scale factor must be 1.0 during fullscreen for elements to be sized
  // properly.
  EXPECT_EQ(1.0f, webViewImpl->pageScaleFactor());

  // Make sure fullscreen nesting doesn't disrupt scroll/scale saving.
  Element* otherElement =
      static_cast<Element*>(webViewImpl->mainFrame()->document().head());
  webViewImpl->enterFullscreenForElement(otherElement);

  // Confirm that exiting fullscreen restores the parameters.
  webViewImpl->exitFullscreen(element->document().frame());
  webViewImpl->didExitFullscreen();
  webViewImpl->updateAllLifecyclePhases();

  EXPECT_EQ(2.0f, webViewImpl->pageScaleFactor());
  EXPECT_EQ(94, webViewImpl->mainFrame()->scrollOffset().width);
  EXPECT_EQ(111, webViewImpl->mainFrame()->scrollOffset().height);
  EXPECT_EQ(12, webViewImpl->visualViewportOffset().x);
  EXPECT_EQ(20, webViewImpl->visualViewportOffset().y);
}

class PrintWebViewClient : public FrameTestHelpers::TestWebViewClient {
 public:
  PrintWebViewClient() : m_printCalled(false) {}

  // WebViewClient methods
  void printPage(WebLocalFrame*) override { m_printCalled = true; }

  bool printCalled() const { return m_printCalled; }

 private:
  bool m_printCalled;
};

TEST_P(WebViewTest, PrintWithXHRInFlight) {
  PrintWebViewClient client;
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("print_with_xhr_inflight.html"));
  WebViewImpl* webViewImpl = m_webViewHelper.initializeAndLoad(
      m_baseURL + "print_with_xhr_inflight.html", true, 0, &client);

  ASSERT_TRUE(toLocalFrame(webViewImpl->page()->mainFrame())
                  ->document()
                  ->loadEventFinished());
  EXPECT_TRUE(client.printCalled());
  m_webViewHelper.reset();
}

static void DragAndDropURL(WebViewImpl* webView, const std::string& url) {
  WebDragData dragData;
  dragData.initialize();

  WebDragData::Item item;
  item.storageType = WebDragData::Item::StorageTypeString;
  item.stringType = "text/uri-list";
  item.stringData = WebString::fromUTF8(url);
  dragData.addItem(item);

  const WebPoint clientPoint(0, 0);
  const WebPoint screenPoint(0, 0);
  WebFrameWidgetBase* widget = webView->mainFrameImpl()->frameWidget();
  widget->dragTargetDragEnter(dragData, clientPoint, screenPoint,
                              WebDragOperationCopy, 0);
  widget->dragTargetDrop(dragData, clientPoint, screenPoint, 0);
  FrameTestHelpers::pumpPendingRequestsForFrameToLoad(webView->mainFrame());
}

TEST_P(WebViewTest, DragDropURL) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), "foo.html");
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), "bar.html");

  const std::string fooUrl = m_baseURL + "foo.html";
  const std::string barUrl = m_baseURL + "bar.html";

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(fooUrl);

  ASSERT_TRUE(webView);

  // Drag and drop barUrl and verify that we've navigated to it.
  DragAndDropURL(webView, barUrl);
  EXPECT_EQ(barUrl, webView->mainFrame()->document().url().string().utf8());

  // Drag and drop fooUrl and verify that we've navigated back to it.
  DragAndDropURL(webView, fooUrl);
  EXPECT_EQ(fooUrl, webView->mainFrame()->document().url().string().utf8());

  // Disable navigation on drag-and-drop.
  webView->settingsImpl()->setNavigateOnDragDrop(false);

  // Attempt to drag and drop to barUrl and verify that no navigation has
  // occurred.
  DragAndDropURL(webView, barUrl);
  EXPECT_EQ(fooUrl, webView->mainFrame()->document().url().string().utf8());
}

class ContentDetectorClient : public FrameTestHelpers::TestWebViewClient {
 public:
  ContentDetectorClient() { reset(); }

  WebURL detectContentIntentAt(const WebHitTestResult& hitTest) override {
    m_contentDetectionRequested = true;
    return m_contentDetectionResult;
  }

  void scheduleContentIntent(const WebURL& url, bool isMainFrame) override {
    m_scheduledIntentURL = url;
    m_wasInMainFrame = isMainFrame;
  }

  void cancelScheduledContentIntents() override {
    m_pendingIntentsCancelled = true;
  }

  void reset() {
    m_contentDetectionRequested = false;
    m_pendingIntentsCancelled = false;
    m_scheduledIntentURL = WebURL();
    m_wasInMainFrame = false;
    m_contentDetectionResult = WebURL();
  }

  bool contentDetectionRequested() const { return m_contentDetectionRequested; }
  bool pendingIntentsCancelled() const { return m_pendingIntentsCancelled; }
  const WebURL& scheduledIntentURL() const { return m_scheduledIntentURL; }
  bool wasInMainFrame() const { return m_wasInMainFrame; }
  void setContentDetectionResult(const WebURL& result) {
    m_contentDetectionResult = result;
  }

 private:
  bool m_contentDetectionRequested;
  bool m_pendingIntentsCancelled;
  WebURL m_scheduledIntentURL;
  bool m_wasInMainFrame;
  WebURL m_contentDetectionResult;
};

bool WebViewTest::tapElement(WebInputEvent::Type type, Element* element) {
  if (!element || !element->layoutObject())
    return false;

  DCHECK(m_webViewHelper.webView());
  element->scrollIntoViewIfNeeded();

  // TODO(bokan): Technically incorrect, event positions should be in viewport
  // space. crbug.com/371902.
  IntPoint center =
      m_webViewHelper.webView()
          ->mainFrameImpl()
          ->frameView()
          ->contentsToScreen(element->layoutObject()->absoluteBoundingBoxRect())
          .center();

  WebGestureEvent event;
  event.type = type;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = center.x();
  event.y = center.y();

  m_webViewHelper.webView()->handleInputEvent(event);
  runPendingTasks();
  return true;
}

bool WebViewTest::tapElementById(WebInputEvent::Type type,
                                 const WebString& id) {
  DCHECK(m_webViewHelper.webView());
  Element* element = static_cast<Element*>(
      m_webViewHelper.webView()->mainFrame()->document().getElementById(id));
  return tapElement(type, element);
}

TEST_P(WebViewTest, DetectContentAroundPosition) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("content_listeners.html"));

  ContentDetectorClient client;
  WebView* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "content_listeners.html", true, 0, &client);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebString clickListener = WebString::fromUTF8("clickListener");
  WebString touchstartListener = WebString::fromUTF8("touchstartListener");
  WebString mousedownListener = WebString::fromUTF8("mousedownListener");
  WebString noListener = WebString::fromUTF8("noListener");
  WebString link = WebString::fromUTF8("link");

  // Ensure content detection is not requested for nodes listening to click,
  // mouse or touch events when we do simple taps.
  EXPECT_TRUE(tapElementById(WebInputEvent::GestureTap, clickListener));
  EXPECT_FALSE(client.contentDetectionRequested());
  client.reset();

  EXPECT_TRUE(tapElementById(WebInputEvent::GestureTap, touchstartListener));
  EXPECT_FALSE(client.contentDetectionRequested());
  client.reset();

  EXPECT_TRUE(tapElementById(WebInputEvent::GestureTap, mousedownListener));
  EXPECT_FALSE(client.contentDetectionRequested());
  client.reset();

  // Content detection should work normally without these event listeners.
  // The click listener in the body should be ignored as a special case.
  EXPECT_TRUE(tapElementById(WebInputEvent::GestureTap, noListener));
  EXPECT_TRUE(client.contentDetectionRequested());
  EXPECT_FALSE(client.scheduledIntentURL().isValid());

  WebURL intentURL = toKURL(m_baseURL);
  client.setContentDetectionResult(intentURL);
  EXPECT_TRUE(tapElementById(WebInputEvent::GestureTap, noListener));
  EXPECT_TRUE(client.scheduledIntentURL() == intentURL);
  EXPECT_TRUE(client.wasInMainFrame());

  // Tapping elsewhere should cancel the scheduled intent.
  WebGestureEvent event;
  event.type = WebInputEvent::GestureTap;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  webView->handleInputEvent(event);
  runPendingTasks();
  EXPECT_TRUE(client.pendingIntentsCancelled());

  // Explicitly reset to break dependency on locally scoped client.
  m_webViewHelper.reset();
}

TEST_P(WebViewTest, ContentDetectionInIframe) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("content_listeners_iframe.html"));

  ContentDetectorClient client;
  WebView* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "content_listeners_iframe.html", true, 0, &client);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebString noListener = WebString::fromUTF8("noListener");
  WebString frameName = WebString::fromUTF8("innerFrame");

  WebURL intentURL = toKURL(m_baseURL);
  client.setContentDetectionResult(intentURL);
  Element* element = static_cast<Element*>(
      webView->findFrameByName(frameName)->document().getElementById(
          noListener));
  EXPECT_TRUE(tapElement(WebInputEvent::GestureTap, element));
  EXPECT_TRUE(client.scheduledIntentURL() == intentURL);
  EXPECT_FALSE(client.wasInMainFrame());

  // Explicitly reset to break dependency on locally scoped client.
  m_webViewHelper.reset();
}

TEST_P(WebViewTest, ClientTapHandling) {
  TapHandlingWebViewClient client;
  client.reset();
  WebView* webView =
      m_webViewHelper.initializeAndLoad("about:blank", true, 0, &client);
  WebGestureEvent event;
  event.type = WebInputEvent::GestureTap;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = 3;
  event.y = 8;
  webView->handleInputEvent(event);
  runPendingTasks();
  EXPECT_EQ(3, client.tapX());
  EXPECT_EQ(8, client.tapY());
  client.reset();
  event.type = WebInputEvent::GestureLongPress;
  event.x = 25;
  event.y = 7;
  webView->handleInputEvent(event);
  runPendingTasks();
  EXPECT_EQ(25, client.longpressX());
  EXPECT_EQ(7, client.longpressY());

  // Explicitly reset to break dependency on locally scoped client.
  m_webViewHelper.reset();
}

TEST_P(WebViewTest, ClientTapHandlingNullWebViewClient) {
  WebViewImpl* webView =
      WebViewImpl::create(nullptr, WebPageVisibilityStateVisible);
  FrameTestHelpers::TestWebFrameClient webFrameClient;
  FrameTestHelpers::TestWebWidgetClient webWidgetClient;
  WebLocalFrame* localFrame =
      WebLocalFrame::create(WebTreeScopeType::Document, &webFrameClient);
  webView->setMainFrame(localFrame);

  // TODO(dcheng): The main frame widget currently has a special case.
  // Eliminate this once WebView is no longer a WebWidget.
  blink::WebFrameWidget::create(&webWidgetClient, webView, localFrame);

  WebGestureEvent event;
  event.type = WebInputEvent::GestureTap;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = 3;
  event.y = 8;
  EXPECT_EQ(WebInputEventResult::NotHandled, webView->handleInputEvent(event));
  webView->close();
}

TEST_P(WebViewTest, LongPressEmptyDiv) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("long_press_empty_div.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "long_press_empty_div.html", true);
  webView->settingsImpl()->setAlwaysShowContextMenuOnTouch(false);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebGestureEvent event;
  event.type = WebInputEvent::GestureLongPress;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = 250;
  event.y = 150;

  EXPECT_EQ(WebInputEventResult::NotHandled, webView->handleInputEvent(event));
}

TEST_P(WebViewTest, LongPressEmptyDivAlwaysShow) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("long_press_empty_div.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "long_press_empty_div.html", true);
  webView->settingsImpl()->setAlwaysShowContextMenuOnTouch(true);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebGestureEvent event;
  event.type = WebInputEvent::GestureLongPress;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = 250;
  event.y = 150;

  EXPECT_EQ(WebInputEventResult::HandledSystem,
            webView->handleInputEvent(event));
}

TEST_P(WebViewTest, LongPressObject) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("long_press_object.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "long_press_object.html", true);
  webView->settingsImpl()->setAlwaysShowContextMenuOnTouch(true);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebGestureEvent event;
  event.type = WebInputEvent::GestureLongPress;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = 10;
  event.y = 10;

  EXPECT_NE(WebInputEventResult::HandledSystem,
            webView->handleInputEvent(event));

  HTMLElement* element =
      toHTMLElement(webView->mainFrame()->document().getElementById("obj"));
  EXPECT_FALSE(element->canStartSelection());
}

TEST_P(WebViewTest, LongPressObjectFallback) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("long_press_object_fallback.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "long_press_object_fallback.html", true);
  webView->settingsImpl()->setAlwaysShowContextMenuOnTouch(true);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebGestureEvent event;
  event.type = WebInputEvent::GestureLongPress;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = 10;
  event.y = 10;

  EXPECT_EQ(WebInputEventResult::HandledSystem,
            webView->handleInputEvent(event));

  HTMLElement* element =
      toHTMLElement(webView->mainFrame()->document().getElementById("obj"));
  EXPECT_TRUE(element->canStartSelection());
}

TEST_P(WebViewTest, LongPressImage) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("long_press_image.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "long_press_image.html", true);
  webView->settingsImpl()->setAlwaysShowContextMenuOnTouch(false);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebGestureEvent event;
  event.type = WebInputEvent::GestureLongPress;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = 10;
  event.y = 10;

  EXPECT_EQ(WebInputEventResult::HandledSystem,
            webView->handleInputEvent(event));
}

TEST_P(WebViewTest, LongPressVideo) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("long_press_video.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "long_press_video.html", true);
  webView->settingsImpl()->setAlwaysShowContextMenuOnTouch(false);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebGestureEvent event;
  event.type = WebInputEvent::GestureLongPress;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = 10;
  event.y = 10;

  EXPECT_EQ(WebInputEventResult::HandledSystem,
            webView->handleInputEvent(event));
}

TEST_P(WebViewTest, LongPressLink) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("long_press_link.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "long_press_link.html", true);
  webView->settingsImpl()->setAlwaysShowContextMenuOnTouch(false);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebGestureEvent event;
  event.type = WebInputEvent::GestureLongPress;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = 500;
  event.y = 300;

  EXPECT_EQ(WebInputEventResult::HandledSystem,
            webView->handleInputEvent(event));
}

TEST_P(WebViewTest, showContextMenuOnLongPressingLinks) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("long_press_links_and_images.html"));

  URLTestHelpers::registerMockedURLLoad(toKURL("http://www.test.com/foo.png"),
                                        "white-1x1.png");
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "long_press_links_and_images.html", true);

  webView->settingsImpl()->setTouchDragDropEnabled(true);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebString anchorTagId = WebString::fromUTF8("anchorTag");
  WebString imageTagId = WebString::fromUTF8("imageTag");

  EXPECT_TRUE(tapElementById(WebInputEvent::GestureLongPress, anchorTagId));
  EXPECT_STREQ("anchor contextmenu",
               webView->mainFrame()->document().title().utf8().data());

  EXPECT_TRUE(tapElementById(WebInputEvent::GestureLongPress, imageTagId));
  EXPECT_STREQ("image contextmenu",
               webView->mainFrame()->document().title().utf8().data());
}

TEST_P(WebViewTest, LongPressEmptyEditableSelection) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("long_press_empty_editable_selection.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "long_press_empty_editable_selection.html", true);
  webView->settingsImpl()->setAlwaysShowContextMenuOnTouch(false);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebGestureEvent event;
  event.type = WebInputEvent::GestureLongPress;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = 10;
  event.y = 10;

  EXPECT_EQ(WebInputEventResult::HandledSystem,
            webView->handleInputEvent(event));
}

TEST_P(WebViewTest, LongPressEmptyNonEditableSelection) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("long_press_image.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "long_press_image.html", true);
  webView->resize(WebSize(500, 500));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebGestureEvent event;
  event.type = WebInputEvent::GestureLongPress;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = 300;
  event.y = 300;
  WebLocalFrameImpl* frame = webView->mainFrameImpl();

  EXPECT_EQ(WebInputEventResult::HandledSystem,
            webView->handleInputEvent(event));
  EXPECT_TRUE(frame->selectionAsText().isEmpty());
}

TEST_P(WebViewTest, LongPressSelection) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("longpress_selection.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "longpress_selection.html", true);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebString target = WebString::fromUTF8("target");
  WebString onselectstartfalse = WebString::fromUTF8("onselectstartfalse");
  WebLocalFrameImpl* frame = webView->mainFrameImpl();

  EXPECT_TRUE(
      tapElementById(WebInputEvent::GestureLongPress, onselectstartfalse));
  EXPECT_EQ("", std::string(frame->selectionAsText().utf8().data()));
  EXPECT_TRUE(tapElementById(WebInputEvent::GestureLongPress, target));
  EXPECT_EQ("testword", std::string(frame->selectionAsText().utf8().data()));
}

#if !OS(MACOSX)
TEST_P(WebViewTest, TouchDoesntSelectEmptyTextarea) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("longpress_textarea.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "longpress_textarea.html", true);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebString blanklinestextbox = WebString::fromUTF8("blanklinestextbox");
  WebLocalFrameImpl* frame = webView->mainFrameImpl();

  // Long-press on carriage returns.
  EXPECT_TRUE(
      tapElementById(WebInputEvent::GestureLongPress, blanklinestextbox));
  EXPECT_TRUE(frame->selectionAsText().isEmpty());

  // Double-tap on carriage returns.
  WebGestureEvent event;
  event.type = WebInputEvent::GestureTap;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = 100;
  event.y = 25;
  event.data.tap.tapCount = 2;

  webView->handleInputEvent(event);
  EXPECT_TRUE(frame->selectionAsText().isEmpty());

  HTMLTextAreaElement* textAreaElement = toHTMLTextAreaElement(
      webView->mainFrame()->document().getElementById(blanklinestextbox));
  textAreaElement->setValue("hello");

  // Long-press past last word of textbox.
  EXPECT_TRUE(
      tapElementById(WebInputEvent::GestureLongPress, blanklinestextbox));
  EXPECT_TRUE(frame->selectionAsText().isEmpty());

  // Double-tap past last word of textbox.
  webView->handleInputEvent(event);
  EXPECT_TRUE(frame->selectionAsText().isEmpty());
}
#endif

TEST_P(WebViewTest, LongPressImageTextarea) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("longpress_image_contenteditable.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "longpress_image_contenteditable.html", true);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebString image = WebString::fromUTF8("purpleimage");

  EXPECT_TRUE(tapElementById(WebInputEvent::GestureLongPress, image));
  WebRange range = webView->caretOrSelectionRange();
  EXPECT_FALSE(range.isNull());
  EXPECT_EQ(0, range.startOffset());
  EXPECT_EQ(1, range.length());
}

TEST_P(WebViewTest, BlinkCaretAfterLongPress) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("blink_caret_on_typing_after_long_press.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "blink_caret_on_typing_after_long_press.html", true);
  webView->resize(WebSize(640, 480));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebString target = WebString::fromUTF8("target");
  WebLocalFrameImpl* mainFrame = webView->mainFrameImpl();

  EXPECT_TRUE(tapElementById(WebInputEvent::GestureLongPress, target));
  EXPECT_FALSE(mainFrame->frame()->selection().isCaretBlinkingSuspended());
}

TEST_P(WebViewTest, BlinkCaretOnClosingContextMenu) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("form.html"));
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "form.html", true);

  webView->setInitialFocus(false);
  runPendingTasks();

  // We suspend caret blinking when pressing with mouse right button.
  // Note that we do not send MouseUp event here since it will be consumed
  // by the context menu once it shows up.
  WebMouseEvent mouseEvent;
  mouseEvent.button = WebMouseEvent::Button::Right;
  mouseEvent.x = 1;
  mouseEvent.y = 1;
  mouseEvent.clickCount = 1;
  mouseEvent.type = WebInputEvent::MouseDown;
  webView->handleInputEvent(mouseEvent);
  runPendingTasks();

  WebLocalFrameImpl* mainFrame = webView->mainFrameImpl();
  EXPECT_TRUE(mainFrame->frame()->selection().isCaretBlinkingSuspended());

  // Caret blinking is still suspended after showing context menu.
  webView->showContextMenu();
  EXPECT_TRUE(mainFrame->frame()->selection().isCaretBlinkingSuspended());

  // Caret blinking will be resumed only after context menu is closed.
  webView->didCloseContextMenu();

  EXPECT_FALSE(mainFrame->frame()->selection().isCaretBlinkingSuspended());
}

TEST_P(WebViewTest, SelectionOnReadOnlyInput) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("selection_readonly.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "selection_readonly.html", true);
  webView->resize(WebSize(640, 480));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  std::string testWord = "This text should be selected.";

  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  EXPECT_EQ(testWord, std::string(frame->selectionAsText().utf8().data()));

  WebRange range = webView->caretOrSelectionRange();
  EXPECT_FALSE(range.isNull());
  EXPECT_EQ(0, range.startOffset());
  EXPECT_EQ(static_cast<int>(testWord.length()), range.length());
}

TEST_P(WebViewTest, KeyDownScrollsHandled) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("content-width-1000.html"));

  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "content-width-1000.html", true);
  webView->resize(WebSize(100, 100));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  WebKeyboardEvent keyEvent;

  // RawKeyDown pagedown should be handled.
  keyEvent.windowsKeyCode = VKEY_NEXT;
  keyEvent.type = WebInputEvent::RawKeyDown;
  EXPECT_EQ(WebInputEventResult::HandledSystem,
            webView->handleInputEvent(keyEvent));
  keyEvent.type = WebInputEvent::KeyUp;
  webView->handleInputEvent(keyEvent);

  // Coalesced KeyDown arrow-down should be handled.
  keyEvent.windowsKeyCode = VKEY_DOWN;
  keyEvent.type = WebInputEvent::KeyDown;
  EXPECT_EQ(WebInputEventResult::HandledSystem,
            webView->handleInputEvent(keyEvent));
  keyEvent.type = WebInputEvent::KeyUp;
  webView->handleInputEvent(keyEvent);

  // Ctrl-Home should be handled...
  keyEvent.windowsKeyCode = VKEY_HOME;
  keyEvent.modifiers = WebInputEvent::ControlKey;
  keyEvent.type = WebInputEvent::RawKeyDown;
  EXPECT_EQ(WebInputEventResult::NotHandled,
            webView->handleInputEvent(keyEvent));
  keyEvent.type = WebInputEvent::KeyUp;
  webView->handleInputEvent(keyEvent);

  // But Ctrl-Down should not.
  keyEvent.windowsKeyCode = VKEY_DOWN;
  keyEvent.modifiers = WebInputEvent::ControlKey;
  keyEvent.type = WebInputEvent::RawKeyDown;
  EXPECT_EQ(WebInputEventResult::NotHandled,
            webView->handleInputEvent(keyEvent));
  keyEvent.type = WebInputEvent::KeyUp;
  webView->handleInputEvent(keyEvent);

  // Shift, meta, and alt should not be handled.
  keyEvent.windowsKeyCode = VKEY_NEXT;
  keyEvent.modifiers = WebInputEvent::ShiftKey;
  keyEvent.type = WebInputEvent::RawKeyDown;
  EXPECT_EQ(WebInputEventResult::NotHandled,
            webView->handleInputEvent(keyEvent));
  keyEvent.type = WebInputEvent::KeyUp;
  webView->handleInputEvent(keyEvent);

  keyEvent.windowsKeyCode = VKEY_NEXT;
  keyEvent.modifiers = WebInputEvent::MetaKey;
  keyEvent.type = WebInputEvent::RawKeyDown;
  EXPECT_EQ(WebInputEventResult::NotHandled,
            webView->handleInputEvent(keyEvent));
  keyEvent.type = WebInputEvent::KeyUp;
  webView->handleInputEvent(keyEvent);

  keyEvent.windowsKeyCode = VKEY_NEXT;
  keyEvent.modifiers = WebInputEvent::AltKey;
  keyEvent.type = WebInputEvent::RawKeyDown;
  EXPECT_EQ(WebInputEventResult::NotHandled,
            webView->handleInputEvent(keyEvent));
  keyEvent.type = WebInputEvent::KeyUp;
  webView->handleInputEvent(keyEvent);

  // System-key labeled Alt-Down (as in Windows) should do nothing,
  // but non-system-key labeled Alt-Down (as in Mac) should be handled
  // as a page-down.
  keyEvent.windowsKeyCode = VKEY_DOWN;
  keyEvent.modifiers = WebInputEvent::AltKey;
  keyEvent.isSystemKey = true;
  keyEvent.type = WebInputEvent::RawKeyDown;
  EXPECT_EQ(WebInputEventResult::NotHandled,
            webView->handleInputEvent(keyEvent));
  keyEvent.type = WebInputEvent::KeyUp;
  webView->handleInputEvent(keyEvent);

  keyEvent.windowsKeyCode = VKEY_DOWN;
  keyEvent.modifiers = WebInputEvent::AltKey;
  keyEvent.isSystemKey = false;
  keyEvent.type = WebInputEvent::RawKeyDown;
  EXPECT_EQ(WebInputEventResult::HandledSystem,
            webView->handleInputEvent(keyEvent));
  keyEvent.type = WebInputEvent::KeyUp;
  webView->handleInputEvent(keyEvent);
}

static void configueCompositingWebView(WebSettings* settings) {
  settings->setAcceleratedCompositingEnabled(true);
  settings->setPreferCompositingToLCDTextEnabled(true);
}

TEST_P(WebViewTest, ShowPressOnTransformedLink) {
  std::unique_ptr<FrameTestHelpers::TestWebViewClient>
      fakeCompositingWebViewClient =
          makeUnique<FrameTestHelpers::TestWebViewClient>();
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl = webViewHelper.initialize(
      true, nullptr, fakeCompositingWebViewClient.get(), nullptr,
      &configueCompositingWebView);

  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));

  WebURL baseURL = URLTestHelpers::toKURL("http://example.com/");
  FrameTestHelpers::loadHTMLString(
      webViewImpl->mainFrame(),
      "<a href='http://www.test.com' style='position: absolute; left: 20px; "
      "top: 20px; width: 200px; transform:translateZ(0);'>A link to "
      "highlight</a>",
      baseURL);

  WebGestureEvent event;
  event.type = WebInputEvent::GestureShowPress;
  event.sourceDevice = WebGestureDeviceTouchscreen;
  event.x = 20;
  event.y = 20;

  // Just make sure we don't hit any asserts.
  webViewImpl->handleInputEvent(event);
}

class MockAutofillClient : public WebAutofillClient {
 public:
  MockAutofillClient()
      : m_ignoreTextChanges(false),
        m_textChangesFromUserGesture(0),
        m_textChangesWhileIgnored(0),
        m_textChangesWhileNotIgnored(0),
        m_userGestureNotificationsCount(0) {}

  ~MockAutofillClient() override {}

  void setIgnoreTextChanges(bool ignore) override {
    m_ignoreTextChanges = ignore;
  }
  void textFieldDidChange(const WebFormControlElement&) override {
    if (m_ignoreTextChanges)
      ++m_textChangesWhileIgnored;
    else
      ++m_textChangesWhileNotIgnored;

    if (UserGestureIndicator::processingUserGesture())
      ++m_textChangesFromUserGesture;
  }
  void firstUserGestureObserved() override {
    ++m_userGestureNotificationsCount;
  }

  void clearChangeCounts() {
    m_textChangesWhileIgnored = 0;
    m_textChangesWhileNotIgnored = 0;
  }

  int textChangesFromUserGesture() { return m_textChangesFromUserGesture; }
  int textChangesWhileIgnored() { return m_textChangesWhileIgnored; }
  int textChangesWhileNotIgnored() { return m_textChangesWhileNotIgnored; }
  int getUserGestureNotificationsCount() {
    return m_userGestureNotificationsCount;
  }

 private:
  bool m_ignoreTextChanges;
  int m_textChangesFromUserGesture;
  int m_textChangesWhileIgnored;
  int m_textChangesWhileNotIgnored;
  int m_userGestureNotificationsCount;
};

TEST_P(WebViewTest, LosingFocusDoesNotTriggerAutofillTextChange) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  MockAutofillClient client;
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->setAutofillClient(&client);
  webView->setInitialFocus(false);

  // Set up a composition that needs to be committed.
  WebVector<WebCompositionUnderline> emptyUnderlines;
  frame->setEditableSelectionOffsets(4, 10);
  frame->setCompositionFromExistingText(8, 12, emptyUnderlines);
  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ(4, info.selectionStart);
  EXPECT_EQ(10, info.selectionEnd);
  EXPECT_EQ(8, info.compositionStart);
  EXPECT_EQ(12, info.compositionEnd);

  // Clear the focus and track that the subsequent composition commit does not
  // trigger a text changed notification for autofill.
  client.clearChangeCounts();
  webView->setFocus(false);
  EXPECT_EQ(0, client.textChangesWhileNotIgnored());

  frame->setAutofillClient(0);
}

static void verifySelectionAndComposition(WebView* webView,
                                          int selectionStart,
                                          int selectionEnd,
                                          int compositionStart,
                                          int compositionEnd,
                                          const char* failMessage) {
  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ(selectionStart, info.selectionStart) << failMessage;
  EXPECT_EQ(selectionEnd, info.selectionEnd) << failMessage;
  EXPECT_EQ(compositionStart, info.compositionStart) << failMessage;
  EXPECT_EQ(compositionEnd, info.compositionEnd) << failMessage;
}

TEST_P(WebViewTest, CompositionNotCancelledByBackspace) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("composition_not_cancelled_by_backspace.html"));
  MockAutofillClient client;
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "composition_not_cancelled_by_backspace.html");
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->setAutofillClient(&client);
  webView->setInitialFocus(false);

  // Test both input elements.
  for (int i = 0; i < 2; ++i) {
    // Select composition and do sanity check.
    WebVector<WebCompositionUnderline> emptyUnderlines;
    frame->setEditableSelectionOffsets(6, 6);
    WebInputMethodController* activeInputMethodController =
        frame->frameWidget()->getActiveWebInputMethodController();
    EXPECT_TRUE(activeInputMethodController->setComposition(
        "fghij", emptyUnderlines, 0, 5));
    frame->setEditableSelectionOffsets(11, 11);
    verifySelectionAndComposition(webView, 11, 11, 6, 11, "initial case");

    // Press Backspace and verify composition didn't get cancelled. This is to
    // verify the fix for crbug.com/429916.
    WebKeyboardEvent keyEvent;
    keyEvent.domKey = Platform::current()->domKeyEnumFromString("\b");
    keyEvent.windowsKeyCode = VKEY_BACK;
    keyEvent.type = WebInputEvent::RawKeyDown;
    webView->handleInputEvent(keyEvent);

    frame->setEditableSelectionOffsets(6, 6);
    EXPECT_TRUE(activeInputMethodController->setComposition(
        "fghi", emptyUnderlines, 0, 4));
    frame->setEditableSelectionOffsets(10, 10);
    verifySelectionAndComposition(webView, 10, 10, 6, 10,
                                  "after pressing Backspace");

    keyEvent.type = WebInputEvent::KeyUp;
    webView->handleInputEvent(keyEvent);

    webView->advanceFocus(false);
  }

  frame->setAutofillClient(0);
}

TEST_P(WebViewTest, FinishComposingTextTriggersAutofillTextChange) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  MockAutofillClient client;
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->setAutofillClient(&client);
  webView->setInitialFocus(false);
  WebInputMethodController* activeInputMethodController =
      frame->frameWidget()->getActiveWebInputMethodController();
  // Set up a composition that needs to be committed.
  std::string compositionText("testingtext");

  WebVector<WebCompositionUnderline> emptyUnderlines;
  activeInputMethodController->setComposition(
      WebString::fromUTF8(compositionText.c_str()), emptyUnderlines, 0,
      compositionText.length());

  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ(0, info.selectionStart);
  EXPECT_EQ((int)compositionText.length(), info.selectionEnd);
  EXPECT_EQ(0, info.compositionStart);
  EXPECT_EQ((int)compositionText.length(), info.compositionEnd);

  client.clearChangeCounts();
  activeInputMethodController->finishComposingText(
      WebInputMethodController::KeepSelection);
  EXPECT_EQ(0, client.textChangesWhileIgnored());
  EXPECT_EQ(1, client.textChangesWhileNotIgnored());

  frame->setAutofillClient(0);
}

TEST_P(WebViewTest, SetCompositionFromExistingTextTriggersAutofillTextChange) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  MockAutofillClient client;
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html", true);
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->setAutofillClient(&client);
  webView->setInitialFocus(false);

  WebVector<WebCompositionUnderline> emptyUnderlines;

  client.clearChangeCounts();
  frame->setCompositionFromExistingText(8, 12, emptyUnderlines);

  WebTextInputInfo info = webView->textInputInfo();
  EXPECT_EQ("0123456789abcdefghijklmnopqrstuvwxyz",
            std::string(info.value.utf8().data()));
  EXPECT_EQ(8, info.compositionStart);
  EXPECT_EQ(12, info.compositionEnd);

  EXPECT_EQ(0, client.textChangesWhileIgnored());
  EXPECT_EQ(0, client.textChangesWhileNotIgnored());

  WebDocument document = webView->mainFrame()->document();
  EXPECT_EQ(WebString::fromUTF8("none"),
            document.getElementById("inputEvent").firstChild().nodeValue());

  frame->setAutofillClient(0);
}

class ViewCreatingWebViewClient : public FrameTestHelpers::TestWebViewClient {
 public:
  ViewCreatingWebViewClient() : m_didFocusCalled(false) {}

  // WebViewClient methods
  WebView* createView(WebLocalFrame* opener,
                      const WebURLRequest&,
                      const WebWindowFeatures&,
                      const WebString& name,
                      WebNavigationPolicy,
                      bool) override {
    return m_webViewHelper.initializeWithOpener(opener, true);
  }

  // WebWidgetClient methods
  void didFocus() override { m_didFocusCalled = true; }

  bool didFocusCalled() const { return m_didFocusCalled; }
  WebView* createdWebView() const { return m_webViewHelper.webView(); }

 private:
  FrameTestHelpers::WebViewHelper m_webViewHelper;
  bool m_didFocusCalled;
};

TEST_P(WebViewTest, DoNotFocusCurrentFrameOnNavigateFromLocalFrame) {
  ViewCreatingWebViewClient client;
  FrameTestHelpers::WebViewHelper m_webViewHelper;
  WebViewImpl* webViewImpl = m_webViewHelper.initialize(true, 0, &client);
  webViewImpl->page()->settings().setJavaScriptCanOpenWindowsAutomatically(
      true);

  WebURL baseURL = URLTestHelpers::toKURL("http://example.com/");
  FrameTestHelpers::loadHTMLString(
      webViewImpl->mainFrame(),
      "<html><body><iframe src=\"about:blank\"></iframe></body></html>",
      baseURL);

  // Make a request from a local frame.
  WebURLRequest webURLRequestWithTargetStart;
  LocalFrame* localFrame =
      toWebLocalFrameImpl(webViewImpl->mainFrame()->firstChild())->frame();
  FrameLoadRequest requestWithTargetStart(
      localFrame->document(), webURLRequestWithTargetStart.toResourceRequest(),
      "_top");
  localFrame->loader().load(requestWithTargetStart);
  EXPECT_FALSE(client.didFocusCalled());

  m_webViewHelper.reset();  // Remove dependency on locally scoped client.
}

TEST_P(WebViewTest, FocusExistingFrameOnNavigate) {
  ViewCreatingWebViewClient client;
  FrameTestHelpers::WebViewHelper m_webViewHelper;
  WebViewImpl* webViewImpl = m_webViewHelper.initialize(true, 0, &client);
  webViewImpl->page()->settings().setJavaScriptCanOpenWindowsAutomatically(
      true);
  WebLocalFrameImpl* frame = webViewImpl->mainFrameImpl();
  frame->setName("_start");

  // Make a request that will open a new window
  WebURLRequest webURLRequest;
  FrameLoadRequest request(0, webURLRequest.toResourceRequest(), "_blank");
  toLocalFrame(webViewImpl->page()->mainFrame())->loader().load(request);
  ASSERT_TRUE(client.createdWebView());
  EXPECT_FALSE(client.didFocusCalled());

  // Make a request from the new window that will navigate the original window.
  // The original window should be focused.
  WebURLRequest webURLRequestWithTargetStart;
  FrameLoadRequest requestWithTargetStart(
      0, webURLRequestWithTargetStart.toResourceRequest(), "_start");
  toLocalFrame(toWebViewImpl(client.createdWebView())->page()->mainFrame())
      ->loader()
      .load(requestWithTargetStart);
  EXPECT_TRUE(client.didFocusCalled());

  m_webViewHelper.reset();  // Remove dependency on locally scoped client.
}

TEST_P(WebViewTest, DispatchesFocusOutFocusInOnViewToggleFocus) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), "focusout_focusin_events.html");
  WebView* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "focusout_focusin_events.html", true, 0);

  webView->setFocus(true);
  webView->setFocus(false);
  webView->setFocus(true);

  WebElement element =
      webView->mainFrame()->document().getElementById("message");
  EXPECT_STREQ("focusoutfocusin", element.textContent().utf8().data());
}

TEST_P(WebViewTest, DispatchesDomFocusOutDomFocusInOnViewToggleFocus) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      "domfocusout_domfocusin_events.html");
  WebView* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "domfocusout_domfocusin_events.html", true, 0);

  webView->setFocus(true);
  webView->setFocus(false);
  webView->setFocus(true);

  WebElement element =
      webView->mainFrame()->document().getElementById("message");
  EXPECT_STREQ("DOMFocusOutDOMFocusIn", element.textContent().utf8().data());
}

static void openDateTimeChooser(WebView* webView,
                                HTMLInputElement* inputElement) {
  inputElement->focus();

  WebKeyboardEvent keyEvent;
  keyEvent.domKey = Platform::current()->domKeyEnumFromString(" ");
  keyEvent.windowsKeyCode = VKEY_SPACE;
  keyEvent.type = WebInputEvent::RawKeyDown;
  webView->handleInputEvent(keyEvent);

  keyEvent.type = WebInputEvent::KeyUp;
  webView->handleInputEvent(keyEvent);
}

// TODO(crbug.com/605112) This test is crashing on Android (Nexus 4) bot.
#if OS(ANDROID)
TEST_P(WebViewTest, DISABLED_ChooseValueFromDateTimeChooser) {
#else
TEST_P(WebViewTest, ChooseValueFromDateTimeChooser) {
#endif
  bool originalMultipleFieldsFlag =
      RuntimeEnabledFeatures::inputMultipleFieldsUIEnabled();
  RuntimeEnabledFeatures::setInputMultipleFieldsUIEnabled(false);
  DateTimeChooserWebViewClient client;
  std::string url = m_baseURL + "date_time_chooser.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "date_time_chooser.html");
  WebViewImpl* webViewImpl =
      m_webViewHelper.initializeAndLoad(url, true, 0, &client);

  Document* document = webViewImpl->mainFrameImpl()->frame()->document();

  HTMLInputElement* inputElement;

  inputElement = toHTMLInputElement(document->getElementById("date"));
  openDateTimeChooser(webViewImpl, inputElement);
  client.chooserCompletion()->didChooseValue(0);
  client.clearChooserCompletion();
  EXPECT_STREQ("1970-01-01", inputElement->value().utf8().data());

  openDateTimeChooser(webViewImpl, inputElement);
  client.chooserCompletion()->didChooseValue(
      std::numeric_limits<double>::quiet_NaN());
  client.clearChooserCompletion();
  EXPECT_STREQ("", inputElement->value().utf8().data());

  inputElement = toHTMLInputElement(document->getElementById("datetimelocal"));
  openDateTimeChooser(webViewImpl, inputElement);
  client.chooserCompletion()->didChooseValue(0);
  client.clearChooserCompletion();
  EXPECT_STREQ("1970-01-01T00:00", inputElement->value().utf8().data());

  openDateTimeChooser(webViewImpl, inputElement);
  client.chooserCompletion()->didChooseValue(
      std::numeric_limits<double>::quiet_NaN());
  client.clearChooserCompletion();
  EXPECT_STREQ("", inputElement->value().utf8().data());

  inputElement = toHTMLInputElement(document->getElementById("month"));
  openDateTimeChooser(webViewImpl, inputElement);
  client.chooserCompletion()->didChooseValue(0);
  client.clearChooserCompletion();
  EXPECT_STREQ("1970-01", inputElement->value().utf8().data());

  openDateTimeChooser(webViewImpl, inputElement);
  client.chooserCompletion()->didChooseValue(
      std::numeric_limits<double>::quiet_NaN());
  client.clearChooserCompletion();
  EXPECT_STREQ("", inputElement->value().utf8().data());

  inputElement = toHTMLInputElement(document->getElementById("time"));
  openDateTimeChooser(webViewImpl, inputElement);
  client.chooserCompletion()->didChooseValue(0);
  client.clearChooserCompletion();
  EXPECT_STREQ("00:00", inputElement->value().utf8().data());

  openDateTimeChooser(webViewImpl, inputElement);
  client.chooserCompletion()->didChooseValue(
      std::numeric_limits<double>::quiet_NaN());
  client.clearChooserCompletion();
  EXPECT_STREQ("", inputElement->value().utf8().data());

  inputElement = toHTMLInputElement(document->getElementById("week"));
  openDateTimeChooser(webViewImpl, inputElement);
  client.chooserCompletion()->didChooseValue(0);
  client.clearChooserCompletion();
  EXPECT_STREQ("1970-W01", inputElement->value().utf8().data());

  openDateTimeChooser(webViewImpl, inputElement);
  client.chooserCompletion()->didChooseValue(
      std::numeric_limits<double>::quiet_NaN());
  client.clearChooserCompletion();
  EXPECT_STREQ("", inputElement->value().utf8().data());

  // Clear the WebViewClient from the webViewHelper to avoid use-after-free in
  // the WebViewHelper destructor.
  m_webViewHelper.reset();
  RuntimeEnabledFeatures::setInputMultipleFieldsUIEnabled(
      originalMultipleFieldsFlag);
}

TEST_P(WebViewTest, DispatchesFocusBlurOnViewToggle) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), "focus_blur_events.html");
  WebView* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "focus_blur_events.html", true, 0);

  webView->setFocus(true);
  webView->setFocus(false);
  webView->setFocus(true);

  WebElement element =
      webView->mainFrame()->document().getElementById("message");
  // Expect not to see duplication of events.
  EXPECT_STREQ("blurfocus", element.textContent().utf8().data());
}

TEST_P(WebViewTest, SmartClipData) {
  static const char kExpectedClipText[] = "\nPrice 10,000,000won";
  static const char kExpectedClipHtml[] =
      "<div id=\"div4\" style=\"padding: 10px; margin: 10px; border: 2px "
      "solid skyblue; float: left; width: 190px; height: 30px; "
      "color: rgb(0, 0, 0); font-family: myahem; font-size: 8px; font-style: "
      "normal; font-variant-ligatures: normal; font-variant-caps: normal; "
      "font-weight: normal; letter-spacing: "
      "normal; orphans: 2; text-align: start; "
      "text-indent: 0px; text-transform: none; white-space: normal; widows: "
      "2; word-spacing: 0px; -webkit-text-stroke-width: 0px; "
      "text-decoration-style: initial; text-decoration-color: initial;\">Air "
      "conditioner</div><div id=\"div5\" style=\"padding: 10px; margin: "
      "10px; border: 2px solid skyblue; float: left; width: "
      "190px; height: 30px; color: rgb(0, 0, 0); font-family: myahem; "
      "font-size: 8px; font-style: normal; font-variant-ligatures: normal; "
      "font-variant-caps: normal; font-weight: normal; "
      "letter-spacing: normal; orphans: 2; "
      "text-align: start; text-indent: 0px; text-transform: "
      "none; white-space: normal; widows: 2; word-spacing: 0px; "
      "-webkit-text-stroke-width: 0px; text-decoration-style: initial; "
      "text-decoration-color: initial;\">Price 10,000,000won</div>";
  WebString clipText;
  WebString clipHtml;
  WebRect clipRect;
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("Ahem.ttf"));
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("smartclip.html"));
  WebView* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "smartclip.html");
  webView->resize(WebSize(500, 500));
  webView->updateAllLifecyclePhases();
  WebRect cropRect(300, 125, 152, 50);
  webView->extractSmartClipData(cropRect, clipText, clipHtml, clipRect);
  EXPECT_STREQ(kExpectedClipText, clipText.utf8().c_str());
  EXPECT_STREQ(kExpectedClipHtml, clipHtml.utf8().c_str());
}

TEST_P(WebViewTest, SmartClipDataWithPinchZoom) {
  static const char kExpectedClipText[] = "\nPrice 10,000,000won";
  static const char kExpectedClipHtml[] =
      "<div id=\"div4\" style=\"padding: 10px; margin: 10px; border: 2px "
      "solid skyblue; float: left; width: 190px; height: 30px; "
      "color: rgb(0, 0, 0); font-family: myahem; font-size: 8px; font-style: "
      "normal; font-variant-ligatures: normal; font-variant-caps: normal; "
      "font-weight: normal; letter-spacing: "
      "normal; orphans: 2; text-align: start; "
      "text-indent: 0px; text-transform: none; white-space: normal; widows: "
      "2; word-spacing: 0px; -webkit-text-stroke-width: 0px; "
      "text-decoration-style: initial; text-decoration-color: initial;\">Air "
      "conditioner</div><div id=\"div5\" style=\"padding: 10px; margin: "
      "10px; border: 2px solid skyblue; float: left; width: "
      "190px; height: 30px; color: rgb(0, 0, 0); font-family: myahem; "
      "font-size: 8px; font-style: normal; font-variant-ligatures: normal; "
      "font-variant-caps: normal; font-weight: normal; letter-spacing: normal; "
      "orphans: 2; text-align: start; text-indent: 0px; "
      "text-transform: none; white-space: normal; widows: 2; "
      "word-spacing: 0px; -webkit-text-stroke-width: 0px;"
      " text-decoration-style: initial; text-decoration-color: initial;\">"
      "Price 10,000,000won</div>";
  WebString clipText;
  WebString clipHtml;
  WebRect clipRect;
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("Ahem.ttf"));
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("smartclip.html"));
  WebView* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "smartclip.html");
  webView->resize(WebSize(500, 500));
  webView->updateAllLifecyclePhases();
  webView->setPageScaleFactor(1.5);
  webView->setVisualViewportOffset(WebFloatPoint(167, 100));
  WebRect cropRect(200, 38, 228, 75);
  webView->extractSmartClipData(cropRect, clipText, clipHtml, clipRect);
  EXPECT_STREQ(kExpectedClipText, clipText.utf8().c_str());
  EXPECT_STREQ(kExpectedClipHtml, clipHtml.utf8().c_str());
}

TEST_P(WebViewTest, SmartClipReturnsEmptyStringsWhenUserSelectIsNone) {
  WebString clipText;
  WebString clipHtml;
  WebRect clipRect;
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("Ahem.ttf"));
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("smartclip_user_select_none.html"));
  WebView* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "smartclip_user_select_none.html");
  webView->resize(WebSize(500, 500));
  webView->updateAllLifecyclePhases();
  WebRect cropRect(0, 0, 100, 100);
  webView->extractSmartClipData(cropRect, clipText, clipHtml, clipRect);
  EXPECT_STREQ("", clipText.utf8().c_str());
  EXPECT_STREQ("", clipHtml.utf8().c_str());
}

TEST_P(WebViewTest, SmartClipDoesNotCrashPositionReversed) {
  WebString clipText;
  WebString clipHtml;
  WebRect clipRect;
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("Ahem.ttf"));
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("smartclip_reversed_positions.html"));
  WebView* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "smartclip_reversed_positions.html");
  webView->resize(WebSize(500, 500));
  webView->updateAllLifecyclePhases();
  // Left upper corner of the rect will be end position in the DOM hierarchy.
  WebRect cropRect(30, 110, 400, 250);
  // This should not still crash. See crbug.com/589082 for more details.
  webView->extractSmartClipData(cropRect, clipText, clipHtml, clipRect);
}

class CreateChildCounterFrameClient
    : public FrameTestHelpers::TestWebFrameClient {
 public:
  CreateChildCounterFrameClient() : m_count(0) {}
  WebLocalFrame* createChildFrame(WebLocalFrame* parent,
                                  WebTreeScopeType,
                                  const WebString& name,
                                  const WebString& uniqueName,
                                  WebSandboxFlags,
                                  const WebFrameOwnerProperties&) override;

  int count() const { return m_count; }

 private:
  int m_count;
};

WebLocalFrame* CreateChildCounterFrameClient::createChildFrame(
    WebLocalFrame* parent,
    WebTreeScopeType scope,
    const WebString& name,
    const WebString& uniqueName,
    WebSandboxFlags sandboxFlags,
    const WebFrameOwnerProperties& frameOwnerProperties) {
  ++m_count;
  return TestWebFrameClient::createChildFrame(
      parent, scope, name, uniqueName, sandboxFlags, frameOwnerProperties);
}

TEST_P(WebViewTest, ChangeDisplayMode) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("display_mode.html"));
  WebView* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "display_mode.html", true);

  std::string content =
      WebFrameContentDumper::dumpWebViewAsText(webView, 21).utf8();
  EXPECT_EQ("regular-ui", content);

  webView->setDisplayMode(WebDisplayModeMinimalUi);
  content = WebFrameContentDumper::dumpWebViewAsText(webView, 21).utf8();
  EXPECT_EQ("minimal-ui", content);
  m_webViewHelper.reset();
}

TEST_P(WebViewTest, AddFrameInCloseUnload) {
  CreateChildCounterFrameClient frameClient;
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("add_frame_in_unload.html"));
  m_webViewHelper.initializeAndLoad(m_baseURL + "add_frame_in_unload.html",
                                    true, &frameClient);
  m_webViewHelper.reset();
  EXPECT_EQ(0, frameClient.count());
}

TEST_P(WebViewTest, AddFrameInCloseURLUnload) {
  CreateChildCounterFrameClient frameClient;
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("add_frame_in_unload.html"));
  m_webViewHelper.initializeAndLoad(m_baseURL + "add_frame_in_unload.html",
                                    true, &frameClient);
  m_webViewHelper.webView()->mainFrame()->dispatchUnloadEvent();
  EXPECT_EQ(0, frameClient.count());
  m_webViewHelper.reset();
}

TEST_P(WebViewTest, AddFrameInNavigateUnload) {
  CreateChildCounterFrameClient frameClient;
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("add_frame_in_unload.html"));
  m_webViewHelper.initializeAndLoad(m_baseURL + "add_frame_in_unload.html",
                                    true, &frameClient);
  FrameTestHelpers::loadFrame(m_webViewHelper.webView()->mainFrame(),
                              "about:blank");
  EXPECT_EQ(0, frameClient.count());
  m_webViewHelper.reset();
}

TEST_P(WebViewTest, AddFrameInChildInNavigateUnload) {
  CreateChildCounterFrameClient frameClient;
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("add_frame_in_unload_wrapper.html"));
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("add_frame_in_unload.html"));
  m_webViewHelper.initializeAndLoad(
      m_baseURL + "add_frame_in_unload_wrapper.html", true, &frameClient);
  FrameTestHelpers::loadFrame(m_webViewHelper.webView()->mainFrame(),
                              "about:blank");
  EXPECT_EQ(1, frameClient.count());
  m_webViewHelper.reset();
}

class TouchEventHandlerWebViewClient
    : public FrameTestHelpers::TestWebViewClient {
 public:
  // WebWidgetClient methods
  void hasTouchEventHandlers(bool state) override {
    m_hasTouchEventHandlerCount[state]++;
  }

  // Local methods
  TouchEventHandlerWebViewClient() : m_hasTouchEventHandlerCount() {}

  int getAndResetHasTouchEventHandlerCallCount(bool state) {
    int value = m_hasTouchEventHandlerCount[state];
    m_hasTouchEventHandlerCount[state] = 0;
    return value;
  }

 private:
  int m_hasTouchEventHandlerCount[2];
};

// This test verifies that WebWidgetClient::hasTouchEventHandlers is called
// accordingly for various calls to EventHandlerRegistry::did{Add|Remove|
// RemoveAll}EventHandler(..., TouchEvent). Verifying that those calls are made
// correctly is the job of LayoutTests/fast/events/event-handler-count.html.
TEST_P(WebViewTest, HasTouchEventHandlers) {
  TouchEventHandlerWebViewClient client;
  std::string url = m_baseURL + "has_touch_event_handlers.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url),
                                        "has_touch_event_handlers.html");
  WebViewImpl* webViewImpl =
      m_webViewHelper.initializeAndLoad(url, true, 0, &client);
  const EventHandlerRegistry::EventHandlerClass touchEvent =
      EventHandlerRegistry::TouchStartOrMoveEventBlocking;

  // The page is initialized with at least one no-handlers call.
  // In practice we get two such calls because WebViewHelper::initializeAndLoad
  // first initializes and empty frame, and then loads a document into it, so
  // there are two FrameLoader::commitProvisionalLoad calls.
  EXPECT_GE(client.getAndResetHasTouchEventHandlerCallCount(false), 1);
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Adding the first document handler results in a has-handlers call.
  Document* document = webViewImpl->mainFrameImpl()->frame()->document();
  EventHandlerRegistry* registry =
      &document->frameHost()->eventHandlerRegistry();
  registry->didAddEventHandler(*document, touchEvent);
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(1, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Adding another handler has no effect.
  registry->didAddEventHandler(*document, touchEvent);
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Removing the duplicate handler has no effect.
  registry->didRemoveEventHandler(*document, touchEvent);
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Removing the final handler results in a no-handlers call.
  registry->didRemoveEventHandler(*document, touchEvent);
  EXPECT_EQ(1, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Adding a handler on a div results in a has-handlers call.
  Element* parentDiv = document->getElementById("parentdiv");
  DCHECK(parentDiv);
  registry->didAddEventHandler(*parentDiv, touchEvent);
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(1, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Adding a duplicate handler on the div, clearing all document handlers
  // (of which there are none) and removing the extra handler on the div
  // all have no effect.
  registry->didAddEventHandler(*parentDiv, touchEvent);
  registry->didRemoveAllEventHandlers(*document);
  registry->didRemoveEventHandler(*parentDiv, touchEvent);
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Removing the final handler on the div results in a no-handlers call.
  registry->didRemoveEventHandler(*parentDiv, touchEvent);
  EXPECT_EQ(1, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Adding two handlers then clearing them in a single call results in a
  // has-handlers then no-handlers call.
  registry->didAddEventHandler(*parentDiv, touchEvent);
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(1, client.getAndResetHasTouchEventHandlerCallCount(true));
  registry->didAddEventHandler(*parentDiv, touchEvent);
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(true));
  registry->didRemoveAllEventHandlers(*parentDiv);
  EXPECT_EQ(1, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Adding a handler inside of a child iframe results in a has-handlers call.
  Element* childFrame = document->getElementById("childframe");
  DCHECK(childFrame);
  Document* childDocument = toHTMLIFrameElement(childFrame)->contentDocument();
  Element* childDiv = childDocument->getElementById("childdiv");
  DCHECK(childDiv);
  registry->didAddEventHandler(*childDiv, touchEvent);
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(1, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Adding and clearing handlers in the parent doc or elsewhere in the child
  // doc has no impact.
  registry->didAddEventHandler(*document, touchEvent);
  registry->didAddEventHandler(*childFrame, touchEvent);
  registry->didAddEventHandler(*childDocument, touchEvent);
  registry->didRemoveAllEventHandlers(*document);
  registry->didRemoveAllEventHandlers(*childFrame);
  registry->didRemoveAllEventHandlers(*childDocument);
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Removing the final handler inside the child frame results in a no-handlers
  // call.
  registry->didRemoveAllEventHandlers(*childDiv);
  EXPECT_EQ(1, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Adding a handler inside the child frame results in a has-handlers call.
  registry->didAddEventHandler(*childDocument, touchEvent);
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(1, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Adding a handler in the parent document and removing the one in the frame
  // has no effect.
  registry->didAddEventHandler(*childFrame, touchEvent);
  registry->didRemoveEventHandler(*childDocument, touchEvent);
  registry->didRemoveAllEventHandlers(*childDocument);
  registry->didRemoveAllEventHandlers(*document);
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Now removing the handler in the parent document results in a no-handlers
  // call.
  registry->didRemoveEventHandler(*childFrame, touchEvent);
  EXPECT_EQ(1, client.getAndResetHasTouchEventHandlerCallCount(false));
  EXPECT_EQ(0, client.getAndResetHasTouchEventHandlerCallCount(true));

  // Free the webView before the TouchEventHandlerWebViewClient gets freed.
  m_webViewHelper.reset();
}

// This test checks that deleting nodes which have only non-JS-registered touch
// handlers also removes them from the event handler registry. Note that this
// is different from detaching and re-attaching the same node, which is covered
// by layout tests under fast/events/.
TEST_P(WebViewTest, DeleteElementWithRegisteredHandler) {
  std::string url = m_baseURL + "simple_div.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "simple_div.html");
  WebViewImpl* webViewImpl = m_webViewHelper.initializeAndLoad(url, true);

  Persistent<Document> document =
      webViewImpl->mainFrameImpl()->frame()->document();
  Element* div = document->getElementById("div");
  EventHandlerRegistry& registry =
      document->frameHost()->eventHandlerRegistry();

  registry.didAddEventHandler(*div, EventHandlerRegistry::ScrollEvent);
  EXPECT_TRUE(registry.hasEventHandlers(EventHandlerRegistry::ScrollEvent));

  TrackExceptionState exceptionState;
  div->remove(exceptionState);

  // For oilpan we have to force a GC to ensure the event handlers have been
  // removed when checking below. We do a precise GC (collectAllGarbage does not
  // scan the stack) to ensure the div element dies. This is also why the
  // Document is in a Persistent since we want that to stay around.
  ThreadState::current()->collectAllGarbage();

  EXPECT_FALSE(registry.hasEventHandlers(EventHandlerRegistry::ScrollEvent));
}

class NonUserInputTextUpdateWebWidgetClient
    : public FrameTestHelpers::TestWebWidgetClient {
 public:
  NonUserInputTextUpdateWebWidgetClient() : m_textIsUpdated(false) {}

  // WebWidgetClient methods
  void didUpdateTextOfFocusedElementByNonUserInput() override {
    m_textIsUpdated = true;
  }

  void reset() { m_textIsUpdated = false; }

  bool textIsUpdated() const { return m_textIsUpdated; }

 private:
  int m_textIsUpdated;
};

// This test verifies the text input flags are correctly exposed to script.
TEST_P(WebViewTest, TextInputFlags) {
  std::string url = m_baseURL + "text_input_flags.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "text_input_flags.html");
  WebViewImpl* webViewImpl = m_webViewHelper.initializeAndLoad(url, true);
  webViewImpl->setInitialFocus(false);

  WebLocalFrameImpl* frame = webViewImpl->mainFrameImpl();
  Document* document = frame->frame()->document();

  // (A) <input>
  // (A.1) Verifies autocorrect/autocomplete/spellcheck flags are Off and
  // autocapitalize is set to none.
  HTMLInputElement* inputElement =
      toHTMLInputElement(document->getElementById("input"));
  document->setFocusedElement(
      inputElement,
      FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeNone, nullptr));
  webViewImpl->setFocus(true);
  WebTextInputInfo info1 = webViewImpl->textInputInfo();
  EXPECT_EQ(WebTextInputFlagAutocompleteOff | WebTextInputFlagAutocorrectOff |
                WebTextInputFlagSpellcheckOff |
                WebTextInputFlagAutocapitalizeNone,
            info1.flags);

  // (A.2) Verifies autocorrect/autocomplete/spellcheck flags are On and
  // autocapitalize is set to sentences.
  inputElement = toHTMLInputElement(document->getElementById("input2"));
  document->setFocusedElement(
      inputElement,
      FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeNone, nullptr));
  webViewImpl->setFocus(true);
  WebTextInputInfo info2 = webViewImpl->textInputInfo();
  EXPECT_EQ(WebTextInputFlagAutocompleteOn | WebTextInputFlagAutocorrectOn |
                WebTextInputFlagSpellcheckOn |
                WebTextInputFlagAutocapitalizeSentences,
            info2.flags);

  // (B) <textarea> Verifies the default text input flags are
  // WebTextInputFlagAutocapitalizeSentences.
  HTMLTextAreaElement* textAreaElement =
      toHTMLTextAreaElement(document->getElementById("textarea"));
  document->setFocusedElement(
      textAreaElement,
      FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeNone, nullptr));
  webViewImpl->setFocus(true);
  WebTextInputInfo info3 = webViewImpl->textInputInfo();
  EXPECT_EQ(WebTextInputFlagAutocapitalizeSentences, info3.flags);

  // (C) Verifies the WebTextInputInfo's don't equal.
  EXPECT_FALSE(info1.equals(info2));
  EXPECT_FALSE(info2.equals(info3));

  // Free the webView before freeing the NonUserInputTextUpdateWebViewClient.
  m_webViewHelper.reset();
}

// This test verifies that
// WebWidgetClient::didUpdateTextOfFocusedElementByNonUserInput is called iff
// value of a focused element is modified via script.
TEST_P(WebViewTest, NonUserInputTextUpdate) {
  NonUserInputTextUpdateWebWidgetClient client;
  std::string url = m_baseURL + "non_user_input_text_update.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url),
                                        "non_user_input_text_update.html");
  WebViewImpl* webViewImpl =
      m_webViewHelper.initializeAndLoad(url, true, nullptr, nullptr, &client);
  webViewImpl->setInitialFocus(false);

  WebLocalFrameImpl* frame = webViewImpl->mainFrameImpl();
  Document* document = frame->frame()->document();
  WebInputMethodController* activeInputMethodController =
      frame->frameWidget()->getActiveWebInputMethodController();

  // (A) <input>
  // (A.1) Focused and value is changed by script.
  client.reset();
  EXPECT_FALSE(client.textIsUpdated());

  HTMLInputElement* inputElement =
      toHTMLInputElement(document->getElementById("input"));
  document->setFocusedElement(
      inputElement,
      FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeNone, nullptr));
  webViewImpl->setFocus(true);
  EXPECT_EQ(document->focusedElement(), static_cast<Element*>(inputElement));

  // Emulate value change from script.
  inputElement->setValue("testA");
  EXPECT_TRUE(client.textIsUpdated());
  WebTextInputInfo info = webViewImpl->textInputInfo();
  EXPECT_EQ("testA", std::string(info.value.utf8().data()));

  // (A.2) Focused and user input modifies value.
  client.reset();
  EXPECT_FALSE(client.textIsUpdated());

  WebVector<WebCompositionUnderline> emptyUnderlines;
  activeInputMethodController->setComposition(WebString::fromUTF8("2"),
                                              emptyUnderlines, 1, 1);
  activeInputMethodController->finishComposingText(
      WebInputMethodController::KeepSelection);
  EXPECT_FALSE(client.textIsUpdated());
  info = webViewImpl->textInputInfo();
  EXPECT_EQ("testA2", std::string(info.value.utf8().data()));

  // (A.3) Unfocused and value is changed by script.
  client.reset();
  EXPECT_FALSE(client.textIsUpdated());
  document->clearFocusedElement();
  webViewImpl->setFocus(false);
  EXPECT_NE(document->focusedElement(), static_cast<Element*>(inputElement));
  inputElement->setValue("testA3");
  EXPECT_FALSE(client.textIsUpdated());

  // (B) <textarea>
  // (B.1) Focused and value is changed by script.
  client.reset();
  EXPECT_FALSE(client.textIsUpdated());
  HTMLTextAreaElement* textAreaElement =
      toHTMLTextAreaElement(document->getElementById("textarea"));
  document->setFocusedElement(
      textAreaElement,
      FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeNone, nullptr));
  webViewImpl->setFocus(true);
  EXPECT_EQ(document->focusedElement(), static_cast<Element*>(textAreaElement));
  textAreaElement->setValue("testB");
  EXPECT_TRUE(client.textIsUpdated());
  info = webViewImpl->textInputInfo();
  EXPECT_EQ("testB", std::string(info.value.utf8().data()));

  // (B.2) Focused and user input modifies value.
  client.reset();
  EXPECT_FALSE(client.textIsUpdated());
  activeInputMethodController->setComposition(WebString::fromUTF8("2"),
                                              emptyUnderlines, 1, 1);
  activeInputMethodController->finishComposingText(
      WebInputMethodController::KeepSelection);
  info = webViewImpl->textInputInfo();
  EXPECT_EQ("testB2", std::string(info.value.utf8().data()));

  // (B.3) Unfocused and value is changed by script.
  client.reset();
  EXPECT_FALSE(client.textIsUpdated());
  document->clearFocusedElement();
  webViewImpl->setFocus(false);
  EXPECT_NE(document->focusedElement(), static_cast<Element*>(textAreaElement));
  inputElement->setValue("testB3");
  EXPECT_FALSE(client.textIsUpdated());

  // Free the webView before freeing the NonUserInputTextUpdateWebViewClient.
  m_webViewHelper.reset();
}

// Check that the WebAutofillClient is correctly notified about first user
// gestures after load, following various input events.
TEST_P(WebViewTest, FirstUserGestureObservedKeyEvent) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("form.html"));
  MockAutofillClient client;
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "form.html", true);
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->setAutofillClient(&client);
  webView->setInitialFocus(false);

  EXPECT_EQ(0, client.getUserGestureNotificationsCount());

  WebKeyboardEvent keyEvent;
  keyEvent.domKey = Platform::current()->domKeyEnumFromString(" ");
  keyEvent.windowsKeyCode = VKEY_SPACE;
  keyEvent.type = WebInputEvent::RawKeyDown;
  webView->handleInputEvent(keyEvent);
  keyEvent.type = WebInputEvent::KeyUp;
  webView->handleInputEvent(keyEvent);

  EXPECT_EQ(1, client.getUserGestureNotificationsCount());
  frame->setAutofillClient(0);
}

TEST_P(WebViewTest, FirstUserGestureObservedMouseEvent) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("form.html"));
  MockAutofillClient client;
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "form.html", true);
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->setAutofillClient(&client);
  webView->setInitialFocus(false);

  EXPECT_EQ(0, client.getUserGestureNotificationsCount());

  WebMouseEvent mouseEvent;
  mouseEvent.button = WebMouseEvent::Button::Left;
  mouseEvent.x = 1;
  mouseEvent.y = 1;
  mouseEvent.clickCount = 1;
  mouseEvent.type = WebInputEvent::MouseDown;
  webView->handleInputEvent(mouseEvent);
  mouseEvent.type = WebInputEvent::MouseUp;
  webView->handleInputEvent(mouseEvent);

  EXPECT_EQ(1, client.getUserGestureNotificationsCount());
  frame->setAutofillClient(0);
}

TEST_P(WebViewTest, FirstUserGestureObservedGestureTap) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("longpress_selection.html"));
  MockAutofillClient client;
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "longpress_selection.html", true);
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->setAutofillClient(&client);
  webView->setInitialFocus(false);

  EXPECT_EQ(0, client.getUserGestureNotificationsCount());

  EXPECT_TRUE(
      tapElementById(WebInputEvent::GestureTap, WebString::fromUTF8("target")));

  EXPECT_EQ(1, client.getUserGestureNotificationsCount());
  frame->setAutofillClient(0);
}

TEST_P(WebViewTest, CompositionIsUserGesture) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_populated.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_populated.html");
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  MockAutofillClient client;
  frame->setAutofillClient(&client);
  webView->setInitialFocus(false);

  EXPECT_TRUE(
      frame->frameWidget()->getActiveWebInputMethodController()->setComposition(
          WebString::fromUTF8(std::string("hello").c_str()),
          WebVector<WebCompositionUnderline>(), 3, 3));
  EXPECT_EQ(1, client.textChangesFromUserGesture());
  EXPECT_FALSE(UserGestureIndicator::processingUserGesture());
  EXPECT_TRUE(frame->hasMarkedText());

  frame->setAutofillClient(0);
}

TEST_P(WebViewTest, CompareSelectAllToContentAsText) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("longpress_selection.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "longpress_selection.html", true);

  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->executeScript(WebScriptSource(
      WebString::fromUTF8("document.execCommand('SelectAll', false, null)")));
  std::string actual = frame->selectionAsText().utf8();

  const int kMaxOutputCharacters = 1024;
  std::string expected =
      WebFrameContentDumper::dumpWebViewAsText(webView, kMaxOutputCharacters)
          .utf8();
  EXPECT_EQ(expected, actual);
}

TEST_P(WebViewTest, AutoResizeSubtreeLayout) {
  std::string url = m_baseURL + "subtree-layout.html";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "subtree-layout.html");
  WebView* webView = m_webViewHelper.initialize(true);

  webView->enableAutoResizeMode(WebSize(200, 200), WebSize(200, 200));
  loadFrame(webView->mainFrame(), url);

  FrameView* frameView =
      m_webViewHelper.webView()->mainFrameImpl()->frameView();

  // Auto-resizing used to DCHECK(needsLayout()) in LayoutBlockFlow::layout.
  // This EXPECT is merely a dummy. The real test is that we don't trigger
  // asserts in debug builds.
  EXPECT_FALSE(frameView->needsLayout());
};

TEST_P(WebViewTest, PreferredSize) {
  std::string url = m_baseURL + "specify_size.html?100px:100px";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "specify_size.html");
  WebView* webView = m_webViewHelper.initializeAndLoad(url, true);

  WebSize size = webView->contentsPreferredMinimumSize();
  EXPECT_EQ(100, size.width);
  EXPECT_EQ(100, size.height);

  webView->setZoomLevel(WebView::zoomFactorToZoomLevel(2.0));
  size = webView->contentsPreferredMinimumSize();
  EXPECT_EQ(200, size.width);
  EXPECT_EQ(200, size.height);

  // Verify that both width and height are rounded (in this case up)
  webView->setZoomLevel(WebView::zoomFactorToZoomLevel(0.9995));
  size = webView->contentsPreferredMinimumSize();
  EXPECT_EQ(100, size.width);
  EXPECT_EQ(100, size.height);

  // Verify that both width and height are rounded (in this case down)
  webView->setZoomLevel(WebView::zoomFactorToZoomLevel(1.0005));
  size = webView->contentsPreferredMinimumSize();
  EXPECT_EQ(100, size.width);
  EXPECT_EQ(100, size.height);

  url = m_baseURL + "specify_size.html?1.5px:1.5px";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "specify_size.html");
  webView = m_webViewHelper.initializeAndLoad(url, true);

  webView->setZoomLevel(WebView::zoomFactorToZoomLevel(1));
  size = webView->contentsPreferredMinimumSize();
  EXPECT_EQ(2, size.width);
  EXPECT_EQ(2, size.height);
}

TEST_P(WebViewTest, PreferredSizeDirtyLayout) {
  std::string url = m_baseURL + "specify_size.html?100px:100px";
  URLTestHelpers::registerMockedURLLoad(toKURL(url), "specify_size.html");
  WebView* webView = m_webViewHelper.initializeAndLoad(url, true);
  WebElement documentElement =
      webView->mainFrame()->document().documentElement();

  WebSize size = webView->contentsPreferredMinimumSize();
  EXPECT_EQ(100, size.width);
  EXPECT_EQ(100, size.height);

  documentElement.setAttribute("style", "display: none");

  size = webView->contentsPreferredMinimumSize();
  EXPECT_EQ(0, size.width);
  EXPECT_EQ(0, size.height);
}

class UnhandledTapWebViewClient : public FrameTestHelpers::TestWebViewClient {
 public:
  void showUnhandledTapUIIfNeeded(const WebPoint& tappedPosition,
                                  const WebNode& tappedNode,
                                  bool pageChanged) override {
    m_wasCalled = true;
    m_tappedPosition = tappedPosition;
    m_tappedNode = tappedNode;
    m_pageChanged = pageChanged;
  }
  bool getWasCalled() const { return m_wasCalled; }
  int getTappedXPos() const { return m_tappedPosition.x(); }
  int getTappedYPos() const { return m_tappedPosition.y(); }
  bool isTappedNodeNull() const { return m_tappedNode.isNull(); }
  const WebNode& getWebNode() const { return m_tappedNode; }
  bool getPageChanged() const { return m_pageChanged; }
  void reset() {
    m_wasCalled = false;
    m_tappedPosition = IntPoint();
    m_tappedNode = WebNode();
    m_pageChanged = false;
  }

 private:
  bool m_wasCalled = false;
  IntPoint m_tappedPosition;
  WebNode m_tappedNode;
  bool m_pageChanged = false;
};

TEST_P(WebViewTest, ShowUnhandledTapUIIfNeeded) {
  std::string testFile = "show_unhandled_tap.html";
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("Ahem.ttf"));
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(testFile));
  UnhandledTapWebViewClient client;
  WebView* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + testFile, true, 0, &client);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();

  // Scroll the bottom into view so we can distinguish window coordinates from
  // document coordinates.
  EXPECT_TRUE(
      tapElementById(WebInputEvent::GestureTap, WebString::fromUTF8("bottom")));
  EXPECT_TRUE(client.getWasCalled());
  EXPECT_EQ(64, client.getTappedXPos());
  EXPECT_EQ(278, client.getTappedYPos());
  EXPECT_FALSE(client.isTappedNodeNull());
  EXPECT_TRUE(client.getWebNode().isTextNode());

  // Test basic tap handling and notification.
  client.reset();
  EXPECT_TRUE(
      tapElementById(WebInputEvent::GestureTap, WebString::fromUTF8("target")));
  EXPECT_TRUE(client.getWasCalled());
  EXPECT_EQ(144, client.getTappedXPos());
  EXPECT_EQ(82, client.getTappedYPos());
  EXPECT_FALSE(client.isTappedNodeNull());
  EXPECT_TRUE(client.getWebNode().isTextNode());
  // Make sure the returned text node has the parent element that was our
  // target.
  EXPECT_EQ(webView->mainFrame()->document().getElementById("target"),
            client.getWebNode().parentNode());

  // Test correct conversion of coordinates to viewport space under pinch-zoom.
  webView->setPageScaleFactor(2);
  webView->setVisualViewportOffset(WebFloatPoint(50, 20));
  client.reset();
  EXPECT_TRUE(
      tapElementById(WebInputEvent::GestureTap, WebString::fromUTF8("target")));
  EXPECT_TRUE(client.getWasCalled());
  EXPECT_EQ(188, client.getTappedXPos());
  EXPECT_EQ(124, client.getTappedYPos());

  m_webViewHelper.reset();  // Remove dependency on locally scoped client.
}

#define TEST_EACH_MOUSEEVENT(handler, EXPECT)                                 \
  frame->executeScript(WebScriptSource("setTest('mousedown-" handler "');")); \
  EXPECT_TRUE(tapElementById(WebInputEvent::GestureTap,                       \
                             WebString::fromUTF8("target")));                 \
  EXPECT_##EXPECT(client.getPageChanged());                                   \
  client.reset();                                                             \
  frame->executeScript(WebScriptSource("setTest('mouseup-" handler "');"));   \
  EXPECT_TRUE(tapElementById(WebInputEvent::GestureTap,                       \
                             WebString::fromUTF8("target")));                 \
  EXPECT_##EXPECT(client.getPageChanged());                                   \
  client.reset();                                                             \
  frame->executeScript(WebScriptSource("setTest('mousemove-" handler "');")); \
  EXPECT_TRUE(tapElementById(WebInputEvent::GestureTap,                       \
                             WebString::fromUTF8("target")));                 \
  EXPECT_##EXPECT(client.getPageChanged());                                   \
  client.reset();                                                             \
  frame->executeScript(WebScriptSource("setTest('click-" handler "');"));     \
  EXPECT_TRUE(tapElementById(WebInputEvent::GestureTap,                       \
                             WebString::fromUTF8("target")));                 \
  EXPECT_##EXPECT(client.getPageChanged());

TEST_P(WebViewTest, ShowUnhandledTapUIIfNeededWithMutateDom) {
  std::string testFile = "show_unhandled_tap.html";
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("Ahem.ttf"));
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(testFile));
  UnhandledTapWebViewClient client;
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + testFile, true, 0, &client);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();
  WebLocalFrameImpl* frame = webView->mainFrameImpl();

  // Test dom mutation.
  TEST_EACH_MOUSEEVENT("mutateDom", TRUE);

  // Test without any DOM mutation.
  client.reset();
  frame->executeScript(WebScriptSource("setTest('none');"));
  EXPECT_TRUE(
      tapElementById(WebInputEvent::GestureTap, WebString::fromUTF8("target")));
  EXPECT_FALSE(client.getPageChanged());

  m_webViewHelper.reset();  // Remove dependency on locally scoped client.
}

TEST_P(WebViewTest, ShowUnhandledTapUIIfNeededWithMutateStyle) {
  std::string testFile = "show_unhandled_tap.html";
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("Ahem.ttf"));
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(testFile));
  UnhandledTapWebViewClient client;
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + testFile, true, 0, &client);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();
  WebLocalFrameImpl* frame = webView->mainFrameImpl();

  // Test style mutation.
  TEST_EACH_MOUSEEVENT("mutateStyle", TRUE);

  // Test checkbox:indeterminate style mutation.
  TEST_EACH_MOUSEEVENT("mutateIndeterminate", TRUE);

  // Test click div with :active style but it is not covered for now.
  client.reset();
  EXPECT_TRUE(tapElementById(WebInputEvent::GestureTap,
                             WebString::fromUTF8("style_active")));
  EXPECT_FALSE(client.getPageChanged());

  // Test without any style mutation.
  client.reset();
  frame->executeScript(WebScriptSource("setTest('none');"));
  EXPECT_TRUE(
      tapElementById(WebInputEvent::GestureTap, WebString::fromUTF8("target")));
  EXPECT_FALSE(client.getPageChanged());

  m_webViewHelper.reset();  // Remove dependency on locally scoped client.
}

TEST_P(WebViewTest, ShowUnhandledTapUIIfNeededWithPreventDefault) {
  std::string testFile = "show_unhandled_tap.html";
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8("Ahem.ttf"));
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(testFile));
  UnhandledTapWebViewClient client;
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + testFile, true, 0, &client);
  webView->resize(WebSize(500, 300));
  webView->updateAllLifecyclePhases();
  runPendingTasks();
  WebLocalFrameImpl* frame = webView->mainFrameImpl();

  // Testswallowing.
  TEST_EACH_MOUSEEVENT("preventDefault", FALSE);

  // Test without any preventDefault.
  client.reset();
  frame->executeScript(WebScriptSource("setTest('none');"));
  EXPECT_TRUE(
      tapElementById(WebInputEvent::GestureTap, WebString::fromUTF8("target")));
  EXPECT_TRUE(client.getWasCalled());

  m_webViewHelper.reset();  // Remove dependency on locally scoped client.
}

TEST_P(WebViewTest, StopLoadingIfJavaScriptURLReturnsNoStringResult) {
  ViewCreatingWebViewClient client;
  FrameTestHelpers::WebViewHelper mainWebView;
  mainWebView.initializeAndLoad("about:blank", true, 0, &client);
  mainWebView.webView()
      ->page()
      ->settings()
      .setJavaScriptCanOpenWindowsAutomatically(true);

  WebFrame* frame = mainWebView.webView()->mainFrame();
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> v8Value =
      frame->executeScriptAndReturnValue(WebScriptSource(
          "var win = window.open('javascript:false'); win.document"));
  ASSERT_TRUE(v8Value->IsObject());
  Document* document =
      V8Document::toImplWithTypeCheck(v8::Isolate::GetCurrent(), v8Value);
  ASSERT_TRUE(document);
  EXPECT_FALSE(document->frame()->isLoading());
}

#if OS(MACOSX)
TEST_P(WebViewTest, WebSubstringUtil) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("content_editable_populated.html"));
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "content_editable_populated.html");
  webView->settings()->setDefaultFontSize(12);
  webView->resize(WebSize(400, 400));
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  FrameView* frameView = frame->frame()->view();

  WebPoint baselinePoint;
  NSAttributedString* result = WebSubstringUtil::attributedSubstringInRange(
      frame, 10, 3, &baselinePoint);
  ASSERT_TRUE(!!result);

  WebPoint point(baselinePoint.x, frameView->height() - baselinePoint.y);
  result = WebSubstringUtil::attributedWordAtPoint(frame->frameWidget(), point,
                                                   baselinePoint);
  ASSERT_TRUE(!!result);

  webView->setZoomLevel(3);

  result =
      WebSubstringUtil::attributedSubstringInRange(frame, 5, 5, &baselinePoint);
  ASSERT_TRUE(!!result);

  point = WebPoint(baselinePoint.x, frameView->height() - baselinePoint.y);
  result = WebSubstringUtil::attributedWordAtPoint(frame->frameWidget(), point,
                                                   baselinePoint);
  ASSERT_TRUE(!!result);
}

TEST_P(WebViewTest, WebSubstringUtilIframe) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("single_iframe.html"));
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("visible_iframe.html"));
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "single_iframe.html");
  webView->settings()->setDefaultFontSize(12);
  webView->settings()->setJavaScriptEnabled(true);
  webView->resize(WebSize(400, 400));
  WebLocalFrameImpl* mainFrame = webView->mainFrameImpl();
  WebLocalFrameImpl* childFrame = WebLocalFrameImpl::fromFrame(
      toLocalFrame(mainFrame->frame()->tree().firstChild()));

  WebPoint baselinePoint;
  NSAttributedString* result = WebSubstringUtil::attributedSubstringInRange(
      childFrame, 11, 7, &baselinePoint);
  ASSERT_NE(result, nullptr);

  WebPoint point(baselinePoint.x,
                 mainFrame->frameView()->height() - baselinePoint.y);
  result = WebSubstringUtil::attributedWordAtPoint(mainFrame->frameWidget(),
                                                   point, baselinePoint);
  ASSERT_NE(result, nullptr);

  int yBeforeChange = baselinePoint.y;

  // Now move the <iframe> down by 100px.
  mainFrame->executeScript(WebScriptSource(
      "document.querySelector('iframe').style.marginTop = '100px';"));

  point = WebPoint(point.x, point.y + 100);
  result = WebSubstringUtil::attributedWordAtPoint(mainFrame->frameWidget(),
                                                   point, baselinePoint);
  ASSERT_NE(result, nullptr);

  EXPECT_EQ(yBeforeChange, baselinePoint.y + 100);
}

#endif

TEST_P(WebViewTest, PasswordFieldEditingIsUserGesture) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("input_field_password.html"));
  MockAutofillClient client;
  WebViewImpl* webView = m_webViewHelper.initializeAndLoad(
      m_baseURL + "input_field_password.html", true);
  WebLocalFrameImpl* frame = webView->mainFrameImpl();
  frame->setAutofillClient(&client);
  webView->setInitialFocus(false);

  EXPECT_TRUE(
      frame->frameWidget()->getActiveWebInputMethodController()->commitText(
          WebString::fromUTF8(std::string("hello").c_str()), 0));
  EXPECT_EQ(1, client.textChangesFromUserGesture());
  EXPECT_FALSE(UserGestureIndicator::processingUserGesture());
  frame->setAutofillClient(0);
}

// Verify that a WebView created with a ScopedPageSuspender already on the
// stack defers its loads.
TEST_P(WebViewTest, CreatedDuringPageSuspension) {
  {
    WebViewImpl* webView = m_webViewHelper.initialize();
    EXPECT_FALSE(webView->page()->suspended());
  }

  {
    ScopedPageSuspender suspender;
    WebViewImpl* webView = m_webViewHelper.initialize();
    EXPECT_TRUE(webView->page()->suspended());
  }
}

// Make sure the SubframeBeforeUnloadUseCounter is only incremented on subframe
// unloads. crbug.com/635029.
TEST_P(WebViewTest, SubframeBeforeUnloadUseCounter) {
  registerMockedHttpURLLoad("visible_iframe.html");
  registerMockedHttpURLLoad("single_iframe.html");
  WebViewImpl* webView =
      m_webViewHelper.initializeAndLoad(m_baseURL + "single_iframe.html", true);

  WebFrame* frame = m_webViewHelper.webView()->mainFrame();
  Document* document =
      toLocalFrame(m_webViewHelper.webView()->page()->mainFrame())->document();

  // Add a beforeunload handler in the main frame. Make sure firing
  // beforeunload doesn't increment the subframe use counter.
  {
    frame->executeScript(
        WebScriptSource("addEventListener('beforeunload', function() {});"));
    webView->mainFrame()->toWebLocalFrame()->dispatchBeforeUnloadEvent(false);
    EXPECT_FALSE(UseCounter::isCounted(*document,
                                       UseCounter::SubFrameBeforeUnloadFired));
  }

  // Add a beforeunload handler in the iframe and dispatch. Make sure we do
  // increment the use counter for subframe beforeunloads.
  {
    frame->executeScript(WebScriptSource(
        "document.getElementsByTagName('iframe')[0].contentWindow."
        "addEventListener('beforeunload', function() {});"));
    webView->mainFrame()
        ->firstChild()
        ->toWebLocalFrame()
        ->dispatchBeforeUnloadEvent(false);

    Document* childDocument =
        toLocalFrame(
            m_webViewHelper.webView()->page()->mainFrame()->tree().firstChild())
            ->document();
    EXPECT_TRUE(UseCounter::isCounted(*childDocument,
                                      UseCounter::SubFrameBeforeUnloadFired));
  }
}

// Verify that page loads are deferred until all ScopedPageLoadDeferrers are
// destroyed.
TEST_P(WebViewTest, NestedPageSuspensions) {
  WebViewImpl* webView = m_webViewHelper.initialize();
  EXPECT_FALSE(webView->page()->suspended());

  {
    ScopedPageSuspender suspender;
    EXPECT_TRUE(webView->page()->suspended());

    {
      ScopedPageSuspender suspender2;
      EXPECT_TRUE(webView->page()->suspended());
    }

    EXPECT_TRUE(webView->page()->suspended());
  }

  EXPECT_FALSE(webView->page()->suspended());
}

TEST_P(WebViewTest, ForceAndResetViewport) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("200-by-300.html"));
  WebViewImpl* webViewImpl =
      m_webViewHelper.initializeAndLoad(m_baseURL + "200-by-300.html");
  webViewImpl->resize(WebSize(100, 150));
  webViewImpl->layerTreeView()->setViewportSize(WebSize(100, 150));
  VisualViewport* visualViewport =
      &webViewImpl->page()->frameHost().visualViewport();
  DevToolsEmulator* devToolsEmulator = webViewImpl->devToolsEmulator();

  TransformationMatrix expectedMatrix;
  expectedMatrix.makeIdentity();
  EXPECT_EQ(expectedMatrix,
            webViewImpl->getDeviceEmulationTransformForTesting());
  EXPECT_FALSE(devToolsEmulator->visibleContentRectForPainting());
  EXPECT_TRUE(visualViewport->containerLayer()->masksToBounds());

  // Override applies transform, sets visibleContentRect, and disables
  // visual viewport clipping.
  devToolsEmulator->forceViewport(WebFloatPoint(50, 55), 2.f);
  expectedMatrix.makeIdentity().scale(2.f).translate(-50, -55);
  EXPECT_EQ(expectedMatrix,
            webViewImpl->getDeviceEmulationTransformForTesting());
  EXPECT_EQ(IntRect(50, 55, 50, 75),
            *devToolsEmulator->visibleContentRectForPainting());
  EXPECT_FALSE(visualViewport->containerLayer()->masksToBounds());

  // Setting new override discards previous one.
  devToolsEmulator->forceViewport(WebFloatPoint(5.4f, 10.5f), 1.5f);
  expectedMatrix.makeIdentity().scale(1.5f).translate(-5.4f, -10.5f);
  EXPECT_EQ(expectedMatrix,
            webViewImpl->getDeviceEmulationTransformForTesting());
  EXPECT_EQ(IntRect(5, 10, 68, 101),
            *devToolsEmulator->visibleContentRectForPainting());
  EXPECT_FALSE(visualViewport->containerLayer()->masksToBounds());

  // Clearing override restores original transform, visibleContentRect and
  // visual viewport clipping.
  devToolsEmulator->resetViewport();
  expectedMatrix.makeIdentity();
  EXPECT_EQ(expectedMatrix,
            webViewImpl->getDeviceEmulationTransformForTesting());
  EXPECT_FALSE(devToolsEmulator->visibleContentRectForPainting());
  EXPECT_TRUE(visualViewport->containerLayer()->masksToBounds());
}

TEST_P(WebViewTest, ViewportOverrideIntegratesDeviceMetricsOffsetAndScale) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("200-by-300.html"));
  WebViewImpl* webViewImpl =
      m_webViewHelper.initializeAndLoad(m_baseURL + "200-by-300.html");
  webViewImpl->resize(WebSize(100, 150));

  TransformationMatrix expectedMatrix;
  expectedMatrix.makeIdentity();
  EXPECT_EQ(expectedMatrix,
            webViewImpl->getDeviceEmulationTransformForTesting());

  WebDeviceEmulationParams emulationParams;
  emulationParams.offset = WebFloatPoint(50, 50);
  emulationParams.scale = 2.f;
  webViewImpl->enableDeviceEmulation(emulationParams);
  expectedMatrix.makeIdentity().translate(50, 50).scale(2.f);
  EXPECT_EQ(expectedMatrix,
            webViewImpl->getDeviceEmulationTransformForTesting());

  // Device metrics offset and scale are applied before viewport override.
  webViewImpl->devToolsEmulator()->forceViewport(WebFloatPoint(5, 10), 1.5f);
  expectedMatrix.makeIdentity()
      .scale(1.5f)
      .translate(-5, -10)
      .translate(50, 50)
      .scale(2.f);
  EXPECT_EQ(expectedMatrix,
            webViewImpl->getDeviceEmulationTransformForTesting());
}

TEST_P(WebViewTest, ViewportOverrideAdaptsToScaleAndScroll) {
  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(m_baseURL.c_str()),
      WebString::fromUTF8("200-by-300.html"));
  WebViewImpl* webViewImpl =
      m_webViewHelper.initializeAndLoad(m_baseURL + "200-by-300.html");
  webViewImpl->resize(WebSize(100, 150));
  webViewImpl->layerTreeView()->setViewportSize(WebSize(100, 150));
  FrameView* frameView = webViewImpl->mainFrameImpl()->frame()->view();
  DevToolsEmulator* devToolsEmulator = webViewImpl->devToolsEmulator();

  TransformationMatrix expectedMatrix;
  expectedMatrix.makeIdentity();
  EXPECT_EQ(expectedMatrix,
            webViewImpl->getDeviceEmulationTransformForTesting());

  // Initial transform takes current page scale and scroll position into
  // account.
  webViewImpl->setPageScaleFactor(1.5f);
  frameView->layoutViewportScrollableArea()->setScrollOffset(
      ScrollOffset(100, 150), ProgrammaticScroll, ScrollBehaviorInstant);
  devToolsEmulator->forceViewport(WebFloatPoint(50, 55), 2.f);
  expectedMatrix.makeIdentity()
      .scale(2.f)
      .translate(-50, -55)
      .translate(100, 150)
      .scale(1. / 1.5f);
  EXPECT_EQ(expectedMatrix,
            webViewImpl->getDeviceEmulationTransformForTesting());
  // Page scroll and scale are irrelevant for visibleContentRect.
  EXPECT_EQ(IntRect(50, 55, 50, 75),
            *devToolsEmulator->visibleContentRectForPainting());

  // Transform adapts to scroll changes.
  frameView->layoutViewportScrollableArea()->setScrollOffset(
      ScrollOffset(50, 55), ProgrammaticScroll, ScrollBehaviorInstant);
  expectedMatrix.makeIdentity()
      .scale(2.f)
      .translate(-50, -55)
      .translate(50, 55)
      .scale(1. / 1.5f);
  EXPECT_EQ(expectedMatrix,
            webViewImpl->getDeviceEmulationTransformForTesting());
  // visibleContentRect doesn't change.
  EXPECT_EQ(IntRect(50, 55, 50, 75),
            *devToolsEmulator->visibleContentRectForPainting());

  // Transform adapts to page scale changes.
  webViewImpl->setPageScaleFactor(2.f);
  expectedMatrix.makeIdentity()
      .scale(2.f)
      .translate(-50, -55)
      .translate(50, 55)
      .scale(1. / 2.f);
  EXPECT_EQ(expectedMatrix,
            webViewImpl->getDeviceEmulationTransformForTesting());
  // visibleContentRect doesn't change.
  EXPECT_EQ(IntRect(50, 55, 50, 75),
            *devToolsEmulator->visibleContentRectForPainting());
}

}  // namespace blink
