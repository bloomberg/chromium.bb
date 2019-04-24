// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "cc/layers/picture_layer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
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
    web_view_helper_->Initialize(nullptr, nullptr, &web_widget_client_,
                                 &ConfigureCompositingWebView);
    web_view_helper_->Resize(WebSize(200, 200));

    // The paint artifact compositor should have been created as part of the
    // web view helper setup.
    DCHECK(paint_artifact_compositor());
    paint_artifact_compositor()->EnableExtraDataForTesting();
  }

  void TearDown() override { web_view_helper_.reset(); }

  // Both sets the inner html and runs the document lifecycle.
  void InitializeWithHTML(LocalFrame& frame, const String& html_content) {
    frame.GetDocument()->body()->SetInnerHTMLFromString(html_content);
    frame.GetDocument()->View()->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
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
    return web_widget_client_.layer_tree_view()->layer_tree_host();
  }

  Element* GetElementById(const AtomicString& id) {
    WebLocalFrameImpl* frame = web_view_helper_->LocalMainFrame();
    return frame->GetFrame()->GetDocument()->getElementById(id);
  }

  void UpdateAllLifecyclePhases() {
    WebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
  }

 private:
  PaintArtifactCompositor* paint_artifact_compositor() {
    return GetLocalFrameView()->GetPaintArtifactCompositorForTesting();
  }

  frame_test_helpers::TestWebWidgetClient web_widget_client_;
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
};

INSTANTIATE_LAYER_LIST_TEST_SUITE_P(WebLayerListTest);

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

  UpdateAllLifecyclePhases();

  Document* document = WebView()->MainFrameImpl()->GetFrame()->GetDocument();
  Element* scrollable = document->getElementById("scrollable");

  auto* scrollable_area =
      ToLayoutBox(scrollable->GetLayoutObject())->GetScrollableArea();
  EXPECT_NE(nullptr, scrollable_area);

  auto initial_content_layer_count = ContentLayerCount();
  auto initial_scroll_hit_test_layer_count = ScrollHitTestLayerCount();

  cc::Layer* overflow_scroll_layer = nullptr;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
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
  UpdateAllLifecyclePhases();
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
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    EXPECT_EQ(ScrollHitTestLayerCount(), initial_scroll_hit_test_layer_count);
  overflow_scroll_layer->SetScrollOffsetFromImplSide(gfx::ScrollOffset(0, 3));

  UpdateAllLifecyclePhases();
  EXPECT_LT(ContentLayerCount(), initial_content_layer_count);
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
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

  UpdateAllLifecyclePhases();

  auto* scrollable_area = GetLocalFrameView()->LayoutViewport();
  EXPECT_NE(nullptr, scrollable_area);

  cc::Layer* scroll_layer = nullptr;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
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
  UpdateAllLifecyclePhases();
  EXPECT_EQ(ScrollOffset(0, 1), scrollable_area->GetScrollOffset());
}

class WebLayerListSimTest : public PaintTestConfigurations, public SimTest {
 public:
  void InitializeWithHTML(const String& html) {
    WebView().MainFrameWidget()->Resize(WebSize(800, 600));

    SimRequest request("https://example.com/test.html", "text/html");
    LoadURL("https://example.com/test.html");
    request.Complete(html);

    // Enable the paint artifact compositor extra testing data.
    UpdateAllLifecyclePhases();
    DCHECK(paint_artifact_compositor());
    paint_artifact_compositor()->EnableExtraDataForTesting();
    UpdateAllLifecyclePhases();

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

  void UpdateAllLifecyclePhases() {
    WebView().MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
  }

  cc::PropertyTrees* GetPropertyTrees() {
    return Compositor().layer_tree_view().layer_tree_host()->property_trees();
  }

 private:
  PaintArtifactCompositor* paint_artifact_compositor() {
    return MainFrame().GetFrameView()->GetPaintArtifactCompositorForTesting();
  }
};

INSTANTIATE_LAYER_LIST_TEST_SUITE_P(WebLayerListSimTest);

TEST_P(WebLayerListSimTest, LayerUpdatesDoNotInvalidateEarlierLayers) {
  // TODO(crbug.com/765003): CAP may make different layerization decisions and
  // we cannot guarantee that both divs will be composited in this test. When
  // CAP gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
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
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(a_layer));
  EXPECT_TRUE(host->LayersThatShouldPushProperties().count(b_layer));

  // After a frame, no layers should need to push properties again.
  Compositor().BeginFrame();
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(a_layer));
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(b_layer));
}

TEST_P(WebLayerListSimTest, LayerUpdatesDoNotInvalidateLaterLayers) {
  // TODO(crbug.com/765003): CAP may make different layerization decisions and
  // we cannot guarantee that both divs will be composited in this test. When
  // CAP gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
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
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(host->LayersThatShouldPushProperties().count(a_layer));
  EXPECT_TRUE(host->LayersThatShouldPushProperties().count(b_layer));
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(c_layer));

  // After a frame, no layers should need to push properties again.
  Compositor().BeginFrame();
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(a_layer));
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(b_layer));
  EXPECT_FALSE(host->LayersThatShouldPushProperties().count(c_layer));
}

TEST_P(WebLayerListSimTest,
       NoopChangeDoesNotCauseFullTreeSyncOrPropertyTreeUpdate) {
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
  int sequence_number = GetPropertyTrees()->sequence_number;
  EXPECT_GT(sequence_number, 0);

  // A no-op update should not cause the host to need a full tree sync.
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(layer_tree_host->needs_full_tree_sync());
  // It should also not cause a property tree update - the sequence number
  // should not change.
  EXPECT_EQ(sequence_number, GetPropertyTrees()->sequence_number);
}

// When a property tree change occurs that affects layer position, all layers
// associated with the changed property tree node, and all layers associated
// with a descendant of the changed property tree node need to have
// |subtree_property_changed| set for damage tracking. In non-layer-list mode,
// this occurs in BuildPropertyTreesInternal (see:
// SetLayerPropertyChangedForChild).
TEST_P(WebLayerListSimTest, LayerSubtreeTransformPropertyChanged) {
  // TODO(crbug.com/765003): CAP may make different layerization decisions and
  // we cannot guarantee that both divs will be composited in this test. When
  // CAP gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
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
  UpdateAllLifecyclePhases();
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
  // TODO(crbug.com/765003): CAP may make different layerization decisions and
  // we cannot guarantee that both divs will be composited in this test. When
  // CAP gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
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

  // Modifying the filter style should set |subtree_property_changed| on
  // both layers.
  outer_element->setAttribute(html_names::kStyleAttr, "filter: blur(20px)");
  UpdateAllLifecyclePhases();
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
  // TODO(crbug.com/765003): CAP may make different layerization decisions and
  // we cannot guarantee that both divs will be composited in this test. When
  // CAP gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
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
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(outer_element_layer->subtree_property_changed());
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
}

TEST_P(WebLayerListSimTest, LayerSubtreeOverflowClipPropertyChanged) {
  // TODO(crbug.com/765003): CAP may make different layerization decisions and
  // we cannot guarantee that both divs will be composited in this test. When
  // CAP gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
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
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(outer_element_layer->subtree_property_changed());
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
}

TEST_P(WebLayerListSimTest, SafeOpaqueBackgroundColorGetsSet) {
  // TODO(crbug.com/765003): CAP may make different layerization decisions and
  // we cannot guarantee that both divs will be composited in this test. When
  // CAP gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
      div {
        position: absolute;
        z-index: 1;
        width: 20px;
        height: 20px;
      }
      #behind {
        top: 12px;
        left: 12px;
        background: blue;
        will-change: transform; /* Composited */
      }
      #topleft {
        top: 0px;
        left: 0px;
        background: lime;
      }
      #bottomright {
        top: 24px;
        left: 24px;
        background: cyan;
      }
      </style>
      <div id="behind"></div>
      <div id="topleft"></div>
      <div id="bottomright"></div>
  )HTML");

  Compositor().BeginFrame();

  auto* behind_element = GetElementById("behind");
  auto* behind_layer = ContentLayerAt(ContentLayerCount() - 2);
  EXPECT_EQ(behind_layer->element_id(),
            CompositorElementIdFromUniqueObjectId(
                behind_element->GetLayoutObject()->UniqueId(),
                CompositorElementIdNamespace::kPrimary));
  EXPECT_EQ(behind_layer->SafeOpaqueBackgroundColor(), SK_ColorBLUE);

  auto* grouped_mapping =
      GetElementById("topleft")->GetLayoutBox()->Layer()->GroupedMapping();
  auto* squashed_layer = grouped_mapping->SquashingLayer()->CcLayer();
  ASSERT_NE(nullptr, squashed_layer);

  // Top left and bottom right are squashed.
  // This squashed layer should not be opaque, as it is squashing two squares
  // with some gaps between them.
  EXPECT_FALSE(squashed_layer->contents_opaque());
  // This shouldn't DCHECK.
  squashed_layer->SafeOpaqueBackgroundColor();
  // Because contents_opaque is false, the SafeOpaqueBackgroundColor() getter
  // will return SK_ColorTRANSPARENT. So we need to grab the actual color,
  // to make sure it's right.
  SkColor squashed_bg_color =
      squashed_layer->ActualSafeOpaqueBackgroundColorForTesting();
  // The squashed layer should have a non-transparent safe opaque background
  // color, that isn't blue. Exactly which color it is depends on heuristics,
  // but it should be one of the two colors of the elements that created it.
  EXPECT_NE(squashed_bg_color, SK_ColorBLUE);
  EXPECT_EQ(SkColorGetA(squashed_bg_color), SK_AlphaOPAQUE);
  // #behind is blue, which is SK_ColorBLUE
  // #topleft is lime, which is SK_ColorGREEN
  // #bottomright is cyan, which is SK_ColorCYAN
  EXPECT_TRUE((squashed_bg_color == SK_ColorGREEN) ||
              (squashed_bg_color == SK_ColorCYAN));
}

}  // namespace blink
