// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <memory>
#include "core/exported/WebViewBase.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/Page.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/graphics/CompositorMutation.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebSettings.h"
#include "public/web/WebViewClient.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

typedef bool TestParamRootLayerScrolling;
class CompositorWorkerTest
    : public ::testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedRootLayerScrollingForTest,
      private ScopedCompositorWorkerForTest,
      public ::testing::Test {
 public:
  CompositorWorkerTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        ScopedCompositorWorkerForTest(true),
        base_url_("http://www.test.com/") {}

  void SetUp() override {
    helper_.Initialize(nullptr, nullptr, nullptr, &ConfigureSettings);
    GetWebView()->Resize(IntSize(320, 240));
  }

  ~CompositorWorkerTest() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void NavigateTo(const String& url) {
    FrameTestHelpers::LoadFrame(GetWebView()->MainFrameImpl(),
                                url.Utf8().data());
  }

  void ForceFullCompositingUpdate() {
    GetWebView()->UpdateAllLifecyclePhases();
  }

  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        base_url_, testing::WebTestDataPath(), WebString::FromUTF8(file_name));
  }

  WebLayer* GetRootScrollLayer() {
    if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
      DCHECK(GetFrame());
      DCHECK(GetFrame()->View());
      DCHECK(GetFrame()->View()->LayoutViewportScrollableArea());
      DCHECK(GetFrame()
                 ->View()
                 ->LayoutViewportScrollableArea()
                 ->LayerForScrolling());
      return GetFrame()
          ->View()
          ->LayoutViewportScrollableArea()
          ->LayerForScrolling()
          ->PlatformLayer();
    }
    PaintLayerCompositor* compositor =
        GetFrame()->ContentLayoutItem().Compositor();
    DCHECK(compositor);
    DCHECK(compositor->ScrollLayer());

    WebLayer* web_scroll_layer = compositor->ScrollLayer()->PlatformLayer();
    return web_scroll_layer;
  }

  WebViewBase* GetWebView() const { return helper_.WebView(); }
  LocalFrame* GetFrame() const {
    return helper_.WebView()->MainFrameImpl()->GetFrame();
  }

 protected:
  String base_url_;

 private:
  static void ConfigureSettings(WebSettings* settings) {
    settings->SetAcceleratedCompositingEnabled(true);
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  FrameTestHelpers::WebViewHelper helper_;
  FrameTestHelpers::UseMockScrollbarSettings mock_scrollbar_settings_;
};

static CompositedLayerMapping* MappingFromElement(Element* element) {
  if (!element)
    return nullptr;
  LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object || !layout_object->IsBoxModelObject())
    return nullptr;
  PaintLayer* layer = ToLayoutBoxModelObject(layout_object)->Layer();
  if (!layer)
    return nullptr;
  if (!layer->HasCompositedLayerMapping())
    return nullptr;
  return layer->GetCompositedLayerMapping();
}

static WebLayer* WebLayerFromGraphicsLayer(GraphicsLayer* graphics_layer) {
  if (!graphics_layer)
    return nullptr;
  return graphics_layer->PlatformLayer();
}

static WebLayer* ScrollingWebLayerFromElement(Element* element) {
  CompositedLayerMapping* composited_layer_mapping =
      MappingFromElement(element);
  if (!composited_layer_mapping)
    return nullptr;
  return WebLayerFromGraphicsLayer(
      composited_layer_mapping->ScrollingContentsLayer());
}

static WebLayer* WebLayerFromElement(Element* element) {
  CompositedLayerMapping* composited_layer_mapping =
      MappingFromElement(element);
  if (!composited_layer_mapping)
    return nullptr;
  return WebLayerFromGraphicsLayer(
      composited_layer_mapping->MainGraphicsLayer());
}

INSTANTIATE_TEST_CASE_P(All, CompositorWorkerTest, ::testing::Bool());

TEST_P(CompositorWorkerTest, plumbingElementIdAndMutableProperties) {
  RegisterMockedHttpURLLoad("compositor-proxy-basic.html");
  NavigateTo(base_url_ + "compositor-proxy-basic.html");

  ForceFullCompositingUpdate();

  Document* document = GetFrame()->GetDocument();

  Element* tall_element = document->getElementById("tall");
  WebLayer* tall_layer = WebLayerFromElement(tall_element);
  EXPECT_TRUE(!tall_layer);

  Element* proxied_element = document->getElementById("proxied-transform");
  WebLayer* proxied_layer = WebLayerFromElement(proxied_element);
  EXPECT_TRUE(proxied_layer->CompositorMutableProperties() &
              CompositorMutableProperty::kTransform);
  EXPECT_FALSE(proxied_layer->CompositorMutableProperties() &
               (CompositorMutableProperty::kScrollLeft |
                CompositorMutableProperty::kScrollTop |
                CompositorMutableProperty::kOpacity));
  EXPECT_TRUE(proxied_layer->GetElementId());

  Element* scroll_element = document->getElementById("proxied-scroller");
  WebLayer* scroll_layer = ScrollingWebLayerFromElement(scroll_element);
  EXPECT_TRUE(scroll_layer->CompositorMutableProperties() &
              (CompositorMutableProperty::kScrollLeft |
               CompositorMutableProperty::kScrollTop));
  EXPECT_FALSE(scroll_layer->CompositorMutableProperties() &
               (CompositorMutableProperty::kTransform |
                CompositorMutableProperty::kOpacity));
  EXPECT_TRUE(scroll_layer->GetElementId());

  WebLayer* root_scroll_layer = GetRootScrollLayer();
  EXPECT_TRUE(root_scroll_layer->CompositorMutableProperties() &
              (CompositorMutableProperty::kScrollLeft |
               CompositorMutableProperty::kScrollTop));
  EXPECT_FALSE(root_scroll_layer->CompositorMutableProperties() &
               (CompositorMutableProperty::kTransform |
                CompositorMutableProperty::kOpacity));

  EXPECT_TRUE(root_scroll_layer->GetElementId());
}

TEST_P(CompositorWorkerTest, noProxies) {
  // This case is identical to compositor-proxy-basic, but no proxies have
  // actually been created.
  RegisterMockedHttpURLLoad("compositor-proxy-plumbing-no-proxies.html");
  NavigateTo(base_url_ + "compositor-proxy-plumbing-no-proxies.html");

  ForceFullCompositingUpdate();

  Document* document = GetFrame()->GetDocument();

  Element* tall_element = document->getElementById("tall");
  WebLayer* tall_layer = WebLayerFromElement(tall_element);
  EXPECT_TRUE(!tall_layer);

  Element* proxied_element = document->getElementById("proxied");
  WebLayer* proxied_layer = WebLayerFromElement(proxied_element);
  EXPECT_TRUE(!proxied_layer);

  // Note: we presume the existance of mutable properties implies that the the
  // element has a corresponding compositor proxy. Element ids (which are also
  // used by animations) do not have this implication, so we do not check for
  // them here.
  Element* scroll_element = document->getElementById("proxied-scroller");
  WebLayer* scroll_layer = ScrollingWebLayerFromElement(scroll_element);
  EXPECT_FALSE(!!scroll_layer->CompositorMutableProperties());

  WebLayer* root_scroll_layer = GetRootScrollLayer();
  EXPECT_FALSE(!!root_scroll_layer->CompositorMutableProperties());
}

TEST_P(CompositorWorkerTest, disconnectedProxies) {
  // This case is identical to compositor-proxy-basic, but the proxies are
  // disconnected (the result should be the same as
  // compositor-proxy-plumbing-no-proxies).
  RegisterMockedHttpURLLoad("compositor-proxy-basic-disconnected.html");
  NavigateTo(base_url_ + "compositor-proxy-basic-disconnected.html");

  ForceFullCompositingUpdate();

  Document* document = GetFrame()->GetDocument();

  Element* tall_element = document->getElementById("tall");
  WebLayer* tall_layer = WebLayerFromElement(tall_element);
  EXPECT_TRUE(!tall_layer);

  Element* proxied_element = document->getElementById("proxied");
  WebLayer* proxied_layer = WebLayerFromElement(proxied_element);
  EXPECT_TRUE(!proxied_layer);

  Element* scroll_element = document->getElementById("proxied-scroller");
  WebLayer* scroll_layer = ScrollingWebLayerFromElement(scroll_element);
  EXPECT_FALSE(!!scroll_layer->CompositorMutableProperties());

  WebLayer* root_scroll_layer = GetRootScrollLayer();
  EXPECT_FALSE(!!root_scroll_layer->CompositorMutableProperties());
}

TEST_P(CompositorWorkerTest, applyingMutationsMultipleElements) {
  RegisterMockedHttpURLLoad("compositor-proxy-basic.html");
  NavigateTo(base_url_ + "compositor-proxy-basic.html");

  Document* document = GetFrame()->GetDocument();

  {
    ForceFullCompositingUpdate();

    Element* proxied_element = document->getElementById("proxied-transform");
    WebLayer* proxied_layer = WebLayerFromElement(proxied_element);
    EXPECT_TRUE(proxied_layer->CompositorMutableProperties() &
                CompositorMutableProperty::kTransform);
    EXPECT_FALSE(proxied_layer->CompositorMutableProperties() &
                 (CompositorMutableProperty::kScrollLeft |
                  CompositorMutableProperty::kScrollTop |
                  CompositorMutableProperty::kOpacity));
    EXPECT_TRUE(proxied_layer->GetElementId());

    TransformationMatrix transform_matrix(11, 12, 13, 14, 21, 22, 23, 24, 31,
                                          32, 33, 34, 41, 42, 43, 44);
    CompositorMutation mutation;
    mutation.SetTransform(TransformationMatrix::ToSkMatrix44(transform_matrix));

    proxied_element->UpdateFromCompositorMutation(mutation);

    ForceFullCompositingUpdate();
    const String& css_value =
        document->domWindow()
            ->getComputedStyle(proxied_element, String())
            ->GetPropertyValueInternal(CSSPropertyTransform);
    EXPECT_EQ(
        "matrix3d(11, 12, 13, 14, 21, 22, 23, 24, 31, 32, 33, 34, 41, 42, 43, "
        "44)",
        css_value);
  }
  {
    Element* proxied_element = document->getElementById("proxied-opacity");
    WebLayer* proxied_layer = WebLayerFromElement(proxied_element);
    EXPECT_TRUE(proxied_layer->CompositorMutableProperties() &
                CompositorMutableProperty::kOpacity);
    EXPECT_FALSE(proxied_layer->CompositorMutableProperties() &
                 (CompositorMutableProperty::kScrollLeft |
                  CompositorMutableProperty::kScrollTop |
                  CompositorMutableProperty::kTransform));
    EXPECT_TRUE(proxied_layer->GetElementId());

    CompositorMutation mutation;
    mutation.SetOpacity(0.5);

    proxied_element->UpdateFromCompositorMutation(mutation);

    ForceFullCompositingUpdate();
    const String& css_value =
        document->domWindow()
            ->getComputedStyle(proxied_element, String())
            ->GetPropertyValueInternal(CSSPropertyOpacity);
    EXPECT_EQ("0.5", css_value);
  }
}

TEST_P(CompositorWorkerTest, applyingMutationsMultipleProperties) {
  RegisterMockedHttpURLLoad("compositor-proxy-basic.html");
  NavigateTo(base_url_ + "compositor-proxy-basic.html");

  Document* document = GetFrame()->GetDocument();

  ForceFullCompositingUpdate();

  Element* proxied_element =
      document->getElementById("proxied-transform-and-opacity");
  WebLayer* proxied_layer = WebLayerFromElement(proxied_element);
  EXPECT_TRUE(proxied_layer->CompositorMutableProperties() &
              CompositorMutableProperty::kTransform);
  EXPECT_TRUE(proxied_layer->CompositorMutableProperties() &
              CompositorMutableProperty::kOpacity);
  EXPECT_FALSE(proxied_layer->CompositorMutableProperties() &
               (CompositorMutableProperty::kScrollLeft |
                CompositorMutableProperty::kScrollTop));
  EXPECT_TRUE(proxied_layer->GetElementId());

  TransformationMatrix transform_matrix(11, 12, 13, 14, 21, 22, 23, 24, 31, 32,
                                        33, 34, 41, 42, 43, 44);
  std::unique_ptr<CompositorMutation> mutation =
      WTF::WrapUnique(new CompositorMutation);
  mutation->SetTransform(TransformationMatrix::ToSkMatrix44(transform_matrix));
  mutation->SetOpacity(0.5);

  proxied_element->UpdateFromCompositorMutation(*mutation);
  {
    const String& transform_value =
        document->domWindow()
            ->getComputedStyle(proxied_element, String())
            ->GetPropertyValueInternal(CSSPropertyTransform);
    EXPECT_EQ(
        "matrix3d(11, 12, 13, 14, 21, 22, 23, 24, 31, 32, 33, 34, 41, 42, 43, "
        "44)",
        transform_value);

    const String& opacity_value =
        document->domWindow()
            ->getComputedStyle(proxied_element, String())
            ->GetPropertyValueInternal(CSSPropertyOpacity);
    EXPECT_EQ("0.5", opacity_value);
  }

  // Verify that updating one property does not impact others
  mutation = WTF::WrapUnique(new CompositorMutation);
  mutation->SetOpacity(0.8);

  proxied_element->UpdateFromCompositorMutation(*mutation);
  {
    const String& transform_value =
        document->domWindow()
            ->getComputedStyle(proxied_element, String())
            ->GetPropertyValueInternal(CSSPropertyTransform);
    EXPECT_EQ(
        "matrix3d(11, 12, 13, 14, 21, 22, 23, 24, 31, 32, 33, 34, 41, 42, 43, "
        "44)",
        transform_value);

    const String& opacity_value =
        document->domWindow()
            ->getComputedStyle(proxied_element, String())
            ->GetPropertyValueInternal(CSSPropertyOpacity);
    EXPECT_EQ("0.8", opacity_value);
  }
}

}  // namespace blink
