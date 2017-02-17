/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "core/layout/LayoutGeometryMap.h"

#include "core/dom/Document.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebFrameClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"

namespace blink {

typedef bool TestParamRootLayerScrolling;
class LayoutGeometryMapTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest {
 public:
  LayoutGeometryMapTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        m_baseURL("http://www.test.com/") {}

  void TearDown() override {
    Platform::current()
        ->getURLLoaderMockFactory()
        ->unregisterAllURLsAndClearMemoryCache();
  }

 protected:
  static LayoutBox* getFrameElement(const char* iframeName,
                                    WebView* webView,
                                    const WTF::AtomicString& elementId) {
    WebLocalFrameImpl* iframe = toWebLocalFrameImpl(
        webView->findFrameByName(WebString::fromUTF8(iframeName)));
    if (!iframe)
      return nullptr;
    LocalFrame* frame = iframe->frame();
    Document* doc = frame->document();
    Element* element = doc->getElementById(elementId);
    if (!element)
      return nullptr;
    return element->layoutBox();
  }

  static Element* getElement(WebView* webView,
                             const WTF::AtomicString& elementId) {
    WebViewImpl* webViewImpl = toWebViewImpl(webView);
    if (!webViewImpl)
      return nullptr;
    LocalFrame* frame = webViewImpl->mainFrameImpl()->frame();
    Document* doc = frame->document();
    return doc->getElementById(elementId);
  }

  static LayoutBox* getLayoutBox(WebView* webView,
                                 const WTF::AtomicString& elementId) {
    Element* element = getElement(webView, elementId);
    if (!element)
      return nullptr;
    return element->layoutBox();
  }

  static const LayoutBoxModelObject* getLayoutContainer(
      WebView* webView,
      const WTF::AtomicString& elementId) {
    LayoutBox* rb = getLayoutBox(webView, elementId);
    if (!rb)
      return nullptr;
    PaintLayer* compositingLayer =
        rb->enclosingLayer()->enclosingLayerForPaintInvalidation();
    if (!compositingLayer)
      return nullptr;
    return &compositingLayer->layoutObject();
  }

  static const LayoutBoxModelObject* getFrameLayoutContainer(
      const char* frameId,
      WebView* webView,
      const WTF::AtomicString& elementId) {
    LayoutBox* rb = getFrameElement(frameId, webView, elementId);
    if (!rb)
      return nullptr;
    PaintLayer* compositingLayer =
        rb->enclosingLayer()->enclosingLayerForPaintInvalidation();
    if (!compositingLayer)
      return nullptr;
    return &compositingLayer->layoutObject();
  }

  static const FloatRect rectFromQuad(const FloatQuad& quad) {
    FloatRect rect;
    rect.setX(std::min(
        quad.p1().x(),
        std::min(quad.p2().x(), std::min(quad.p3().x(), quad.p4().x()))));
    rect.setY(std::min(
        quad.p1().y(),
        std::min(quad.p2().y(), std::min(quad.p3().y(), quad.p4().y()))));

    rect.setWidth(std::max(quad.p1().x(),
                           std::max(quad.p2().x(),
                                    std::max(quad.p3().x(), quad.p4().x()))) -
                  rect.x());
    rect.setHeight(std::max(quad.p1().y(),
                            std::max(quad.p2().y(),
                                     std::max(quad.p3().y(), quad.p4().y()))) -
                   rect.y());
    return rect;
  }

  // Adjust rect by the scroll offset of the LayoutView.  This only has an
  // effect if root layer scrolling is enabled.  The only reason for doing
  // this here is so the test expected values can be the same whether or not
  // root layer scrolling is enabled.  For more context, see:
  // https://codereview.chromium.org/2417103002/#msg11
  static FloatRect adjustForFrameScroll(WebView* webView,
                                        const FloatRect& rect) {
    FloatRect result(rect);
    LocalFrame* frame = toWebViewImpl(webView)->mainFrameImpl()->frame();
    LayoutView* layoutView = frame->document()->layoutView();
    if (layoutView->hasOverflowClip())
      result.move(layoutView->scrolledContentOffset());
    return result;
  }

  void registerMockedHttpURLLoad(const std::string& fileName) {
    URLTestHelpers::registerMockedURLLoadFromBase(
        WebString::fromUTF8(m_baseURL), testing::webTestDataPath(),
        WebString::fromUTF8(fileName));
  }

  const std::string m_baseURL;
  FrameTestHelpers::TestWebFrameClient m_mockWebViewClient;
};

INSTANTIATE_TEST_CASE_P(All, LayoutGeometryMapTest, ::testing::Bool());

TEST_P(LayoutGeometryMapTest, SimpleGeometryMapTest) {
  registerMockedHttpURLLoad("rgm_test.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView =
      webViewHelper.initializeAndLoad(m_baseURL + "rgm_test.html", true, 0, 0);
  webView->resize(WebSize(1000, 1000));
  webView->updateAllLifecyclePhases();

  // We are going test everything twice. Once with FloatPoints and once with
  // FloatRects. This is because LayoutGeometryMap treats both slightly
  // differently
  LayoutGeometryMap rgm;
  rgm.pushMappingsToAncestor(getLayoutBox(webView, "InitialDiv"), 0);
  FloatRect rect(0.0f, 0.0f, 1.0f, 2.0f);
  EXPECT_EQ(FloatQuad(FloatRect(8.0f, 8.0f, 1.0f, 2.0f)),
            rgm.mapToAncestor(rect, nullptr));

  rgm.popMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  EXPECT_EQ(FloatQuad(FloatRect(0.0f, 0.0f, 1.0f, 2.0f)),
            rgm.mapToAncestor(rect, nullptr));

  rgm.pushMappingsToAncestor(getLayoutBox(webView, "InnerDiv"), 0);
  EXPECT_EQ(FloatQuad(FloatRect(21.0f, 6.0f, 1.0f, 2.0f)),
            rgm.mapToAncestor(rect, getLayoutBox(webView, "CenterDiv")));

  rgm.pushMappingsToAncestor(getLayoutBox(webView, "OtherDiv"),
                             getLayoutBox(webView, "InnerDiv"));
  EXPECT_EQ(FloatQuad(FloatRect(22.0f, 12.0f, 1.0f, 2.0f)),
            rgm.mapToAncestor(rect, getLayoutBox(webView, "CenterDiv")));

  EXPECT_EQ(FloatQuad(FloatRect(1.0f, 6.0f, 1.0f, 2.0f)),
            rgm.mapToAncestor(rect, getLayoutBox(webView, "InnerDiv")));

  EXPECT_EQ(FloatQuad(FloatRect(50.0f, 44.0f, 1.0f, 2.0f)),
            rgm.mapToAncestor(rect, nullptr));
}

// Fails on Windows due to crbug.com/391457. When run through the transform the
// position on windows differs by a pixel
#if OS(WIN)
TEST_P(LayoutGeometryMapTest, DISABLED_TransformedGeometryTest)
#else
TEST_P(LayoutGeometryMapTest, TransformedGeometryTest)
#endif
{
  registerMockedHttpURLLoad("rgm_transformed_test.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "rgm_transformed_test.html", true, 0, 0);
  webView->resize(WebSize(1000, 1000));
  webView->updateAllLifecyclePhases();

  LayoutGeometryMap rgm;
  rgm.pushMappingsToAncestor(getLayoutBox(webView, "InitialDiv"), 0);
  const float rectWidth = 15.0f;
  const float scaleWidth = 2.0f;
  const float scaleHeight = 3.0f;
  FloatRect rect(0.0f, 0.0f, 15.0f, 25.0f);
  EXPECT_EQ(FloatQuad(FloatRect(8.0f, 8.0f, 15.0f, 25.0f)),
            rgm.mapToAncestor(rect, nullptr));

  rgm.popMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  EXPECT_EQ(FloatQuad(FloatRect(0.0f, 0.0f, 15.0f, 25.0f)).boundingBox(),
            rgm.mapToAncestor(rect, nullptr).boundingBox());

  rgm.pushMappingsToAncestor(getLayoutBox(webView, "InnerDiv"), 0);
  EXPECT_EQ(FloatQuad(FloatRect(523.0f - rectWidth, 6.0f, 15.0f, 25.0f))
                .boundingBox(),
            rgm.mapToAncestor(rect, getLayoutBox(webView, "CenterDiv"))
                .boundingBox());

  rgm.pushMappingsToAncestor(getLayoutBox(webView, "OtherDiv"),
                             getLayoutBox(webView, "InnerDiv"));
  EXPECT_EQ(FloatQuad(FloatRect(522.0f - rectWidth, 12.0f, 15.0f, 25.0f))
                .boundingBox(),
            rgm.mapToAncestor(rect, getLayoutBox(webView, "CenterDiv"))
                .boundingBox());

  EXPECT_EQ(
      FloatQuad(FloatRect(1.0f, 6.0f, 15.0f, 25.0f)).boundingBox(),
      rgm.mapToAncestor(rect, getLayoutBox(webView, "InnerDiv")).boundingBox());

  EXPECT_EQ(FloatQuad(FloatRect(821.0f - rectWidth * scaleWidth, 31.0f,
                                15.0f * scaleWidth, 25.0f * scaleHeight))
                .boundingBox(),
            rgm.mapToAncestor(rect, nullptr).boundingBox());

  rect = FloatRect(10.0f, 25.0f, 15.0f, 25.0f);
  EXPECT_EQ(FloatQuad(FloatRect(512.0f - rectWidth, 37.0f, 15.0f, 25.0f))
                .boundingBox(),
            rgm.mapToAncestor(rect, getLayoutBox(webView, "CenterDiv"))
                .boundingBox());

  EXPECT_EQ(
      FloatQuad(FloatRect(11.0f, 31.0f, 15.0f, 25.0f)).boundingBox(),
      rgm.mapToAncestor(rect, getLayoutBox(webView, "InnerDiv")).boundingBox());

  EXPECT_EQ(FloatQuad(FloatRect(801.0f - rectWidth * scaleWidth, 106.0f,
                                15.0f * scaleWidth, 25.0f * scaleHeight))
                .boundingBox(),
            rgm.mapToAncestor(rect, nullptr).boundingBox());
}

TEST_P(LayoutGeometryMapTest, FixedGeometryTest) {
  registerMockedHttpURLLoad("rgm_fixed_position_test.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "rgm_fixed_position_test.html", true, 0, 0);
  webView->resize(WebSize(1000, 1000));
  webView->updateAllLifecyclePhases();

  LayoutGeometryMap rgm;
  rgm.pushMappingsToAncestor(getLayoutBox(webView, "InitialDiv"), 0);
  FloatRect rect(0.0f, 0.0f, 15.0f, 25.0f);
  EXPECT_EQ(FloatQuad(FloatRect(8.0f, 8.0f, 15.0f, 25.0f)),
            rgm.mapToAncestor(rect, nullptr));

  rgm.popMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  EXPECT_EQ(FloatQuad(FloatRect(0.0f, 0.0f, 15.0f, 25.0f)),
            rgm.mapToAncestor(rect, nullptr));

  rgm.pushMappingsToAncestor(getLayoutBox(webView, "InnerDiv"), 0);
  EXPECT_EQ(FloatQuad(FloatRect(20.0f, 14.0f, 15.0f, 25.0f)),
            rgm.mapToAncestor(rect, nullptr));

  rgm.pushMappingsToAncestor(getLayoutBox(webView, "OtherDiv"),
                             getLayoutBox(webView, "InnerDiv"));
  EXPECT_EQ(FloatQuad(FloatRect(21.0f, 20.0f, 15.0f, 25.0f)),
            rgm.mapToAncestor(rect, getLayoutContainer(webView, "CenterDiv")));

  rect = FloatRect(22.0f, 15.2f, 15.3f, 0.0f);
  EXPECT_EQ(FloatQuad(FloatRect(43.0f, 35.2f, 15.3f, 0.0f)),
            rgm.mapToAncestor(rect, getLayoutContainer(webView, "CenterDiv")));

  EXPECT_EQ(FloatQuad(FloatRect(43.0f, 35.2f, 15.3f, 0.0f)),
            rgm.mapToAncestor(rect, getLayoutContainer(webView, "InnerDiv")));

  EXPECT_EQ(FloatQuad(FloatRect(43.0f, 35.2f, 15.3f, 0.0f)),
            rgm.mapToAncestor(rect, nullptr));
}

TEST_P(LayoutGeometryMapTest, ContainsFixedPositionTest) {
  registerMockedHttpURLLoad("rgm_contains_fixed_position_test.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "rgm_contains_fixed_position_test.html", true, 0, 0);
  webView->resize(WebSize(1000, 1000));
  webView->updateAllLifecyclePhases();

  FloatRect rect(0.0f, 0.0f, 100.0f, 100.0f);
  LayoutGeometryMap rgm;

  // This fixed position element is not contained and so is attached at the top
  // of the viewport.
  rgm.pushMappingsToAncestor(getLayoutBox(webView, "simple-container"), 0);
  EXPECT_EQ(FloatRect(8.0f, 100.0f, 100.0f, 100.0f),
            adjustForFrameScroll(webView, rgm.absoluteRect(rect)));
  rgm.pushMappingsToAncestor(getLayoutBox(webView, "fixed1"),
                             getLayoutBox(webView, "simple-container"));
  EXPECT_EQ(FloatRect(8.0f, 50.0f, 100.0f, 100.0f),
            adjustForFrameScroll(webView, rgm.absoluteRect(rect)));
  rgm.popMappingsToAncestor(static_cast<PaintLayer*>(nullptr));

  // Transforms contain fixed position descendants.
  rgm.pushMappingsToAncestor(getLayoutBox(webView, "has-transform"), 0);
  EXPECT_EQ(FloatRect(8.0f, 100.0f, 100.0f, 100.0f),
            adjustForFrameScroll(webView, rgm.absoluteRect(rect)));
  rgm.pushMappingsToAncestor(getLayoutBox(webView, "fixed2"),
                             getLayoutBox(webView, "has-transform"));
  EXPECT_EQ(FloatRect(8.0f, 100.0f, 100.0f, 100.0f),
            adjustForFrameScroll(webView, rgm.absoluteRect(rect)));
  rgm.popMappingsToAncestor(static_cast<PaintLayer*>(nullptr));

  // Paint containment contains fixed position descendants.
  rgm.pushMappingsToAncestor(getLayoutBox(webView, "contains-paint"), 0);
  EXPECT_EQ(FloatRect(8.0f, 100.0f, 100.0f, 100.0f),
            adjustForFrameScroll(webView, rgm.absoluteRect(rect)));
  rgm.pushMappingsToAncestor(getLayoutBox(webView, "fixed3"),
                             getLayoutBox(webView, "contains-paint"));
  EXPECT_EQ(FloatRect(8.0f, 100.0f, 100.0f, 100.0f),
            adjustForFrameScroll(webView, rgm.absoluteRect(rect)));
  rgm.popMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
}

TEST_P(LayoutGeometryMapTest, IframeTest) {
  registerMockedHttpURLLoad("rgm_iframe_test.html");
  registerMockedHttpURLLoad("rgm_test.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "rgm_iframe_test.html", true, 0, 0);
  webView->resize(WebSize(1000, 1000));
  webView->updateAllLifecyclePhases();

  LayoutGeometryMap rgm(TraverseDocumentBoundaries);
  LayoutGeometryMap rgmNoFrame;

  rgmNoFrame.pushMappingsToAncestor(
      getFrameElement("test_frame", webView, "InitialDiv"), 0);
  rgm.pushMappingsToAncestor(
      getFrameElement("test_frame", webView, "InitialDiv"), 0);
  FloatRect rect(0.0f, 0.0f, 1.0f, 2.0f);

  EXPECT_EQ(FloatQuad(FloatRect(8.0f, 8.0f, 1.0f, 2.0f)),
            rgmNoFrame.mapToAncestor(rect, nullptr));

  // Our initial rect looks like: (0, 0, 1, 2)
  //        p0_____
  //          |   |
  //          |   |
  //          |   |
  //          |___|
  // When we rotate we get a rect of the form:
  //         p0_
  //          / -_
  //         /   /
  //        /   /
  //         -_/
  // So it is sensible that the minimum y is the same as for a point at 0, 0.
  // The minimum x should be p0.x - 2 * sin(30deg) = p0.x - 1.
  // That maximum x should likewise be p0.x + cos(30deg) = p0.x + 0.866.
  // And the maximum y should be p0.x + sin(30deg) + 2*cos(30deg)
  //      = p0.y + 2.232.
  EXPECT_NEAR(69.5244f, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).x(),
              0.0001f);
  EXPECT_NEAR(-44.0237, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).y(),
              0.0001f);
  EXPECT_NEAR(1.866, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).width(),
              0.0001f);
  EXPECT_NEAR(2.232, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).height(),
              0.0001f);

  rgm.popMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  rgmNoFrame.popMappingsToAncestor(static_cast<PaintLayer*>(nullptr));

  rgm.pushMappingsToAncestor(getFrameElement("test_frame", webView, "InnerDiv"),
                             0);
  rgmNoFrame.pushMappingsToAncestor(
      getFrameElement("test_frame", webView, "InnerDiv"), 0);
  EXPECT_EQ(FloatQuad(FloatRect(21.0f, 6.0f, 1.0f, 2.0f)),
            rgm.mapToAncestor(rect, getFrameLayoutContainer(
                                        "test_frame", webView, "CenterDiv")));
  EXPECT_EQ(
      FloatQuad(FloatRect(21.0f, 6.0f, 1.0f, 2.0f)),
      rgmNoFrame.mapToAncestor(
          rect, getFrameLayoutContainer("test_frame", webView, "CenterDiv")));

  rgm.pushMappingsToAncestor(
      getFrameElement("test_frame", webView, "OtherDiv"),
      getFrameLayoutContainer("test_frame", webView, "InnerDiv"));
  rgmNoFrame.pushMappingsToAncestor(
      getFrameElement("test_frame", webView, "OtherDiv"),
      getFrameLayoutContainer("test_frame", webView, "InnerDiv"));
  EXPECT_EQ(FloatQuad(FloatRect(22.0f, 12.0f, 1.0f, 2.0f)),
            rgm.mapToAncestor(rect, getFrameLayoutContainer(
                                        "test_frame", webView, "CenterDiv")));
  EXPECT_EQ(
      FloatQuad(FloatRect(22.0f, 12.0f, 1.0f, 2.0f)),
      rgmNoFrame.mapToAncestor(
          rect, getFrameLayoutContainer("test_frame", webView, "CenterDiv")));

  EXPECT_EQ(FloatQuad(FloatRect(1.0f, 6.0f, 1.0f, 2.0f)),
            rgm.mapToAncestor(rect, getFrameLayoutContainer(
                                        "test_frame", webView, "InnerDiv")));
  EXPECT_EQ(
      FloatQuad(FloatRect(1.0f, 6.0f, 1.0f, 2.0f)),
      rgmNoFrame.mapToAncestor(
          rect, getFrameLayoutContainer("test_frame", webView, "InnerDiv")));

  EXPECT_NEAR(87.8975f, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).x(),
              0.0001f);
  EXPECT_NEAR(8.1532f, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).y(),
              0.0001f);
  EXPECT_NEAR(1.866, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).width(),
              0.0001f);
  EXPECT_NEAR(2.232, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).height(),
              0.0001f);

  EXPECT_EQ(FloatQuad(FloatRect(50.0f, 44.0f, 1.0f, 2.0f)),
            rgmNoFrame.mapToAncestor(rect, nullptr));
}

TEST_P(LayoutGeometryMapTest, ColumnTest) {
  registerMockedHttpURLLoad("rgm_column_test.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "rgm_column_test.html", true, 0, 0);
  webView->resize(WebSize(1000, 1000));
  webView->updateAllLifecyclePhases();

  // The document is 1000f wide (we resized to that size).
  // We have a 8px margin on either side of the document.
  // Between each column we have a 10px gap, and we have 3 columns.
  // The width of a given column is (1000 - 16 - 20)/3.
  // The total offset between any column and it's neighbour is width + 10px
  // (for the gap).
  float offset = (1000.0f - 16.0f - 20.0f) / 3.0f + 10.0f;

  LayoutGeometryMap rgm;
  rgm.pushMappingsToAncestor(getLayoutBox(webView, "A"), 0);
  FloatPoint point;
  FloatRect rect(0.0f, 0.0f, 5.0f, 3.0f);

  EXPECT_EQ(FloatQuad(FloatRect(8.0f, 8.0f, 5.0f, 3.0f)),
            rgm.mapToAncestor(rect, nullptr));

  rgm.popMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  EXPECT_EQ(FloatQuad(FloatRect(0.0f, 0.0f, 5.0f, 3.0f)),
            rgm.mapToAncestor(rect, nullptr));

  rgm.pushMappingsToAncestor(getLayoutBox(webView, "Col1"), 0);
  EXPECT_EQ(FloatQuad(FloatRect(8.0f, 8.0f, 5.0f, 3.0f)),
            rgm.mapToAncestor(rect, nullptr));

  rgm.popMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  rgm.pushMappingsToAncestor(getLayoutBox(webView, "Col2"), nullptr);
  EXPECT_NEAR(8.0f + offset, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).x(),
              0.1f);
  EXPECT_NEAR(8.0f, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).y(), 0.1f);
  EXPECT_EQ(5.0f, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).width());
  EXPECT_EQ(3.0f, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).height());

  rgm.popMappingsToAncestor(static_cast<PaintLayer*>(nullptr));
  rgm.pushMappingsToAncestor(getLayoutBox(webView, "Col3"), nullptr);
  EXPECT_NEAR(8.0f + offset * 2.0f,
              rectFromQuad(rgm.mapToAncestor(rect, nullptr)).x(), 0.1f);
  EXPECT_NEAR(8.0f, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).y(), 0.1f);
  EXPECT_EQ(5.0f, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).width());
  EXPECT_EQ(3.0f, rectFromQuad(rgm.mapToAncestor(rect, nullptr)).height());
}

TEST_P(LayoutGeometryMapTest, FloatUnderInlineLayer) {
  registerMockedHttpURLLoad("rgm_float_under_inline.html");
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebView* webView = webViewHelper.initializeAndLoad(
      m_baseURL + "rgm_float_under_inline.html", true, 0, 0);
  webView->resize(WebSize(1000, 1000));
  webView->updateAllLifecyclePhases();

  LayoutGeometryMap rgm;
  auto* layerUnderFloat = getLayoutBox(webView, "layer-under-float");
  auto* span = getElement(webView, "span")->layoutBoxModelObject();
  auto* floating = getLayoutBox(webView, "float");
  auto* container = getLayoutBox(webView, "container");
  FloatRect rect(3.0f, 4.0f, 10.0f, 8.0f);

  rgm.pushMappingsToAncestor(container->layer(), nullptr);
  rgm.pushMappingsToAncestor(span->layer(), container->layer());
  rgm.pushMappingsToAncestor(layerUnderFloat->layer(), span->layer());
  EXPECT_EQ(rect, rectFromQuad(rgm.mapToAncestor(rect, container)));
  EXPECT_EQ(FloatRect(63.0f, 54.0f, 10.0f, 8.0f),
            rectFromQuad(rgm.mapToAncestor(rect, nullptr)));

  rgm.popMappingsToAncestor(span->layer());
  EXPECT_EQ(FloatRect(203.0f, 104.0f, 10.0f, 8.0f),
            rectFromQuad(rgm.mapToAncestor(rect, container)));
  EXPECT_EQ(FloatRect(263.0f, 154.0f, 10.0f, 8.0f),
            rectFromQuad(rgm.mapToAncestor(rect, nullptr)));

  rgm.pushMappingsToAncestor(floating, span);
  EXPECT_EQ(rect, rectFromQuad(rgm.mapToAncestor(rect, container)));
  EXPECT_EQ(FloatRect(63.0f, 54.0f, 10.0f, 8.0f),
            rectFromQuad(rgm.mapToAncestor(rect, nullptr)));
}

}  // namespace blink
