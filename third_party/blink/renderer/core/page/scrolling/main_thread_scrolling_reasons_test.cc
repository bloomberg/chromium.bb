// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class MainThreadScrollingReasonsTest
    : public testing::Test,
      public testing::WithParamInterface<bool>,
      private ScopedBlinkGenPropertyTreesForTest {
 public:
  MainThreadScrollingReasonsTest()
      : ScopedBlinkGenPropertyTreesForTest(GetParam()),
        base_url_("http://www.test.com/") {
    helper_.InitializeWithSettings(&ConfigureSettings);
    GetWebView()->MainFrameWidget()->Resize(IntSize(320, 240));

    // macOS attaches main frame scrollbars to the VisualViewport so the
    // VisualViewport layers need to be initialized.
    GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
    WebFrameWidgetBase* main_frame_widget =
        GetWebView()->MainFrameImpl()->FrameWidgetImpl();
    main_frame_widget->SetRootGraphicsLayer(GetWebView()
                                                ->MainFrameImpl()
                                                ->GetFrame()
                                                ->View()
                                                ->GetLayoutView()
                                                ->Compositor()
                                                ->RootGraphicsLayer());
  }

  ~MainThreadScrollingReasonsTest() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void NavigateTo(const std::string& url) {
    frame_test_helpers::LoadFrame(GetWebView()->MainFrameImpl(), url);
  }

  void LoadHTML(const std::string& html) {
    frame_test_helpers::LoadHTMLString(GetWebView()->MainFrameImpl(), html,
                                       url_test_helpers::ToKURL("about:blank"));
  }

  void ForceFullCompositingUpdate() {
    GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
  }

  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  uint32_t GetViewMainThreadScrollingReasons() const {
    // Pre-BlinkGenPropertyTrees, main thread scrolling reasons are set on
    // cc::Layer in ScrollingCoordinator.
    if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
      auto* layer = GetFrame()->View()->LayoutViewport()->LayerForScrolling();
      return layer->CcLayer()->GetMainThreadScrollingReasons();
    }
    // With BlinkGenPropertyTrees, main thread scrolling reasons are calculated
    // in the blink property tree builder and set on set on scroll nodes.
    const auto* scroll = GetFrame()
                             ->View()
                             ->GetLayoutView()
                             ->FirstFragment()
                             .PaintProperties()
                             ->Scroll();
    return scroll->GetMainThreadScrollingReasons();
  }

  WebViewImpl* GetWebView() const { return helper_.GetWebView(); }
  LocalFrame* GetFrame() const { return helper_.LocalMainFrame()->GetFrame(); }

 protected:
  std::string base_url_;

 private:
  static void ConfigureSettings(WebSettings* settings) {
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  frame_test_helpers::WebViewHelper helper_;
};

INSTANTIATE_TEST_CASE_P(All, MainThreadScrollingReasonsTest, testing::Bool());


TEST_P(MainThreadScrollingReasonsTest,
       CustomScrollbarShouldTriggerMainThreadScroll) {
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  GetWebView()->SetDeviceScaleFactor(2.f);
  RegisterMockedHttpURLLoad("custom_scrollbar.html");
  NavigateTo(base_url_ + "custom_scrollbar.html");
  ForceFullCompositingUpdate();

  Document* document = GetFrame()->GetDocument();
  Element* container = document->getElementById("container");
  Element* content = document->getElementById("content");
  DCHECK_EQ(container->getAttribute(html_names::kClassAttr),
            "custom_scrollbar");
  DCHECK(container);
  DCHECK(content);

  LayoutObject* layout_object = container->GetLayoutObject();
  ASSERT_TRUE(layout_object->IsBox());
  LayoutBox* box = ToLayoutBox(layout_object);
  ASSERT_TRUE(box->UsesCompositedScrolling());
  CompositedLayerMapping* composited_layer_mapping =
      box->Layer()->GetCompositedLayerMapping();
  // When Blink generates property trees, the main thread reasons are stored in
  // scroll nodes instead of being set on the custom scrollbar layer, so we use
  // the scrolling contents layer to access the main thread scrolling reasons.
  GraphicsLayer* layer =
      RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()
          ? composited_layer_mapping->ScrollingContentsLayer()
          : composited_layer_mapping->LayerForVerticalScrollbar();
  ASSERT_TRUE(layer);
  EXPECT_TRUE(layer->CcLayer()->GetMainThreadScrollingReasons());
  EXPECT_TRUE(layer->CcLayer()->GetMainThreadScrollingReasons() &
              MainThreadScrollingReason::kCustomScrollbarScrolling);

  // remove custom scrollbar class, the scrollbar is expected to scroll on
  // impl thread as it is an overlay scrollbar.
  container->removeAttribute("class");
  ForceFullCompositingUpdate();
  layer = RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()
              ? composited_layer_mapping->ScrollingContentsLayer()
              : composited_layer_mapping->LayerForVerticalScrollbar();
  EXPECT_FALSE(layer->CcLayer()->GetMainThreadScrollingReasons());
  EXPECT_FALSE(layer->CcLayer()->GetMainThreadScrollingReasons() &
               MainThreadScrollingReason::kCustomScrollbarScrolling);
}

TEST_P(MainThreadScrollingReasonsTest,
       BackgroundAttachmentFixedShouldTriggerMainThreadScroll) {
  RegisterMockedHttpURLLoad("iframe-background-attachment-fixed.html");
  RegisterMockedHttpURLLoad("iframe-background-attachment-fixed-inner.html");
  RegisterMockedHttpURLLoad("white-1x1.png");
  NavigateTo(base_url_ + "iframe-background-attachment-fixed.html");
  ForceFullCompositingUpdate();

  Element* iframe = GetFrame()->GetDocument()->getElementById("iframe");
  ASSERT_TRUE(iframe);

  LayoutObject* layout_object = iframe->GetLayoutObject();
  ASSERT_TRUE(layout_object);
  ASSERT_TRUE(layout_object->IsLayoutEmbeddedContent());

  LayoutEmbeddedContent* layout_embedded_content =
      ToLayoutEmbeddedContent(layout_object);
  ASSERT_TRUE(layout_embedded_content);

  LocalFrameView* inner_frame_view =
      ToLocalFrameView(layout_embedded_content->ChildFrameView());
  ASSERT_TRUE(inner_frame_view);

  auto* inner_layout_view = inner_frame_view->GetLayoutView();
  ASSERT_TRUE(inner_layout_view);

  PaintLayerCompositor* inner_compositor = inner_layout_view->Compositor();
  ASSERT_TRUE(inner_compositor->InCompositingMode());

  GraphicsLayer* scroll_layer =
      inner_frame_view->LayoutViewport()->LayerForScrolling();
  ASSERT_TRUE(scroll_layer);

  cc::Layer* cc_scroll_layer = scroll_layer->CcLayer();
  ASSERT_TRUE(cc_scroll_layer->scrollable());
  ASSERT_TRUE(cc_scroll_layer->GetMainThreadScrollingReasons() &
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);

  // Remove fixed background-attachment should make the iframe
  // scroll on cc.
  auto* iframe_doc = ToHTMLIFrameElement(iframe)->contentDocument();
  iframe = iframe_doc->getElementById("scrollable");
  ASSERT_TRUE(iframe);

  iframe->removeAttribute("class");
  ForceFullCompositingUpdate();

  layout_object = iframe->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  scroll_layer =
      layout_object->GetFrameView()->LayoutViewport()->LayerForScrolling();
  ASSERT_TRUE(scroll_layer);

  cc_scroll_layer = scroll_layer->CcLayer();
  ASSERT_TRUE(cc_scroll_layer->scrollable());
  ASSERT_FALSE(cc_scroll_layer->GetMainThreadScrollingReasons() &
               MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);

  // Force main frame to scroll on main thread. All its descendants
  // should scroll on main thread as well.
  Element* element = GetFrame()->GetDocument()->getElementById("scrollable");
  element->setAttribute(
      "style",
      "background-image: url('white-1x1.png'); background-attachment: fixed;",
      ASSERT_NO_EXCEPTION);

  ForceFullCompositingUpdate();

  layout_object = iframe->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  scroll_layer =
      layout_object->GetFrameView()->LayoutViewport()->LayerForScrolling();
  ASSERT_TRUE(scroll_layer);

  cc_scroll_layer = scroll_layer->CcLayer();
  ASSERT_TRUE(cc_scroll_layer->scrollable());
  ASSERT_TRUE(cc_scroll_layer->GetMainThreadScrollingReasons() &
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
}

// Upon resizing the content size, the main thread scrolling reason
// kHasNonLayerViewportConstrainedObject should be updated on all frames
TEST_P(MainThreadScrollingReasonsTest,
       RecalculateMainThreadScrollingReasonsUponResize) {
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  RegisterMockedHttpURLLoad("has-non-layer-viewport-constrained-objects.html");
  RegisterMockedHttpURLLoad("white-1x1.png");
  NavigateTo(base_url_ + "has-non-layer-viewport-constrained-objects.html");
  ForceFullCompositingUpdate();

  // When the main document is not scrollable, there should be no reasons.
  EXPECT_FALSE(GetViewMainThreadScrollingReasons());

  // When the div forces the document to be scrollable, it should scroll on main
  // thread.
  Element* element = GetFrame()->GetDocument()->getElementById("scrollable");
  element->setAttribute(
      "style",
      "background-image: url('white-1x1.png'); background-attachment: fixed;",
      ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  EXPECT_TRUE(GetViewMainThreadScrollingReasons() &
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);

  // The main thread scrolling reason should be reset upon the following change.
  element->setAttribute("style", "", ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  EXPECT_FALSE(GetViewMainThreadScrollingReasons());
}

TEST_P(MainThreadScrollingReasonsTest, FastScrollingCanBeDisabledWithSetting) {
  GetWebView()->MainFrameWidget()->Resize(WebSize(800, 600));
  LoadHTML("<div id='spacer' style='height: 1000px'></div>");
  GetWebView()->GetSettings()->SetThreadedScrollingEnabled(false);
  GetFrame()->View()->SetNeedsPaintPropertyUpdate();
  ForceFullCompositingUpdate();

  // Main scrolling should be enabled with the setting override.
  EXPECT_TRUE(GetViewMainThreadScrollingReasons());

  // Main scrolling should also propagate to inner viewport layer.
  cc::Layer* inner_viewport_scroll_layer =
      GetFrame()->GetPage()->GetVisualViewport().ScrollLayer()->CcLayer();
  ASSERT_TRUE(inner_viewport_scroll_layer->scrollable());
  EXPECT_TRUE(inner_viewport_scroll_layer->GetMainThreadScrollingReasons());
}

TEST_P(MainThreadScrollingReasonsTest, FastScrollingForFixedPosition) {
  RegisterMockedHttpURLLoad("fixed-position.html");
  NavigateTo(base_url_ + "fixed-position.html");
  ForceFullCompositingUpdate();

  // Fixed position should not fall back to main thread scrolling.
  EXPECT_FALSE(GetViewMainThreadScrollingReasons());
}

TEST_P(MainThreadScrollingReasonsTest, FastScrollingForStickyPosition) {
  RegisterMockedHttpURLLoad("sticky-position.html");
  NavigateTo(base_url_ + "sticky-position.html");
  ForceFullCompositingUpdate();

  // Sticky position should not fall back to main thread scrolling.
  EXPECT_FALSE(GetViewMainThreadScrollingReasons());
}

TEST_P(MainThreadScrollingReasonsTest, FastScrollingByDefault) {
  GetWebView()->MainFrameWidget()->Resize(WebSize(800, 600));
  LoadHTML("<div id='spacer' style='height: 1000px'></div>");
  ForceFullCompositingUpdate();

  // Fast scrolling should be enabled by default.
  EXPECT_FALSE(GetViewMainThreadScrollingReasons());

  cc::Layer* inner_viewport_scroll_layer =
      GetFrame()->GetPage()->GetVisualViewport().ScrollLayer()->CcLayer();
  ASSERT_TRUE(inner_viewport_scroll_layer->scrollable());
  EXPECT_FALSE(inner_viewport_scroll_layer->GetMainThreadScrollingReasons());
}

TEST_P(MainThreadScrollingReasonsTest,
       ScrollbarsForceMainThreadOrHaveCompositorScrollbarLayer) {
  RegisterMockedHttpURLLoad("trivial-scroller.html");
  NavigateTo(base_url_ + "trivial-scroller.html");
  ForceFullCompositingUpdate();

  Document* document = GetFrame()->GetDocument();
  Element* scrollable_element = document->getElementById("scroller");
  DCHECK(scrollable_element);

  LayoutObject* layout_object = scrollable_element->GetLayoutObject();
  ASSERT_TRUE(layout_object->IsBox());
  LayoutBox* box = ToLayoutBox(layout_object);
  ASSERT_TRUE(box->UsesCompositedScrolling());
  CompositedLayerMapping* composited_layer_mapping =
      box->Layer()->GetCompositedLayerMapping();
  GraphicsLayer* scrollbar_graphics_layer =
      composited_layer_mapping->LayerForVerticalScrollbar();
  ASSERT_TRUE(scrollbar_graphics_layer);

  bool has_cc_scrollbar_layer = !scrollbar_graphics_layer->DrawsContent();
  EXPECT_TRUE(
      has_cc_scrollbar_layer ||
      scrollbar_graphics_layer->CcLayer()->GetMainThreadScrollingReasons());
}

class NonCompositedMainThreadScrollingReasonsTest
    : public MainThreadScrollingReasonsTest {
  static const uint32_t kLCDTextRelatedReasons =
      MainThreadScrollingReason::kHasOpacityAndLCDText |
      MainThreadScrollingReason::kHasTransformAndLCDText |
      MainThreadScrollingReason::kBackgroundNotOpaqueInRectAndLCDText |
      MainThreadScrollingReason::kIsNotStackingContextAndLCDText;

 protected:
  NonCompositedMainThreadScrollingReasonsTest() {
    RegisterMockedHttpURLLoad("two_scrollable_area.html");
    NavigateTo(base_url_ + "two_scrollable_area.html");
  }
  void TestNonCompositedReasons(const std::string& target,
                                const uint32_t reason) {
    GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
    Document* document = GetFrame()->GetDocument();
    Element* container = document->getElementById("scroller1");
    container->setAttribute("class", target.c_str(), ASSERT_NO_EXCEPTION);
    ForceFullCompositingUpdate();

    PaintLayerScrollableArea* scrollable_area =
        ToLayoutBoxModelObject(container->GetLayoutObject())
            ->GetScrollableArea();
    ASSERT_TRUE(scrollable_area);
    EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
                reason);

    Element* container2 = document->getElementById("scroller2");
    PaintLayerScrollableArea* scrollable_area2 =
        ToLayoutBoxModelObject(container2->GetLayoutObject())
            ->GetScrollableArea();
    ASSERT_TRUE(scrollable_area2);
    // Different scrollable area should remain unaffected.
    EXPECT_FALSE(
        scrollable_area2->GetNonCompositedMainThreadScrollingReasons() &
        reason);

    LocalFrameView* frame_view = GetFrame()->View();
    ASSERT_TRUE(frame_view);
    EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons() & reason);

    // Remove attribute from the scroller 1 would lead to scroll on impl.
    container->removeAttribute("class");
    ForceFullCompositingUpdate();

    EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
                 reason);
    EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons() & reason);

    // Add target attribute would again lead to scroll on main thread
    container->setAttribute("class", target.c_str(), ASSERT_NO_EXCEPTION);
    ForceFullCompositingUpdate();

    EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
                reason);
    EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons() & reason);

    if ((reason & kLCDTextRelatedReasons) &&
        !(reason & ~kLCDTextRelatedReasons)) {
      GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
      ForceFullCompositingUpdate();
      EXPECT_FALSE(
          scrollable_area->GetNonCompositedMainThreadScrollingReasons());
      EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons());
    }
  }
};

INSTANTIATE_TEST_CASE_P(All,
                        NonCompositedMainThreadScrollingReasonsTest,
                        testing::Bool());

TEST_P(NonCompositedMainThreadScrollingReasonsTest, TransparentTest) {
  TestNonCompositedReasons("transparent",
                           MainThreadScrollingReason::kHasOpacityAndLCDText);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, TransformTest) {
  TestNonCompositedReasons("transform",
                           MainThreadScrollingReason::kHasTransformAndLCDText);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, BackgroundNotOpaqueTest) {
  TestNonCompositedReasons(
      "background-not-opaque",
      MainThreadScrollingReason::kBackgroundNotOpaqueInRectAndLCDText);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, ClipTest) {
  TestNonCompositedReasons("clip",
                           MainThreadScrollingReason::kHasClipRelatedProperty);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, ClipPathTest) {
  uint32_t clip_reason = MainThreadScrollingReason::kHasClipRelatedProperty;
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  Document* document = GetFrame()->GetDocument();
  // Test ancestor with ClipPath
  Element* element = document->body();
  element->setAttribute(html_names::kStyleAttr,
                        "clip-path:circle(115px at 20px 20px);");
  Element* container = document->getElementById("scroller1");
  ASSERT_TRUE(container);
  ForceFullCompositingUpdate();

  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(container->GetLayoutObject())->GetScrollableArea();
  EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
              clip_reason);

  EXPECT_FALSE(GetViewMainThreadScrollingReasons() & clip_reason);

  // Remove clip path from ancestor.
  element->removeAttribute(html_names::kStyleAttr);
  ForceFullCompositingUpdate();

  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
               clip_reason);
  EXPECT_FALSE(GetViewMainThreadScrollingReasons() & clip_reason);

  // Test descendant with ClipPath
  element = document->getElementById("content1");
  ASSERT_TRUE(element);
  element->setAttribute(html_names::kStyleAttr,
                        "clip-path:circle(115px at 20px 20px);");
  ForceFullCompositingUpdate();
  EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
              clip_reason);
  EXPECT_FALSE(GetViewMainThreadScrollingReasons() & clip_reason);

  // Remove clip path from descendant.
  element->removeAttribute(html_names::kStyleAttr);
  ForceFullCompositingUpdate();
  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
               clip_reason);
  EXPECT_FALSE(GetViewMainThreadScrollingReasons() & clip_reason);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, LCDTextEnabledTest) {
  TestNonCompositedReasons("transparent",
                           MainThreadScrollingReason::kHasOpacityAndLCDText);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, BoxShadowTest) {
  TestNonCompositedReasons(
      "box-shadow", MainThreadScrollingReason::kHasBoxShadowFromNonRootLayer);
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest, StackingContextTest) {
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);

  Document* document = GetFrame()->GetDocument();
  Element* container = document->getElementById("scroller1");
  ASSERT_TRUE(container);

  ForceFullCompositingUpdate();

  // If a scroller contains all its children, it's not a stacking context.
  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(container->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
              MainThreadScrollingReason::kIsNotStackingContextAndLCDText);

  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  ForceFullCompositingUpdate();
  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
               MainThreadScrollingReason::kIsNotStackingContextAndLCDText);
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);

  // Adding "contain: paint" to force a stacking context leads to promotion.
  container->setAttribute("style", "contain: paint", ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons());
}

TEST_P(NonCompositedMainThreadScrollingReasonsTest,
       CompositedWithLCDTextRelatedReasonsTest) {
  // With "will-change:transform" we composite elements with
  // LCDTextRelatedReasons only. For elements with other
  // NonCompositedReasons, we don't create scrollingLayer for their
  // CompositedLayerMapping therefore they don't get composited.
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  Document* document = GetFrame()->GetDocument();
  Element* container = document->getElementById("scroller1");
  ASSERT_TRUE(container);
  container->setAttribute("class", "composited transparent",
                          ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(container->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons());

  Element* container2 = document->getElementById("scroller2");
  ASSERT_TRUE(container2);
  container2->setAttribute("class", "composited border-radius",
                           ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();
  PaintLayerScrollableArea* scrollable_area2 =
      ToLayoutBoxModelObject(container2->GetLayoutObject())
          ->GetScrollableArea();
  ASSERT_TRUE(scrollable_area2);
  ASSERT_TRUE(scrollable_area2->UsesCompositedScrolling());
}

}  // namespace blink
