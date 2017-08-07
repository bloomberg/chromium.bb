// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <memory>
#include "core/exported/WebViewImpl.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/WebLocalFrameImpl.h"
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
        base_url_, testing::CoreTestDataPath(), WebString::FromUTF8(file_name));
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

  WebViewImpl* GetWebView() const { return helper_.WebView(); }
  LocalFrame* GetFrame() const { return helper_.LocalMainFrame()->GetFrame(); }

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

INSTANTIATE_TEST_CASE_P(All, CompositorWorkerTest, ::testing::Bool());

TEST_P(CompositorWorkerTest, applyingMutationsMultipleElements) {
  RegisterMockedHttpURLLoad("compositor-worker-basic.html");
  NavigateTo(base_url_ + "compositor-worker-basic.html");

  Document* document = GetFrame()->GetDocument();

  {
    ForceFullCompositingUpdate();

    Element* proxied_element = document->getElementById("proxied-transform");

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
  RegisterMockedHttpURLLoad("compositor-worker-basic.html");
  NavigateTo(base_url_ + "compositor-worker-basic.html");

  Document* document = GetFrame()->GetDocument();

  ForceFullCompositingUpdate();

  Element* proxied_element =
      document->getElementById("proxied-transform-and-opacity");

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
