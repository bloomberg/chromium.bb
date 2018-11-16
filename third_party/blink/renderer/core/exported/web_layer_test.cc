// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {

// Tests the integration between blink and cc where a layer list is sent to cc.
class WebLayerListTest : public PaintTestConfigurations, public testing::Test {
 public:
  static void ConfigureCompositingWebView(WebSettings* settings) {
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  ~WebLayerListTest() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void SetUp() override {
    web_view_helper_ = std::make_unique<frame_test_helpers::WebViewHelper>();
    web_view_helper_->Initialize(nullptr, &web_view_client_, nullptr,
                                 &ConfigureCompositingWebView);
    web_view_helper_->Resize(WebSize(200, 200));

    // The paint artifact compositor should have been created as part of the
    // web view helper setup.
    DCHECK(paint_artifact_compositor());
    paint_artifact_compositor()->EnableExtraDataForTesting();
  }

  // Both sets the inner html and runs the document lifecycle.
  void InitializeWithHTML(LocalFrame& frame, const String& html_content) {
    frame.GetDocument()->body()->SetInnerHTMLFromString(html_content);
    frame.GetDocument()->View()->UpdateAllLifecyclePhases();
  }

  WebLocalFrame* LocalMainFrame() { return web_view_helper_->LocalMainFrame(); }

  LocalFrameView* GetLocalFrameView() {
    return web_view_helper_->LocalMainFrame()->GetFrameView();
  }

  WebViewImpl* WebView() { return web_view_helper_->GetWebView(); }

  size_t ContentLayerCount() {
    return paint_artifact_compositor()
        ->GetExtraDataForTesting()
        ->content_layers.size();
  }

  cc::Layer* ContentLayerAt(size_t index) {
    return paint_artifact_compositor()
        ->GetExtraDataForTesting()
        ->content_layers[index]
        .get();
  }

  size_t ScrollHitTestLayerCount() {
    return paint_artifact_compositor()
        ->GetExtraDataForTesting()
        ->scroll_hit_test_layers.size();
  }

  cc::Layer* ScrollHitTestLayerAt(unsigned index) {
    return paint_artifact_compositor()
        ->GetExtraDataForTesting()
        ->ScrollHitTestWebLayerAt(index);
  }

  cc::LayerTreeHost* LayerTreeHost() {
    return web_view_client_.layer_tree_view()->layer_tree_host();
  }

  Element* GetElementById(const AtomicString& id) {
    WebLocalFrameImpl* frame = web_view_helper_->LocalMainFrame();
    return frame->GetFrame()->GetDocument()->getElementById(id);
  }

 private:
  PaintArtifactCompositor* paint_artifact_compositor() {
    return GetLocalFrameView()->GetPaintArtifactCompositorForTesting();
  }
  frame_test_helpers::TestWebViewClient web_view_client_;
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
};

INSTANTIATE_LAYER_LIST_TEST_CASE_P(WebLayerListTest);

TEST_P(WebLayerListTest, DidScrollCallbackAfterScrollableAreaChanges) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(),
                     "<style>"
                     "  #scrollable {"
                     "    height: 100px;"
                     "    width: 100px;"
                     "    overflow: scroll;"
                     "    will-change: transform;"
                     "  }"
                     "  #forceScroll { height: 120px; width: 50px; }"
                     "</style>"
                     "<div id='scrollable'>"
                     "  <div id='forceScroll'></div>"
                     "</div>");

  WebView()->MainFrameWidget()->UpdateAllLifecyclePhases();

  Document* document = WebView()->MainFrameImpl()->GetFrame()->GetDocument();
  Element* scrollable = document->getElementById("scrollable");

  auto* scrollable_area =
      ToLayoutBox(scrollable->GetLayoutObject())->GetScrollableArea();
  EXPECT_NE(nullptr, scrollable_area);

  auto initial_content_layer_count = ContentLayerCount();
  auto initial_scroll_hit_test_layer_count = ScrollHitTestLayerCount();

  cc::Layer* overflow_scroll_layer = nullptr;
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    overflow_scroll_layer = ScrollHitTestLayerAt(ScrollHitTestLayerCount() - 1);
  } else {
    overflow_scroll_layer = ContentLayerAt(ContentLayerCount() - 2);
  }
  EXPECT_TRUE(overflow_scroll_layer->scrollable());
  EXPECT_EQ(overflow_scroll_layer->scroll_container_bounds(),
            gfx::Size(100, 100));

  // Ensure a synthetic impl-side scroll offset propagates to the scrollable
  // area using the DidScroll callback.
  EXPECT_EQ(ScrollOffset(), scrollable_area->GetScrollOffset());
  overflow_scroll_layer->SetScrollOffsetFromImplSide(gfx::ScrollOffset(0, 1));
  WebView()->MainFrameWidget()->UpdateAllLifecyclePhases();
  EXPECT_EQ(ScrollOffset(0, 1), scrollable_area->GetScrollOffset());

  // Make the scrollable area non-scrollable.
  scrollable->setAttribute(html_names::kStyleAttr, "overflow: visible");

  // Update layout without updating compositing state.
  LocalMainFrame()->ExecuteScript(
      WebScriptSource("var forceLayoutFromScript = scrollable.offsetTop;"));
  EXPECT_EQ(document->Lifecycle().GetState(), DocumentLifecycle::kLayoutClean);

  EXPECT_EQ(nullptr,
            ToLayoutBox(scrollable->GetLayoutObject())->GetScrollableArea());

  // The web scroll layer has not been deleted yet and we should be able to
  // apply impl-side offsets without crashing.
  EXPECT_EQ(ContentLayerCount(), initial_content_layer_count);
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    EXPECT_EQ(ScrollHitTestLayerCount(), initial_scroll_hit_test_layer_count);
  overflow_scroll_layer->SetScrollOffsetFromImplSide(gfx::ScrollOffset(0, 3));

  WebView()->MainFrameWidget()->UpdateAllLifecyclePhases();
  EXPECT_LT(ContentLayerCount(), initial_content_layer_count);
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    EXPECT_LT(ScrollHitTestLayerCount(), initial_scroll_hit_test_layer_count);
}

TEST_P(WebLayerListTest, FrameViewScroll) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(),
                     "<style>"
                     "  #forceScroll {"
                     "    height: 2000px;"
                     "    width: 100px;"
                     "  }"
                     "</style>"
                     "<div id='forceScroll'></div>");

  WebView()->MainFrameWidget()->UpdateAllLifecyclePhases();

  auto* scrollable_area = GetLocalFrameView()->LayoutViewport();
  EXPECT_NE(nullptr, scrollable_area);

  cc::Layer* scroll_layer = nullptr;
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(ScrollHitTestLayerCount(), 1u);
    scroll_layer = ScrollHitTestLayerAt(0);
  } else {
    // Find the last scroll layer.
    for (size_t index = ContentLayerCount() - 1; index >= 0; index--) {
      if (ContentLayerAt(index)->scrollable()) {
        scroll_layer = ContentLayerAt(index);
        break;
      }
    }
  }
  EXPECT_TRUE(scroll_layer->scrollable());

  // Ensure a synthetic impl-side scroll offset propagates to the scrollable
  // area using the DidScroll callback.
  EXPECT_EQ(ScrollOffset(), scrollable_area->GetScrollOffset());
  scroll_layer->SetScrollOffsetFromImplSide(gfx::ScrollOffset(0, 1));
  WebView()->MainFrameWidget()->UpdateAllLifecyclePhases();
  EXPECT_EQ(ScrollOffset(0, 1), scrollable_area->GetScrollOffset());
}

class WebLayerListSimTest : public PaintTestConfigurations, public SimTest {
 public:
  void InitializeWithHTML(const String& html) {
    WebView().Resize(WebSize(800, 600));

    SimRequest request("https://example.com/test.html", "text/html");
    LoadURL("https://example.com/test.html");
    request.Complete(html);

    // Enable the paint artifact compositor extra testing data.
    WebView().MainFrameWidget()->UpdateAllLifecyclePhases();
    DCHECK(paint_artifact_compositor());
    paint_artifact_compositor()->EnableExtraDataForTesting();
    WebView().MainFrameWidget()->UpdateAllLifecyclePhases();
    DCHECK(paint_artifact_compositor()->GetExtraDataForTesting());
  }

  size_t ContentLayerCount() {
    return paint_artifact_compositor()
        ->GetExtraDataForTesting()
        ->content_layers.size();
  }

  cc::Layer* ContentLayerAt(size_t index) {
    return paint_artifact_compositor()
        ->GetExtraDataForTesting()
        ->content_layers[index]
        .get();
  }

  Element* GetElementById(const AtomicString& id) {
    return MainFrame().GetFrame()->GetDocument()->getElementById(id);
  }

 private:
  PaintArtifactCompositor* paint_artifact_compositor() {
    return MainFrame().GetFrameView()->GetPaintArtifactCompositorForTesting();
  }
};

INSTANTIATE_LAYER_LIST_TEST_CASE_P(WebLayerListSimTest);

TEST_P(WebLayerListSimTest, LayerUpdatesDoNotInvalidateEarlierLayers) {
  // TODO(crbug.com/765003): SPV2 may make different layerization decisions and
  // we cannot guarantee that both divs will be composited in this test. When
  // SPV2 gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        div {
          width: 100px;
          height: 100px;
          will-change: transform;
        }
      </style>
      <div id='a'></div>
      <div id='b'></div>
  )HTML");

  Compositor().BeginFrame();

  auto* a_element = GetElementById("a");
  auto* a_layer = ContentLayerAt(ContentLayerCount() - 2);
  DCHECK_EQ(a_layer->element_id(), CompositorElementIdFromUniqueObjectId(
                                       a_element->GetLayoutObject()->UniqueId(),
                                       CompositorElementIdNamespace::kPrimary));
  auto* b_element = GetElementById("b");
  auto* b_layer = ContentLayerAt(ContentLayerCount() - 1);
  DCHECK_EQ(b_layer->element_id(), CompositorElementIdFromUniqueObjectId(
                                       b_element->GetLayoutObject()->UniqueId(),
                                       CompositorElementIdNamespace::kPrimary));

  // Initially, neither a nor b should have a layer that should push properties.
  auto* host = Compositor().layer_tree_view().layer_tree_host();
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(a_layer));
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(b_layer));

  // Modifying b should only cause the b layer to need to push properties.
  b_element->setAttribute(html_names::kStyleAttr, "opacity: 0.2");
  WebView().MainFrameWidget()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(a_layer));
  EXPECT_TRUE(host->LayersThatShouldPushProperties().count(b_layer));

  // After a frame, no layers should need to push properties again.
  Compositor().BeginFrame();
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(a_layer));
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(b_layer));
}

TEST_P(WebLayerListSimTest, LayerUpdatesDoNotInvalidateLaterLayers) {
  // TODO(crbug.com/765003): SPV2 may make different layerization decisions and
  // we cannot guarantee that both divs will be composited in this test. When
  // SPV2 gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        div {
          width: 100px;
          height: 100px;
          will-change: transform;
        }
      </style>
      <div id='a'></div>
      <div id='b' style='opacity: 0.2;'></div>
      <div id='c'></div>
  )HTML");

  Compositor().BeginFrame();

  auto* a_element = GetElementById("a");
  auto* a_layer = ContentLayerAt(ContentLayerCount() - 3);
  DCHECK_EQ(a_layer->element_id(), CompositorElementIdFromUniqueObjectId(
                                       a_element->GetLayoutObject()->UniqueId(),
                                       CompositorElementIdNamespace::kPrimary));
  auto* b_element = GetElementById("b");
  auto* b_layer = ContentLayerAt(ContentLayerCount() - 2);
  DCHECK_EQ(b_layer->element_id(), CompositorElementIdFromUniqueObjectId(
                                       b_element->GetLayoutObject()->UniqueId(),
                                       CompositorElementIdNamespace::kPrimary));
  auto* c_element = GetElementById("c");
  auto* c_layer = ContentLayerAt(ContentLayerCount() - 1);
  DCHECK_EQ(c_layer->element_id(), CompositorElementIdFromUniqueObjectId(
                                       c_element->GetLayoutObject()->UniqueId(),
                                       CompositorElementIdNamespace::kPrimary));

  // Initially, no layer should need to push properties.
  auto* host = Compositor().layer_tree_view().layer_tree_host();
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(a_layer));
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(b_layer));
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(c_layer));

  // Modifying a and b (adding opacity to a and removing opacity from b) should
  // not cause the c layer to push properties.
  a_element->setAttribute(html_names::kStyleAttr, "opacity: 0.3");
  b_element->setAttribute(html_names::kStyleAttr, "");
  WebView().MainFrameWidget()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(host->LayersThatShouldPushProperties().count(a_layer));
  EXPECT_TRUE(host->LayersThatShouldPushProperties().count(b_layer));
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(c_layer));

  // After a frame, no layers should need to push properties again.
  Compositor().BeginFrame();
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(a_layer));
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(b_layer));
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(c_layer));
}

TEST_P(WebLayerListSimTest, NoopChangeDoesNotCauseFullTreeSync) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        div {
          width: 100px;
          height: 100px;
          will-change: transform;
        }
      </style>
      <div></div>
  )HTML");

  Compositor().BeginFrame();

  // Initially the host should not need to sync.
  auto* layer_tree_host = Compositor().layer_tree_view().layer_tree_host();
  EXPECT_FALSE(layer_tree_host->needs_full_tree_sync());

  // A no-op update should not cause the host to need a full tree sync.
  WebView().MainFrameWidget()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(layer_tree_host->needs_full_tree_sync());
}

// When a property tree change occurs that affects layer position, all layers
// associated with the changed property tree node, and all layers associated
// with a descendant of the changed property tree node need to have
// |subtree_property_changed| set for damage tracking. In non-layer-list mode,
// this occurs in BuildPropertyTreesInternal (see:
// SetLayerPropertyChangedForChild).
TEST_P(WebLayerListSimTest, LayerSubtreeTransformPropertyChanged) {
  // TODO(crbug.com/765003): SPV2 may make different layerization decisions and
  // we cannot guarantee that both divs will be composited in this test. When
  // SPV2 gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        #outer {
          width: 100px;
          height: 100px;
          will-change: transform;
          transform: translate(10px, 10px);
        }
        #inner {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightblue;
        }
      </style>
      <div id='outer'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* outer_element = GetElementById("outer");
  auto* outer_element_layer = ContentLayerAt(ContentLayerCount() - 2);
  DCHECK_EQ(outer_element_layer->element_id(),
            CompositorElementIdFromUniqueObjectId(
                outer_element->GetLayoutObject()->UniqueId(),
                CompositorElementIdNamespace::kPrimary));
  auto* inner_element = GetElementById("inner");
  auto* inner_element_layer = ContentLayerAt(ContentLayerCount() - 1);
  DCHECK_EQ(inner_element_layer->element_id(),
            CompositorElementIdFromUniqueObjectId(
                inner_element->GetLayoutObject()->UniqueId(),
                CompositorElementIdNamespace::kPrimary));

  // Initially, no layer should have |subtree_property_changed| set.
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());

  // Modifying the transform style should set |subtree_property_changed| on
  // both layers.
  outer_element->setAttribute(html_names::kStyleAttr,
                              "transform: translate(20px, 20px)");
  WebView().MainFrameWidget()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(outer_element_layer->subtree_property_changed());
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
}

// This test is similar to |LayerSubtreeTransformPropertyChanged| but for
// effect property node changes.
TEST_P(WebLayerListSimTest, LayerSubtreeEffectPropertyChanged) {
  // TODO(crbug.com/765003): SPV2 may make different layerization decisions and
  // we cannot guarantee that both divs will be composited in this test. When
  // SPV2 gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        #outer {
          width: 100px;
          height: 100px;
          will-change: transform;
          filter: blur(10px);
        }
        #inner {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightblue;
        }
      </style>
      <div id='outer'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* outer_element = GetElementById("outer");
  auto* outer_element_layer = ContentLayerAt(ContentLayerCount() - 2);
  DCHECK_EQ(outer_element_layer->element_id(),
            CompositorElementIdFromUniqueObjectId(
                outer_element->GetLayoutObject()->UniqueId(),
                CompositorElementIdNamespace::kEffectFilter));
  auto* inner_element = GetElementById("inner");
  auto* inner_element_layer = ContentLayerAt(ContentLayerCount() - 1);
  DCHECK_EQ(inner_element_layer->element_id(),
            CompositorElementIdFromUniqueObjectId(
                inner_element->GetLayoutObject()->UniqueId(),
                CompositorElementIdNamespace::kPrimary));

  // Initially, no layer should have |subtree_property_changed| set.
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());

  // Modifying the filter style should set |subtree_property_changed| on
  // both layers.
  outer_element->setAttribute(html_names::kStyleAttr, "filter: blur(20px)");
  WebView().MainFrameWidget()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(outer_element_layer->subtree_property_changed());
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
}

// This test is similar to |LayerSubtreeTransformPropertyChanged| but for
// clip property node changes.
TEST_P(WebLayerListSimTest, LayerSubtreeClipPropertyChanged) {
  // TODO(crbug.com/765003): SPV2 may make different layerization decisions and
  // we cannot guarantee that both divs will be composited in this test. When
  // SPV2 gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        #outer {
          width: 100px;
          height: 100px;
          will-change: transform;
          position: absolute;
          clip: rect(10px, 80px, 70px, 40px);
        }
        #inner {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightblue;
        }
      </style>
      <div id='outer'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* outer_element = GetElementById("outer");
  auto* outer_element_layer = ContentLayerAt(ContentLayerCount() - 2);
  auto* inner_element = GetElementById("inner");
  auto* inner_element_layer = ContentLayerAt(ContentLayerCount() - 1);
  DCHECK_EQ(inner_element_layer->element_id(),
            CompositorElementIdFromUniqueObjectId(
                inner_element->GetLayoutObject()->UniqueId(),
                CompositorElementIdNamespace::kPrimary));

  // Initially, no layer should have |subtree_property_changed| set.
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());

  // Modifying the clip style should set |subtree_property_changed| on
  // both layers.
  outer_element->setAttribute(html_names::kStyleAttr,
                              "clip: rect(1px, 8px, 7px, 4px);");
  WebView().MainFrameWidget()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(outer_element_layer->subtree_property_changed());
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
}

TEST_P(WebLayerListSimTest, LayerSubtreeOverflowClipPropertyChanged) {
  // TODO(crbug.com/765003): SPV2 may make different layerization decisions and
  // we cannot guarantee that both divs will be composited in this test. When
  // SPV2 gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        #outer {
          width: 100px;
          height: 100px;
          will-change: transform;
          position: absolute;
          overflow: hidden;
        }
        #inner {
          width: 200px;
          height: 100px;
          will-change: transform;
          background: lightblue;
        }
      </style>
      <div id='outer'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* outer_element = GetElementById("outer");
  auto* outer_element_layer = ContentLayerAt(ContentLayerCount() - 2);
  auto* inner_element = GetElementById("inner");
  auto* inner_element_layer = ContentLayerAt(ContentLayerCount() - 1);
  DCHECK_EQ(inner_element_layer->element_id(),
            CompositorElementIdFromUniqueObjectId(
                inner_element->GetLayoutObject()->UniqueId(),
                CompositorElementIdNamespace::kPrimary));

  // Initially, no layer should have |subtree_property_changed| set.
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());

  // Modifying the clip width should set |subtree_property_changed| on
  // both layers.
  outer_element->setAttribute(html_names::kStyleAttr, "width: 200px;");
  WebView().MainFrameWidget()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(outer_element_layer->subtree_property_changed());
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
}

}  // namespace blink
