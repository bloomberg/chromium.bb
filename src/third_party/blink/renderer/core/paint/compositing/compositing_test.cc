// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/compositor_commit_data.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {

// Tests the integration between blink and cc where a layer list is sent to cc.
class CompositingTest : public PaintTestConfigurations, public testing::Test {
 public:
  static void ConfigureCompositingWebView(WebSettings* settings) {
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  void SetUp() override {
    web_view_helper_ = std::make_unique<frame_test_helpers::WebViewHelper>();
    web_view_helper_->Initialize(nullptr, nullptr,
                                 &ConfigureCompositingWebView);
    web_view_helper_->Resize(gfx::Size(200, 200));

    // The paint artifact compositor should have been created as part of the
    // web view helper setup.
    DCHECK(paint_artifact_compositor());
  }

  void TearDown() override { web_view_helper_.reset(); }

  // Both sets the inner html and runs the document lifecycle.
  void InitializeWithHTML(LocalFrame& frame, const String& html_content) {
    frame.GetDocument()->body()->setInnerHTML(html_content);
    frame.GetDocument()->View()->UpdateAllLifecyclePhasesForTest();
  }

  WebLocalFrame* LocalMainFrame() { return web_view_helper_->LocalMainFrame(); }

  LocalFrameView* GetLocalFrameView() {
    return web_view_helper_->LocalMainFrame()->GetFrameView();
  }

  WebViewImpl* WebView() { return web_view_helper_->GetWebView(); }

  const cc::Layer* RootCcLayer() {
    return paint_artifact_compositor()->RootLayer();
  }

  const cc::Layer* CcLayerByDOMElementId(const char* id) {
    auto layers = CcLayersByDOMElementId(RootCcLayer(), id);
    return layers.IsEmpty() ? nullptr : layers[0];
  }

  cc::LayerTreeHost* LayerTreeHost() {
    return web_view_helper_->LocalMainFrame()
        ->FrameWidgetImpl()
        ->LayerTreeHostForTesting();
  }

  Element* GetElementById(const AtomicString& id) {
    WebLocalFrameImpl* frame = web_view_helper_->LocalMainFrame();
    return frame->GetFrame()->GetDocument()->getElementById(id);
  }

  LayoutObject* GetLayoutObjectById(const AtomicString& id) {
    return GetElementById(id)->GetLayoutObject();
  }

  void UpdateAllLifecyclePhases() {
    WebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  cc::PropertyTrees* GetPropertyTrees() {
    return LayerTreeHost()->property_trees();
  }

  cc::TransformNode* GetTransformNode(const cc::Layer* layer) {
    return GetPropertyTrees()->transform_tree.Node(
        layer->transform_tree_index());
  }

  PaintArtifactCompositor* paint_artifact_compositor() {
    return GetLocalFrameView()->GetPaintArtifactCompositor();
  }

 private:
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(CompositingTest);

TEST_P(CompositingTest, DisableAndEnableAcceleratedCompositing) {
  auto* settings = GetLocalFrameView()->GetFrame().GetSettings();
  size_t num_layers = RootCcLayer()->children().size();
  EXPECT_GT(num_layers, 1u);
  settings->SetAcceleratedCompositingEnabled(false);
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(paint_artifact_compositor());
  settings->SetAcceleratedCompositingEnabled(true);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(num_layers, RootCcLayer()->children().size());
}

TEST_P(CompositingTest, DidScrollCallbackAfterScrollableAreaChanges) {
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

  auto* scrollable_area = scrollable->GetLayoutBox()->GetScrollableArea();
  EXPECT_NE(nullptr, scrollable_area);

  CompositorElementId scroll_element_id = scrollable_area->GetScrollElementId();
  const auto* overflow_scroll_layer =
      CcLayerByCcElementId(RootCcLayer(), scroll_element_id);
  const auto* scroll_node =
      RootCcLayer()
          ->layer_tree_host()
          ->property_trees()
          ->scroll_tree.FindNodeFromElementId(scroll_element_id);
  EXPECT_TRUE(scroll_node->scrollable);
  EXPECT_EQ(scroll_node->container_bounds, gfx::Size(100, 100));

  // Ensure a synthetic impl-side scroll offset propagates to the scrollable
  // area using the DidScroll callback.
  EXPECT_EQ(ScrollOffset(), scrollable_area->GetScrollOffset());
  cc::CompositorCommitData commit_data;
  commit_data.scrolls.push_back(
      {scroll_element_id, gfx::ScrollOffset(0, 1), base::nullopt});
  overflow_scroll_layer->layer_tree_host()->ApplyCompositorChanges(
      &commit_data);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(ScrollOffset(0, 1), scrollable_area->GetScrollOffset());

  // Make the scrollable area non-scrollable.
  scrollable->setAttribute(html_names::kStyleAttr, "overflow: visible");

  // Update layout without updating compositing state.
  LocalMainFrame()->ExecuteScript(
      WebScriptSource("var forceLayoutFromScript = scrollable.offsetTop;"));
  EXPECT_EQ(document->Lifecycle().GetState(), DocumentLifecycle::kLayoutClean);

  EXPECT_EQ(nullptr, scrollable->GetLayoutBox()->GetScrollableArea());

  // The web scroll layer has not been deleted yet and we should be able to
  // apply impl-side offsets without crashing.
  ASSERT_EQ(overflow_scroll_layer,
            CcLayerByCcElementId(RootCcLayer(), scroll_element_id));
  commit_data.scrolls[0] = {scroll_element_id, gfx::ScrollOffset(0, 1),
                            base::nullopt};
  overflow_scroll_layer->layer_tree_host()->ApplyCompositorChanges(
      &commit_data);

  UpdateAllLifecyclePhases();
  EXPECT_FALSE(CcLayerByCcElementId(RootCcLayer(), scroll_element_id));
}

TEST_P(CompositingTest, FrameViewScroll) {
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

  const auto* scroll_node = RootCcLayer()
                                ->layer_tree_host()
                                ->property_trees()
                                ->scroll_tree.FindNodeFromElementId(
                                    scrollable_area->GetScrollElementId());
  ASSERT_TRUE(scroll_node);
  EXPECT_TRUE(scroll_node->scrollable);

  // Ensure a synthetic impl-side scroll offset propagates to the scrollable
  // area using the DidScroll callback.
  EXPECT_EQ(ScrollOffset(), scrollable_area->GetScrollOffset());
  cc::CompositorCommitData commit_data;
  commit_data.scrolls.push_back({scrollable_area->GetScrollElementId(),
                                 gfx::ScrollOffset(0, 1), base::nullopt});
  RootCcLayer()->layer_tree_host()->ApplyCompositorChanges(&commit_data);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(ScrollOffset(0, 1), scrollable_area->GetScrollOffset());
}

TEST_P(CompositingTest, WillChangeTransformHint) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <style>
      #willChange {
        width: 100px;
        height: 100px;
        will-change: transform;
        background: blue;
      }
    </style>
    <div id="willChange"></div>
  )HTML");
  UpdateAllLifecyclePhases();
  auto* layer = CcLayerByDOMElementId("willChange");
  auto* transform_node = GetTransformNode(layer);
  EXPECT_TRUE(transform_node->will_change_transform);
}

TEST_P(CompositingTest, WillChangeTransformHintInSVG) {
  ScopedCompositeSVGForTest enable_feature(true);
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <!doctype html>
    <style>
      #willChange {
        width: 100px;
        height: 100px;
        will-change: transform;
      }
    </style>
    <svg width="200" height="200">
      <rect id="willChange" fill="blue"></rect>
    </svg>
  )HTML");
  UpdateAllLifecyclePhases();
  auto* layer = CcLayerByDOMElementId("willChange");
  auto* transform_node = GetTransformNode(layer);
  // For now will-change:transform triggers compositing for SVG, but we don't
  // pass the flag to cc to ensure raster quality.
  EXPECT_FALSE(transform_node->will_change_transform);
}

TEST_P(CompositingTest, Compositing3DTransformOnSVGModelObject) {
  ScopedCompositeSVGForTest enable_feature(true);
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <rect id="target" fill="blue" width="100" height="100"></rect>
    </svg>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(CcLayerByDOMElementId("target"));

  // Adding a 3D transform should trigger compositing.
  auto* target_element = GetElementById("target");
  target_element->setAttribute(html_names::kStyleAttr,
                               "transform: translate3d(0, 0, 1px)");
  UpdateAllLifecyclePhases();
  // |HasTransformRelatedProperty| is used in |CompositingReasonsFor3DTransform|
  // and must be set correctly.
  ASSERT_TRUE(GetLayoutObjectById("target")->HasTransformRelatedProperty());
  EXPECT_TRUE(CcLayerByDOMElementId("target"));

  // Removing a 3D transform removes the compositing trigger.
  target_element->setAttribute(html_names::kStyleAttr, "transform: none");
  UpdateAllLifecyclePhases();
  // |HasTransformRelatedProperty| is used in |CompositingReasonsFor3DTransform|
  // and must be set correctly.
  ASSERT_FALSE(GetLayoutObjectById("target")->HasTransformRelatedProperty());
  EXPECT_FALSE(CcLayerByDOMElementId("target"));

  // Adding a 2D transform should not trigger compositing.
  target_element->setAttribute(html_names::kStyleAttr,
                               "transform: translate(1px, 0)");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(CcLayerByDOMElementId("target"));

  // Switching from a 2D to a 3D transform should trigger compositing.
  target_element->setAttribute(html_names::kStyleAttr,
                               "transform: translate3d(0, 0, 1px)");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(CcLayerByDOMElementId("target"));
}

TEST_P(CompositingTest, Compositing3DTransformOnSVGBlock) {
  ScopedCompositeSVGForTest enable_feature(true);
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <text id="target" x="50" y="50">text</text>
    </svg>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(CcLayerByDOMElementId("target"));

  // Adding a 3D transform should trigger compositing.
  auto* target_element = GetElementById("target");
  target_element->setAttribute(html_names::kStyleAttr,
                               "transform: translate3d(0, 0, 1px)");
  UpdateAllLifecyclePhases();
  // |HasTransformRelatedProperty| is used in |CompositingReasonsFor3DTransform|
  // and must be set correctly.
  ASSERT_TRUE(GetLayoutObjectById("target")->HasTransformRelatedProperty());
  EXPECT_TRUE(CcLayerByDOMElementId("target"));

  // Removing a 3D transform removes the compositing trigger.
  target_element->setAttribute(html_names::kStyleAttr, "transform: none");
  UpdateAllLifecyclePhases();
  // |HasTransformRelatedProperty| is used in |CompositingReasonsFor3DTransform|
  // and must be set correctly.
  ASSERT_FALSE(GetLayoutObjectById("target")->HasTransformRelatedProperty());
  EXPECT_FALSE(CcLayerByDOMElementId("target"));

  // Adding a 2D transform should not trigger compositing.
  target_element->setAttribute(html_names::kStyleAttr,
                               "transform: translate(1px, 0)");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(CcLayerByDOMElementId("target"));

  // Switching from a 2D to a 3D transform should trigger compositing.
  target_element->setAttribute(html_names::kStyleAttr,
                               "transform: translate3d(0, 0, 1px)");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(CcLayerByDOMElementId("target"));
}

// Inlines do not support the transform property and should not be composited
// due to 3D transforms.
TEST_P(CompositingTest, NotCompositing3DTransformOnSVGInline) {
  ScopedCompositeSVGForTest enable_feature(true);
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <!doctype html>
    <svg width="200" height="200">
      <text x="50" y="50">
        text
        <tspan id="inline">tspan</tspan>
      </text>
    </svg>
  )HTML");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(CcLayerByDOMElementId("inline"));

  // Adding a 3D transform to an inline should not trigger compositing.
  auto* inline_element = GetElementById("inline");
  inline_element->setAttribute(html_names::kStyleAttr,
                               "transform: translate3d(0, 0, 1px)");
  UpdateAllLifecyclePhases();
  // |HasTransformRelatedProperty| is used in |CompositingReasonsFor3DTransform|
  // and must be set correctly.
  ASSERT_FALSE(GetLayoutObjectById("inline")->HasTransformRelatedProperty());
  EXPECT_FALSE(CcLayerByDOMElementId("inline"));
}

TEST_P(CompositingTest, PaintPropertiesWhenCompositingSVG) {
  ScopedCompositeSVGForTest enable_feature(true);
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <!doctype html>
    <style>
      #ancestor {
        opacity: 0.9;
      }
      #svg {
        opacity: 0.8;
      }
      #rect {
        width: 100px;
        height: 100px;
        will-change: transform;
        opacity: 0.7;
      }
    </style>
    <div id="ancestor">
      <svg id="svg" width="200" height="200">
        <rect width="10" height="10" fill="red"></rect>
        <rect id="rect" fill="blue" stroke-width="1" stroke="black"></rect>
      </svg>
    </div>
  )HTML");
  UpdateAllLifecyclePhases();
  auto* ancestor = CcLayerByDOMElementId("ancestor");
  auto* ancestor_effect_node =
      GetPropertyTrees()->effect_tree.Node(ancestor->effect_tree_index());
  EXPECT_EQ(ancestor_effect_node->opacity, 0.9f);

  auto* svg_root = CcLayerByDOMElementId("svg");
  auto* svg_root_effect_node =
      GetPropertyTrees()->effect_tree.Node(svg_root->effect_tree_index());
  EXPECT_EQ(svg_root_effect_node->opacity, 0.8f);
  EXPECT_EQ(svg_root_effect_node->parent_id, ancestor_effect_node->id);

  auto* rect = CcLayerByDOMElementId("rect");
  auto* rect_filter_node =
      GetPropertyTrees()->effect_tree.Node(rect->effect_tree_index());
  EXPECT_EQ(rect_filter_node->opacity, 1);

  auto* rect_effect_node =
      GetPropertyTrees()->effect_tree.Node(rect_filter_node->parent_id);
  EXPECT_EQ(rect_effect_node->opacity, 0.7f);
  EXPECT_EQ(rect_effect_node->parent_id, svg_root_effect_node->id);
}

TEST_P(CompositingTest, BackgroundColorInScrollingContentsLayer) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <style>
      html {
        background-color: rgb(10, 20, 30);
      }
      #scroller {
        will-change: transform;
        overflow: scroll;
        height: 100px;
        width: 100px;
        background-color: rgb(30, 40, 50);
      }
      .spacer {
        height: 1000px;
      }
    </style>
    <div id="scroller">
      <div class="spacer"></div>
    </div>
    <div class="spacer"></div>
  )HTML");
  UpdateAllLifecyclePhases();

  LayoutView* layout_view = GetLocalFrameView()->GetLayoutView();
  Element* scroller = GetElementById("scroller");
  LayoutBox* scroller_box = scroller->GetLayoutBox();
  ASSERT_TRUE(layout_view->GetBackgroundPaintLocation() ==
              kBackgroundPaintInScrollingContents);
  ASSERT_TRUE(scroller_box->GetBackgroundPaintLocation() ==
              kBackgroundPaintInScrollingContents);

  // The root layer and root scrolling contents layer get background_color by
  // blending the CSS background-color of the <html> element with
  // LocalFrameView::BaseBackgroundColor(), which is white by default.
  auto* layer = CcLayersByName(RootCcLayer(), "LayoutView #document")[0];
  SkColor expected_color = SkColorSetRGB(10, 20, 30);
  EXPECT_EQ(layer->background_color(), SK_ColorTRANSPARENT);
  auto* scrollable_area = GetLocalFrameView()->LayoutViewport();
  layer = ScrollingContentsCcLayerByScrollElementId(
      RootCcLayer(), scrollable_area->GetScrollElementId());
  EXPECT_EQ(layer->background_color(), expected_color);

  // Non-root layers set background_color based on the CSS background color of
  // the layer-defining element.
  expected_color = SkColorSetRGB(30, 40, 50);
  layer = CcLayerByDOMElementId("scroller");
  EXPECT_EQ(layer->background_color(), SK_ColorTRANSPARENT);
  scrollable_area = scroller_box->GetScrollableArea();
  layer = ScrollingContentsCcLayerByScrollElementId(
      RootCcLayer(), scrollable_area->GetScrollElementId());
  EXPECT_EQ(layer->background_color(), expected_color);
}

TEST_P(CompositingTest, BackgroundColorInGraphicsLayer) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <style>
      html {
        background-image: linear-gradient(rgb(10, 20, 30), rgb(60, 70, 80));
        background-attachment: fixed;
      }
      #scroller {
        will-change: transform;
        overflow: scroll;
        height: 100px;
        width: 100px;
        background-color: rgba(30, 40, 50, .6);
        background-clip: content-box;
        background-attachment: scroll;
        padding: 1px;
      }
      .spacer {
        height: 1000px;
      }
    </style>
    <div id="scroller">
      <div class="spacer"></div>
    </div>
    <div class="spacer"></div>
  )HTML");
  UpdateAllLifecyclePhases();

  LayoutView* layout_view = GetLocalFrameView()->GetLayoutView();
  Element* scroller = GetElementById("scroller");
  LayoutBox* scroller_box = scroller->GetLayoutBox();
  ASSERT_TRUE(layout_view->GetBackgroundPaintLocation() ==
              kBackgroundPaintInGraphicsLayer);
  ASSERT_TRUE(scroller_box->GetBackgroundPaintLocation() ==
              kBackgroundPaintInGraphicsLayer);

  // The root layer gets background_color by blending the CSS background-color
  // of the <html> element with LocalFrameView::BaseBackgroundColor(), which is
  // white by default. In this case, because the background is a gradient, it
  // will blend transparent with white, resulting in white. Because the
  // background is painted into the root graphics layer, the root scrolling
  // contents layer should not checkerboard, so its background color should be
  // transparent.
  auto* layer = CcLayersByName(RootCcLayer(), "LayoutView #document")[0];
  EXPECT_EQ(layer->background_color(), SK_ColorWHITE);
  auto* scrollable_area = GetLocalFrameView()->LayoutViewport();
  layer = ScrollingContentsCcLayerByScrollElementId(
      RootCcLayer(), scrollable_area->GetScrollElementId());
  EXPECT_EQ(layer->background_color(), SK_ColorTRANSPARENT);
  EXPECT_EQ(layer->SafeOpaqueBackgroundColor(), SK_ColorTRANSPARENT);

  // Non-root layers set background_color based on the CSS background color of
  // the layer-defining element.
  SkColor expected_color = SkColorSetARGB(roundf(255. * 0.6), 30, 40, 50);
  layer = CcLayerByDOMElementId("scroller");
  EXPECT_EQ(layer->background_color(), expected_color);
  scrollable_area = scroller_box->GetScrollableArea();
  layer = ScrollingContentsCcLayerByScrollElementId(
      RootCcLayer(), scrollable_area->GetScrollElementId());
  EXPECT_EQ(layer->background_color(), SK_ColorTRANSPARENT);
  EXPECT_EQ(layer->SafeOpaqueBackgroundColor(), SK_ColorTRANSPARENT);
}

TEST_P(CompositingTest, ContainPaintLayerBounds) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <div id="target" style="will-change: transform; contain: paint;
                            width: 200px; height: 100px">
      <div style="width: 300px; height: 400px"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhases();
  auto* layer = CcLayersByDOMElementId(RootCcLayer(), "target")[0];
  ASSERT_TRUE(layer);
  EXPECT_EQ(gfx::Size(200, 100), layer->bounds());
}

TEST_P(CompositingTest, SVGForeignObjectDirectlyCompositedContainer) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <!doctype html>
    <div id="container" style="backface-visibility: hidden">
      <svg>
        <foreignObject id="foreign">
          <div id="child" style="position: relative"></div>
        </foreignObject>
      </svg>
    </body>
  )HTML");
  UpdateAllLifecyclePhases();

  // PaintInvalidator should use the same directly_composited_container during
  // PrePaint and should not fail DCHECK.
  auto* container = GetLayoutObjectById("container");
  EXPECT_FALSE(container->IsStackingContext());
  EXPECT_EQ(container,
            &GetLayoutObjectById("foreign")->DirectlyCompositableContainer());
  EXPECT_EQ(container,
            &GetLayoutObjectById("child")->DirectlyCompositableContainer());
}

class CompositingSimTest : public PaintTestConfigurations, public SimTest {
 public:
  void InitializeWithHTML(const String& html) {
    SimRequest request("https://example.com/test.html", "text/html");
    LoadURL("https://example.com/test.html");
    request.Complete(html);
    UpdateAllLifecyclePhases();
    DCHECK(paint_artifact_compositor());
  }

  const cc::Layer* RootCcLayer() {
    return paint_artifact_compositor()->RootLayer();
  }

  const cc::Layer* CcLayerByDOMElementId(const char* id) {
    auto layers = CcLayersByDOMElementId(RootCcLayer(), id);
    return layers.IsEmpty() ? nullptr : layers[0];
  }

  const cc::Layer* CcLayerByOwnerNodeId(Node* node) {
    DOMNodeId id = DOMNodeIds::IdForNode(node);
    for (auto& layer : RootCcLayer()->children()) {
      if (layer->debug_info() && layer->debug_info()->owner_node_id == id)
        return layer.get();
    }
    return nullptr;
  }

  Element* GetElementById(const AtomicString& id) {
    return MainFrame().GetFrame()->GetDocument()->getElementById(id);
  }

  void UpdateAllLifecyclePhases() {
    WebView().MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  void UpdateAllLifecyclePhasesExceptPaint() {
    WebView().MainFrameWidget()->UpdateLifecycle(WebLifecycleUpdate::kPrePaint,
                                                 DocumentUpdateReason::kTest);
  }

  cc::PropertyTrees* GetPropertyTrees() {
    return Compositor().LayerTreeHost()->property_trees();
  }

  cc::TransformNode* GetTransformNode(const cc::Layer* layer) {
    return GetPropertyTrees()->transform_tree.Node(
        layer->transform_tree_index());
  }

  cc::EffectNode* GetEffectNode(const cc::Layer* layer) {
    return GetPropertyTrees()->effect_tree.Node(layer->effect_tree_index());
  }

  PaintArtifactCompositor* paint_artifact_compositor() {
    return MainFrame().GetFrameView()->GetPaintArtifactCompositor();
  }

 private:
  void SetUp() override {
    SimTest::SetUp();
    // Ensure a non-empty size so painting does not early-out.
    WebView().Resize(gfx::Size(800, 600));
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(CompositingSimTest);

TEST_P(CompositingSimTest, LayerUpdatesDoNotInvalidateEarlierLayers) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        div {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightblue;
        }
      </style>
      <div id='a'></div>
      <div id='b'></div>
  )HTML");

  Compositor().BeginFrame();

  auto* a_layer = CcLayerByDOMElementId("a");
  auto* b_element = GetElementById("b");
  auto* b_layer = CcLayerByDOMElementId("b");

  // Initially, neither a nor b should have a layer that should push properties.
  cc::LayerTreeHost& host = *Compositor().LayerTreeHost();
  EXPECT_FALSE(host.LayersThatShouldPushProperties().count(a_layer));
  EXPECT_FALSE(host.LayersThatShouldPushProperties().count(b_layer));

  // Modifying b should only cause the b layer to need to push properties.
  b_element->setAttribute(html_names::kStyleAttr, "opacity: 0.2");
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(host.LayersThatShouldPushProperties().count(a_layer));
  EXPECT_TRUE(host.LayersThatShouldPushProperties().count(b_layer));

  // After a frame, no layers should need to push properties again.
  Compositor().BeginFrame();
  EXPECT_FALSE(host.LayersThatShouldPushProperties().count(a_layer));
  EXPECT_FALSE(host.LayersThatShouldPushProperties().count(b_layer));
}

TEST_P(CompositingSimTest, LayerUpdatesDoNotInvalidateLaterLayers) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        div {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightblue;
        }
      </style>
      <div id='a'></div>
      <div id='b' style='opacity: 0.2;'></div>
      <div id='c'></div>
  )HTML");

  Compositor().BeginFrame();

  auto* a_element = GetElementById("a");
  auto* a_layer = CcLayerByDOMElementId("a");
  auto* b_element = GetElementById("b");
  auto* b_layer = CcLayerByDOMElementId("b");
  auto* c_layer = CcLayerByDOMElementId("c");

  // Initially, no layer should need to push properties.
  cc::LayerTreeHost& host = *Compositor().LayerTreeHost();
  EXPECT_FALSE(host.LayersThatShouldPushProperties().count(a_layer));
  EXPECT_FALSE(host.LayersThatShouldPushProperties().count(b_layer));
  EXPECT_FALSE(host.LayersThatShouldPushProperties().count(c_layer));

  // Modifying a and b (adding opacity to a and removing opacity from b) should
  // not cause the c layer to push properties.
  a_element->setAttribute(html_names::kStyleAttr, "opacity: 0.3");
  b_element->setAttribute(html_names::kStyleAttr, "");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(host.LayersThatShouldPushProperties().count(a_layer));
  EXPECT_TRUE(host.LayersThatShouldPushProperties().count(b_layer));
  EXPECT_FALSE(host.LayersThatShouldPushProperties().count(c_layer));

  // After a frame, no layers should need to push properties again.
  Compositor().BeginFrame();
  EXPECT_FALSE(host.LayersThatShouldPushProperties().count(a_layer));
  EXPECT_FALSE(host.LayersThatShouldPushProperties().count(b_layer));
  EXPECT_FALSE(host.LayersThatShouldPushProperties().count(c_layer));
}

TEST_P(CompositingSimTest,
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
  cc::LayerTreeHost& layer_tree_host = *Compositor().LayerTreeHost();
  EXPECT_FALSE(layer_tree_host.needs_full_tree_sync());
  int sequence_number = GetPropertyTrees()->sequence_number;
  EXPECT_GT(sequence_number, 0);

  // A no-op update should not cause the host to need a full tree sync.
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(layer_tree_host.needs_full_tree_sync());
  // It should also not cause a property tree update - the sequence number
  // should not change.
  EXPECT_EQ(sequence_number, GetPropertyTrees()->sequence_number);
}

// When a property tree change occurs that affects layer transform in the
// general case, all layers associated with the changed property tree node, and
// all layers associated with a descendant of the changed property tree node
// need to have |subtree_property_changed| set for damage tracking. In
// non-layer-list mode, this occurs in BuildPropertyTreesInternal (see:
// SetLayerPropertyChangedForChild).
TEST_P(CompositingSimTest, LayerSubtreeTransformPropertyChanged) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        #outer {
          width: 100px;
          height: 100px;
          will-change: transform;
          transform: translate(10px, 10px);
          background: lightgreen;
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
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  auto* inner_element_layer = CcLayerByDOMElementId("inner");

  // Initially, no layer should have |subtree_property_changed| set.
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetTransformNode(outer_element_layer)->transform_changed);
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetTransformNode(inner_element_layer)->transform_changed);

  // Modifying the transform style should set |subtree_property_changed| on
  // both layers.
  outer_element->setAttribute(html_names::kStyleAttr,
                              "transform: rotate(10deg)");
  UpdateAllLifecyclePhases();
  // This is still set by the traditional GraphicsLayer::SetTransform().
  EXPECT_TRUE(outer_element_layer->subtree_property_changed());
  // Set by blink::PropertyTreeManager.
  EXPECT_TRUE(GetTransformNode(outer_element_layer)->transform_changed);
  // TODO(wangxianzhu): Probably avoid setting this flag on transform change.
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetTransformNode(inner_element_layer)->transform_changed);

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetTransformNode(outer_element_layer)->transform_changed);
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetTransformNode(inner_element_layer)->transform_changed);
}

// When a property tree change occurs that affects layer transform in a simple
// case (ie before and after transforms both preserve axis alignment), the
// transforms can be directly updated without explicitly marking layers as
// damaged. The ensure damage occurs, the transform node should have
// |transform_changed| set. In non-layer-list mode, this occurs in
// cc::TransformTree::OnTransformAnimated and cc::Layer::SetTransform.
TEST_P(CompositingSimTest, DirectTransformPropertyUpdate) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        #outer {
          width: 100px;
          height: 100px;
          will-change: transform;
          transform: translate(10px, 10px) scale(1, 2);
          background: lightgreen;
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
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  auto transform_tree_index = outer_element_layer->transform_tree_index();
  auto* transform_node =
      GetPropertyTrees()->transform_tree.Node(transform_tree_index);

  // Initially, transform should be unchanged.
  EXPECT_FALSE(transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Modifying the transform in a simple way allowed for a direct update.
  outer_element->setAttribute(html_names::kStyleAttr,
                              "transform: translate(30px, 20px) scale(5, 5)");
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // After a frame the |transform_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(transform_node->transform_changed);
}

TEST_P(CompositingSimTest, DirectSVGTransformPropertyUpdate) {
  ScopedCompositeSVGForTest enable_feature(true);
  InitializeWithHTML(R"HTML(
    <!doctype html>
    <style>
      #willChange {
        width: 100px;
        height: 100px;
        will-change: transform;
        transform: translate(10px, 10px);
      }
    </style>
    <svg width="200" height="200">
      <rect id="willChange" fill="blue"></rect>
    </svg>
  )HTML");

  Compositor().BeginFrame();

  auto* will_change_layer = CcLayerByDOMElementId("willChange");
  auto transform_tree_index = will_change_layer->transform_tree_index();
  auto* transform_node =
      GetPropertyTrees()->transform_tree.Node(transform_tree_index);

  // Initially, transform should be unchanged.
  EXPECT_FALSE(transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Modifying the transform in a simple way allowed for a direct update.
  auto* will_change_element = GetElementById("willChange");
  will_change_element->setAttribute(html_names::kStyleAttr,
                                    "transform: translate(30px, 20px)");
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // After a frame the |transform_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(transform_node->transform_changed);
}

// This test is similar to |DirectTransformPropertyUpdate| but tests that
// the changed value of a directly updated transform is still set if some other
// change causes PaintArtifactCompositor to run and do non-direct updates.
TEST_P(CompositingSimTest, DirectTransformPropertyUpdateCausesChange) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        #outer {
          width: 100px;
          height: 100px;
          will-change: transform;
          transform: translate(1px, 2px);
          background: lightgreen;
        }
        #inner {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightblue;
          transform: translate(3px, 4px);
        }
      </style>
      <div id='outer'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* outer_element = GetElementById("outer");
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  auto outer_transform_tree_index = outer_element_layer->transform_tree_index();
  auto* outer_transform_node =
      GetPropertyTrees()->transform_tree.Node(outer_transform_tree_index);

  auto* inner_element = GetElementById("inner");
  auto* inner_element_layer = CcLayerByDOMElementId("inner");
  auto inner_transform_tree_index = inner_element_layer->transform_tree_index();
  auto* inner_transform_node =
      GetPropertyTrees()->transform_tree.Node(inner_transform_tree_index);

  // Initially, the transforms should be unchanged.
  EXPECT_FALSE(outer_transform_node->transform_changed);
  EXPECT_FALSE(inner_transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Modifying the outer transform in a simple way should allow for a direct
  // update of the outer transform. Modifying the inner transform in a
  // non-simple way should not allow for a direct update of the inner transform.
  outer_element->setAttribute(html_names::kStyleAttr,
                              "transform: translate(5px, 6px)");
  inner_element->setAttribute(html_names::kStyleAttr,
                              "transform: rotate(30deg)");
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(outer_transform_node->transform_changed);
  EXPECT_FALSE(inner_transform_node->transform_changed);
  EXPECT_TRUE(paint_artifact_compositor()->NeedsUpdate());

  // After a PaintArtifactCompositor update, which was needed due to the inner
  // element's transform change, both the inner and outer transform nodes
  // should be marked as changed to ensure they result in damage.
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(outer_transform_node->transform_changed);
  EXPECT_TRUE(inner_transform_node->transform_changed);

  // After a frame the |transform_changed| values should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_transform_node->transform_changed);
  EXPECT_FALSE(inner_transform_node->transform_changed);
}

// This test ensures that the correct transform nodes are created and bits set
// so that the browser controls movement adjustments needed by bottom-fixed
// elements will work.
TEST_P(CompositingSimTest, AffectedByOuterViewportBoundsDelta) {
  // TODO(bokan): This test will have to be reevaluated for CAP. It looks like
  // the fixed layer isn't composited in CAP.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        body { height: 2000px; }
        #fixed {
          width: 100px;
          height: 100px;
          position: fixed;
          left: 0;
          background-color: red;
        }
      </style>
      <div id='fixed'></div>
  )HTML");

  auto* fixed_element = GetElementById("fixed");
  auto* fixed_element_layer = CcLayerByDOMElementId("fixed");
  DCHECK_EQ(fixed_element_layer->element_id(),
            CompositorElementIdFromUniqueObjectId(
                fixed_element->GetLayoutObject()->UniqueId(),
                CompositorElementIdNamespace::kPrimary));

  // Fix the DIV to the bottom of the viewport. Since the viewport height will
  // expand/contract, the fixed element will need to be moved as the bounds
  // delta changes.
  {
    fixed_element->setAttribute(html_names::kStyleAttr, "bottom: 0");
    Compositor().BeginFrame();

    auto transform_tree_index = fixed_element_layer->transform_tree_index();
    auto* transform_node =
        GetPropertyTrees()->transform_tree.Node(transform_tree_index);

    DCHECK(transform_node);
    EXPECT_TRUE(transform_node->moved_by_outer_viewport_bounds_delta_y);
  }

  // Fix it to the top now. Since the top edge doesn't change (relative to the
  // renderer origin), we no longer need to move it as the bounds delta
  // changes.
  {
    fixed_element->setAttribute(html_names::kStyleAttr, "top: 0");
    Compositor().BeginFrame();

    auto transform_tree_index = fixed_element_layer->transform_tree_index();
    auto* transform_node =
        GetPropertyTrees()->transform_tree.Node(transform_tree_index);

    DCHECK(transform_node);
    EXPECT_FALSE(transform_node->moved_by_outer_viewport_bounds_delta_y);
  }
}

// When a property tree change occurs that affects layer transform-origin, the
// transform can be directly updated without explicitly marking the layer as
// damaged. The ensure damage occurs, the transform node should have
// |transform_changed| set. In non-layer-list mode, this occurs in
// cc::Layer::SetTransformOrigin.
TEST_P(CompositingSimTest, DirectTransformOriginPropertyUpdate) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        html { overflow: hidden; }
        #box {
          width: 100px;
          height: 100px;
          transform: rotate3d(3, 2, 1, 45deg);
          transform-origin: 10px 10px 100px;
          background: lightblue;
        }
      </style>
      <div id='box'></div>
  )HTML");

  Compositor().BeginFrame();

  auto* box_element = GetElementById("box");
  auto* box_element_layer = CcLayerByDOMElementId("box");
  auto transform_tree_index = box_element_layer->transform_tree_index();
  auto* transform_node =
      GetPropertyTrees()->transform_tree.Node(transform_tree_index);

  // Initially, transform should be unchanged.
  EXPECT_FALSE(transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Modifying the transform-origin in a simple way allowed for a direct update.
  box_element->setAttribute(html_names::kStyleAttr,
                            "transform-origin: -10px -10px -100px");
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(transform_node->transform_changed);
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // After a frame the |transform_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(transform_node->transform_changed);
}

// This test is similar to |LayerSubtreeTransformPropertyChanged| but for
// effect property node changes.
TEST_P(CompositingSimTest, LayerSubtreeEffectPropertyChanged) {
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
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  DCHECK_EQ(outer_element_layer->element_id(),
            CompositorElementIdFromUniqueObjectId(
                outer_element->GetLayoutObject()->UniqueId(),
                CompositorElementIdNamespace::kPrimary));
  auto* inner_element = GetElementById("inner");
  auto* inner_element_layer = CcLayerByDOMElementId("inner");
  DCHECK_EQ(inner_element_layer->element_id(),
            CompositorElementIdFromUniqueObjectId(
                inner_element->GetLayoutObject()->UniqueId(),
                CompositorElementIdNamespace::kPrimary));

  // Initially, no layer should have |subtree_property_changed| set.
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetEffectNode(outer_element_layer)->effect_changed);
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetEffectNode(inner_element_layer)->effect_changed);

  // Modifying the filter style should set |subtree_property_changed| on
  // both layers.
  outer_element->setAttribute(html_names::kStyleAttr, "filter: blur(20px)");
  UpdateAllLifecyclePhases();
  // TODO(wangxianzhu): Probably avoid setting this flag on transform change.
  EXPECT_TRUE(outer_element_layer->subtree_property_changed());
  // Set by blink::PropertyTreeManager.
  EXPECT_TRUE(GetEffectNode(outer_element_layer)->effect_changed);
  // TODO(wangxianzhu): Probably avoid setting this flag on transform change.
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetEffectNode(inner_element_layer)->effect_changed);

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetEffectNode(outer_element_layer)->effect_changed);
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
  EXPECT_FALSE(GetEffectNode(inner_element_layer)->effect_changed);
}

// This test is similar to |LayerSubtreeTransformPropertyChanged| but for
// clip property node changes.
TEST_P(CompositingSimTest, LayerSubtreeClipPropertyChanged) {
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
          background: lightgreen;
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
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  auto* inner_element_layer = CcLayerByDOMElementId("inner");

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

TEST_P(CompositingSimTest, LayerSubtreeOverflowClipPropertyChanged) {
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
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  auto* inner_element = GetElementById("inner");
  auto* inner_element_layer = CcLayerByDOMElementId("inner");
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
  // The overflow clip does not affect |outer_element_layer|, so
  // subtree_property_changed should be false for it. It does affect
  // |inner_element_layer| though.
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(outer_element_layer->subtree_property_changed());
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
}

// This test is similar to |LayerSubtreeClipPropertyChanged| but for cases when
// the clip node itself does not change but the clip node associated with a
// layer changes.
TEST_P(CompositingSimTest, LayerClipPropertyChanged) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #outer {
          width: 100px;
          height: 100px;
        }
        #inner {
          width: 50px;
          height: 200px;
          backface-visibility: hidden;
          background: lightblue;
        }
      </style>
      <div id='outer' style='overflow: hidden;'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* inner_element_layer = CcLayerByDOMElementId("inner");
  EXPECT_TRUE(inner_element_layer->should_check_backface_visibility());

  // Initially, no layer should have |subtree_property_changed| set.
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());

  // Removing overflow: hidden on the outer div should set
  // |subtree_property_changed| on the inner div's cc::Layer.
  auto* outer_element = GetElementById("outer");
  outer_element->setAttribute(html_names::kStyleAttr, "");
  UpdateAllLifecyclePhases();

  inner_element_layer = CcLayerByDOMElementId("inner");
  EXPECT_TRUE(inner_element_layer->should_check_backface_visibility());
  EXPECT_TRUE(inner_element_layer->subtree_property_changed());

  // After a frame the |subtree_property_changed| value should be reset.
  Compositor().BeginFrame();
  EXPECT_FALSE(inner_element_layer->subtree_property_changed());
}

TEST_P(CompositingSimTest, SafeOpaqueBackgroundColor) {
  InitializeWithHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      body { background: yellow; }
      div {
        position: absolute;
        z-index: 1;
        width: 20px;
        height: 20px;
        will-change: transform; /* Composited */
      }
      #opaque-color {
        background: blue;
      }
      #opaque-image, #opaque-image-translucent-color {
        background: linear-gradient(blue, green);
      }
      #partly-opaque div {
        width: 15px;
        height: 15px;
        background: blue;
        will-change: initial;
      }
      #translucent, #opaque-image-translucent-color div {
        background: rgba(0, 255, 255, 0.5);
        will-change: initial;
      }
    </style>
    <div id="opaque-color"></div>
    <div id="opaque-image"></div>
    <div id="opaque-image-translucent-color">
      <div></div>
    </div>
    <div id="partly-opaque">
      <div></div>
    </div>
    <div id="translucent"></div>
  )HTML");

  Compositor().BeginFrame();

  auto* opaque_color = CcLayerByDOMElementId("opaque-color");
  EXPECT_TRUE(opaque_color->contents_opaque());
  EXPECT_EQ(SK_ColorBLUE, opaque_color->background_color());
  EXPECT_EQ(SK_ColorBLUE, opaque_color->SafeOpaqueBackgroundColor());

  auto* opaque_image = CcLayerByDOMElementId("opaque-image");
  EXPECT_TRUE(opaque_image->contents_opaque());
  EXPECT_EQ(SK_ColorTRANSPARENT, opaque_image->background_color());
  // Fallback to use the viewport background.
  EXPECT_EQ(SK_ColorYELLOW, opaque_image->SafeOpaqueBackgroundColor());

  const SkColor kTranslucentCyan = SkColorSetARGB(128, 0, 255, 255);
  auto* opaque_image_translucent_color =
      CcLayerByDOMElementId("opaque-image-translucent-color");
  EXPECT_TRUE(opaque_image_translucent_color->contents_opaque());
  EXPECT_EQ(kTranslucentCyan,
            opaque_image_translucent_color->background_color());
  // Use background_color() with the alpha channel forced to be opaque.
  EXPECT_EQ(SK_ColorCYAN,
            opaque_image_translucent_color->SafeOpaqueBackgroundColor());

  auto* partly_opaque = CcLayerByDOMElementId("partly-opaque");
  EXPECT_FALSE(partly_opaque->contents_opaque());
  EXPECT_EQ(SK_ColorBLUE, partly_opaque->background_color());
  // SafeOpaqueBackgroundColor() returns SK_ColorTRANSPARENT when
  // background_color() is opaque and contents_opaque() is false.
  EXPECT_EQ(SK_ColorTRANSPARENT, partly_opaque->SafeOpaqueBackgroundColor());

  auto* translucent = CcLayerByDOMElementId("translucent");
  EXPECT_FALSE(translucent->contents_opaque());
  EXPECT_EQ(kTranslucentCyan, translucent->background_color());
  // SafeOpaqueBackgroundColor() returns background_color() if it's not opaque
  // and contents_opaque() is false.
  EXPECT_EQ(kTranslucentCyan, translucent->SafeOpaqueBackgroundColor());
}

TEST_P(CompositingSimTest, SquashingLayerSafeOpaqueBackgroundColor) {
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
        width: 100px;
        height: 100px;
        background: cyan;
      }
    </style>
    <div id="behind"></div>
    <div id="topleft"></div>
    <div id="bottomright"></div>
  )HTML");

  Compositor().BeginFrame();

  auto* squashing_layer = CcLayerByDOMElementId("topleft");
  ASSERT_TRUE(squashing_layer);
  EXPECT_EQ(gfx::Size(124, 124), squashing_layer->bounds());

  // Top left and bottom right are squashed.
  // This squashed layer should not be opaque, as it is squashing two squares
  // with some gaps between them.
  EXPECT_FALSE(squashing_layer->contents_opaque());
  // The background color of #bottomright is used as the background color
  // because it covers the most significant area of the squashing layer.
  EXPECT_EQ(squashing_layer->background_color(), SK_ColorCYAN);
  // SafeOpaqueBackgroundColor() returns SK_ColorTRANSPARENT when
  // background_color() is opaque and contents_opaque() is false.
  EXPECT_EQ(squashing_layer->SafeOpaqueBackgroundColor(), SK_ColorTRANSPARENT);
}

// Test that a pleasant checkerboard color is used in the presence of blending.
TEST_P(CompositingSimTest, RootScrollingContentsSafeOpaqueBackgroundColor) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <div style="mix-blend-mode: multiply;"></div>
      <div id="forcescroll" style="height: 10000px;"></div>
  )HTML");
  Compositor().BeginFrame();

  auto* scrolling_contents = ScrollingContentsCcLayerByScrollElementId(
      RootCcLayer(),
      MainFrame().GetFrameView()->LayoutViewport()->GetScrollElementId());
  EXPECT_EQ(scrolling_contents->background_color(), SK_ColorWHITE);
  EXPECT_EQ(scrolling_contents->SafeOpaqueBackgroundColor(), SK_ColorWHITE);
}

TEST_P(CompositingSimTest, NonDrawableLayersIgnoredForRenderSurfaces) {
  // TODO(crbug.com/765003): CAP may make different layerization decisions. When
  // CAP gets closer to launch, this test should be updated to pass.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #outer {
          width: 100px;
          height: 100px;
          opacity: 0.5;
          background: blue;
        }
        #inner {
          width: 10px;
          height: 10px;
          will-change: transform;
        }
      </style>
      <div id='outer'>
        <div id='inner'></div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  auto* inner_element_layer = CcLayerByDOMElementId("inner");
  EXPECT_FALSE(inner_element_layer->DrawsContent());
  auto* outer_element_layer = CcLayerByDOMElementId("outer");
  EXPECT_TRUE(outer_element_layer->DrawsContent());

  // The inner element layer is only needed for hit testing and does not draw
  // content, so it should not cause a render surface.
  auto effect_tree_index = outer_element_layer->effect_tree_index();
  auto* effect_node = GetPropertyTrees()->effect_tree.Node(effect_tree_index);
  EXPECT_EQ(effect_node->opacity, 0.5f);
  EXPECT_FALSE(effect_node->HasRenderSurface());
}

TEST_P(CompositingSimTest, NoRenderSurfaceWithAxisAlignedTransformAnimation) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        @keyframes translation {
          0% { transform: translate(10px, 11px); }
          100% { transform: translate(20px, 21px); }
        }
        .animate {
          animation-name: translation;
          animation-duration: 1s;
          width: 100px;
          height: 100px;
          overflow: hidden;
        }
        .compchild {
          height: 200px;
          width: 10px;
          background: lightblue;
          will-change: transform;
        }
      </style>
      <div class="animate"><div class="compchild"></div></div>
  )HTML");
  Compositor().BeginFrame();
  // No effect node with kClipAxisAlignment should be created because the
  // animation is axis-aligned.
  for (const auto& effect_node : GetPropertyTrees()->effect_tree.nodes()) {
    EXPECT_NE(cc::RenderSurfaceReason::kClipAxisAlignment,
              effect_node.render_surface_reason);
  }
}

TEST_P(CompositingSimTest, PromoteCrossOriginIframe) {
  InitializeWithHTML("<!DOCTYPE html><iframe id=iframe sandbox></iframe>");
  Compositor().BeginFrame();
  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("iframe"))->contentDocument();
  Node* owner_node = iframe_doc;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    owner_node = iframe_doc->documentElement();
  auto* layer = CcLayerByOwnerNodeId(owner_node);
  EXPECT_TRUE(layer);
  EXPECT_EQ(layer->bounds(), gfx::Size(300, 150));
}

// On initial layout, the iframe is not yet loaded and is not considered
// cross origin. This test ensures the iframe is promoted due to being cross
// origin after the iframe loads.
TEST_P(CompositingSimTest, PromoteCrossOriginIframeAfterLoading) {
  SimRequest main_resource("https://origin-a.com/a.html", "text/html");
  SimRequest frame_resource("https://origin-b.com/b.html", "text/html");

  LoadURL("https://origin-a.com/a.html");
  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="iframe" src="https://origin-b.com/b.html"></iframe>
  )HTML");
  frame_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();

  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("iframe"))->contentDocument();
  Node* owner_node = iframe_doc;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    owner_node = iframe_doc->documentElement();
  EXPECT_TRUE(CcLayerByOwnerNodeId(owner_node));
}

// An iframe that is cross-origin to the parent should be composited. This test
// sets up nested frames with domains A -> B -> A. Both the child and grandchild
// frames should be composited because they are cross-origin to their parent.
TEST_P(CompositingSimTest, PromoteCrossOriginToParent) {
  SimRequest main_resource("https://origin-a.com/a.html", "text/html");
  SimRequest child_resource("https://origin-b.com/b.html", "text/html");
  SimRequest grandchild_resource("https://origin-a.com/c.html", "text/html");

  LoadURL("https://origin-a.com/a.html");
  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="main_iframe" src="https://origin-b.com/b.html"></iframe>
  )HTML");
  child_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="child_iframe" src="https://origin-a.com/c.html"></iframe>
  )HTML");
  grandchild_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();

  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("main_iframe"))
          ->contentDocument();
  EXPECT_TRUE(CcLayerByOwnerNodeId(iframe_doc));

  iframe_doc =
      To<HTMLFrameOwnerElement>(iframe_doc->getElementById("child_iframe"))
          ->contentDocument();
  Node* owner_node = iframe_doc;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    owner_node = iframe_doc->documentElement();
  EXPECT_TRUE(CcLayerByOwnerNodeId(owner_node));
}

// Initially the iframe is cross-origin and should be composited. After changing
// to same-origin, the frame should no longer be composited.
TEST_P(CompositingSimTest, PromoteCrossOriginIframeAfterDomainChange) {
  SimRequest main_resource("https://origin-a.com/a.html", "text/html");
  SimRequest frame_resource("https://sub.origin-a.com/b.html", "text/html");

  LoadURL("https://origin-a.com/a.html");
  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="iframe" src="https://sub.origin-a.com/b.html"></iframe>
  )HTML");
  frame_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();

  auto* iframe_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("iframe"));

  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("iframe"))->contentDocument();
  Node* owner_node = iframe_doc;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    owner_node = iframe_doc->documentElement();
  EXPECT_TRUE(CcLayerByOwnerNodeId(owner_node));

  NonThrowableExceptionState exception_state;
  GetDocument().setDomain(String("origin-a.com"), exception_state);
  iframe_element->contentDocument()->setDomain(String("origin-a.com"),
                                               exception_state);
  // We may not have scheduled a visual update so force an update instead of
  // using BeginFrame.
  UpdateAllLifecyclePhases();

  iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("iframe"))->contentDocument();
  owner_node = iframe_doc;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    owner_node = iframe_doc->documentElement();
  EXPECT_FALSE(CcLayerByOwnerNodeId(owner_node));
}

// This test sets up nested frames with domains A -> B -> A. Initially, the
// child frame and grandchild frame should be composited. After changing the
// child frame to A (same-origin), both child and grandchild frames should no
// longer be composited.
TEST_P(CompositingSimTest, PromoteCrossOriginToParentIframeAfterDomainChange) {
  SimRequest main_resource("https://origin-a.com/a.html", "text/html");
  SimRequest child_resource("https://sub.origin-a.com/b.html", "text/html");
  SimRequest grandchild_resource("https://origin-a.com/c.html", "text/html");

  LoadURL("https://origin-a.com/a.html");
  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="main_iframe" src="https://sub.origin-a.com/b.html"></iframe>
  )HTML");
  child_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="child_iframe" src="https://origin-a.com/c.html"></iframe>
  )HTML");
  grandchild_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();

  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("main_iframe"))
          ->contentDocument();
  EXPECT_TRUE(CcLayerByOwnerNodeId(iframe_doc));

  iframe_doc =
      To<HTMLFrameOwnerElement>(iframe_doc->getElementById("child_iframe"))
          ->contentDocument();
  Node* owner_node = iframe_doc;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    owner_node = iframe_doc->documentElement();
  EXPECT_TRUE(CcLayerByOwnerNodeId(owner_node));

  auto* main_iframe_element =
      To<HTMLIFrameElement>(GetDocument().getElementById("main_iframe"));
  NonThrowableExceptionState exception_state;

  GetDocument().setDomain(String("origin-a.com"), exception_state);
  auto* child_iframe_element = To<HTMLIFrameElement>(
      main_iframe_element->contentDocument()->getElementById("child_iframe"));
  child_iframe_element->contentDocument()->setDomain(String("origin-a.com"),
                                                     exception_state);
  main_iframe_element->contentDocument()->setDomain(String("origin-a.com"),
                                                    exception_state);

  // We may not have scheduled a visual update so force an update instead of
  // using BeginFrame.
  UpdateAllLifecyclePhases();
  iframe_doc = To<HTMLFrameOwnerElement>(GetElementById("main_iframe"))
                   ->contentDocument();
  EXPECT_FALSE(CcLayerByOwnerNodeId(iframe_doc));

  iframe_doc =
      To<HTMLFrameOwnerElement>(iframe_doc->getElementById("child_iframe"))
          ->contentDocument();
  owner_node = iframe_doc;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    owner_node = iframe_doc->documentElement();
  EXPECT_FALSE(CcLayerByOwnerNodeId(owner_node));
}

// Regression test for https://crbug.com/1095167. Render surfaces require that
// EffectNode::stable_id is set.
TEST_P(CompositingTest, EffectNodesShouldHaveStableIds) {
  InitializeWithHTML(*WebView()->MainFrameImpl()->GetFrame(), R"HTML(
    <div style="overflow: hidden; border-radius: 2px; height: 10px;">
      <div style="backdrop-filter: grayscale(3%);">
        a
        <span style="backdrop-filter: grayscale(3%);">b</span>
      </div>
    </div>
  )HTML");
  auto* property_trees = RootCcLayer()->layer_tree_host()->property_trees();
  for (const auto& effect_node : property_trees->effect_tree.nodes()) {
    if (effect_node.parent_id != -1)
      EXPECT_TRUE(!!effect_node.stable_id);
  }
}

TEST_P(CompositingSimTest, ImplSideScrollSkipsCommit) {
  // TODO(crbug.com/1046544): This test fails with CompositeAfterPaint because
  // PaintArtifactCompositor::Update is run for scroll offset changes. When we
  // have an early-out to avoid SetNeedsCommit for non-changing interest-rects,
  // this test will pass.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  InitializeWithHTML(R"HTML(
    <div id='scroller' style='will-change: transform; overflow: scroll;
        width: 100px; height: 100px'>
      <div style='height: 1000px'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  auto* scroller = GetDocument().getElementById("scroller");
  auto* scrollable_area = scroller->GetLayoutBox()->GetScrollableArea();
  auto element_id = scrollable_area->GetScrollElementId();

  EXPECT_FALSE(Compositor().LayerTreeHost()->CommitRequested());

  // Simulate the scroll update with scroll delta from impl-side.
  cc::CompositorCommitData commit_data;
  commit_data.scrolls.emplace_back(cc::CompositorCommitData::ScrollUpdateInfo(
      element_id, gfx::ScrollOffset(0, 10), base::nullopt));
  Compositor().LayerTreeHost()->ApplyCompositorChanges(&commit_data);
  EXPECT_EQ(FloatPoint(0, 10), scrollable_area->ScrollPosition());
  EXPECT_EQ(gfx::ScrollOffset(0, 10),
            GetPropertyTrees()->scroll_tree.current_scroll_offset(element_id));

  // Update just the blink lifecycle because a full frame would clear the bit
  // for whether a commit was requested.
  UpdateAllLifecyclePhases();

  // A main frame is needed to call UpdateLayers which updates property trees,
  // re-calculating cached to/from-screen transforms.
  EXPECT_TRUE(
      Compositor().LayerTreeHost()->RequestedMainFramePendingForTesting());

  // A full commit is not needed.
  EXPECT_FALSE(Compositor().LayerTreeHost()->CommitRequested());
}

TEST_P(CompositingSimTest, FrameAttribution) {
  InitializeWithHTML(R"HTML(
    <div id='child' style='will-change: transform;'>test</div>
    <iframe id='iframe' sandbox></iframe>
  )HTML");

  Compositor().BeginFrame();

  // Ensure that we correctly attribute child layers in the main frame to their
  // containing document.
  auto* child_layer = CcLayerByDOMElementId("child");
  ASSERT_TRUE(child_layer);

  auto* child_transform_node = GetTransformNode(child_layer);
  ASSERT_TRUE(child_transform_node);

  // Iterate the transform tree to gather the parent frame element ID.
  cc::ElementId visible_frame_element_id;
  auto* current_transform_node = child_transform_node;
  while (current_transform_node) {
    visible_frame_element_id = current_transform_node->visible_frame_element_id;
    if (visible_frame_element_id)
      break;
    current_transform_node =
        GetPropertyTrees()->transform_tree.parent(current_transform_node);
  }

  EXPECT_EQ(visible_frame_element_id,
            CompositorElementIdFromUniqueObjectId(
                DOMNodeIds::IdForNode(&GetDocument()),
                CompositorElementIdNamespace::kDOMNodeId));

  // Test that a layerized subframe's frame element ID is that of its
  // containing document.
  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("iframe"))->contentDocument();
  Node* owner_node = iframe_doc;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    owner_node = iframe_doc->documentElement();
  auto* iframe_layer = CcLayerByOwnerNodeId(owner_node);
  ASSERT_TRUE(iframe_layer);
  auto* iframe_transform_node = GetTransformNode(iframe_layer);
  EXPECT_TRUE(iframe_transform_node);

  EXPECT_EQ(iframe_transform_node->visible_frame_element_id,
            CompositorElementIdFromUniqueObjectId(
                DOMNodeIds::IdForNode(iframe_doc),
                CompositorElementIdNamespace::kDOMNodeId));
}

TEST_P(CompositingSimTest, VisibleFrameRootLayers) {
  SimRequest main_resource("https://origin-a.com/a.html", "text/html");
  SimRequest frame_resource("https://origin-b.com/b.html", "text/html");

  LoadURL("https://origin-a.com/a.html");
  main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <iframe id="iframe" src="https://origin-b.com/b.html"></iframe>
  )HTML");
  frame_resource.Complete("<!DOCTYPE html>");
  Compositor().BeginFrame();

  // Ensure that the toplevel is marked as a visible root.
  auto* toplevel_layer = CcLayerByOwnerNodeId(&GetDocument());
  ASSERT_TRUE(toplevel_layer);
  auto* toplevel_transform_node = GetTransformNode(toplevel_layer);
  ASSERT_TRUE(toplevel_transform_node);

  EXPECT_TRUE(toplevel_transform_node->visible_frame_element_id);

  // Ensure that the iframe is marked as a visible root.
  Document* iframe_doc =
      To<HTMLFrameOwnerElement>(GetElementById("iframe"))->contentDocument();
  Node* owner_node = iframe_doc;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    owner_node = iframe_doc->documentElement();
  auto* iframe_layer = CcLayerByOwnerNodeId(owner_node);
  ASSERT_TRUE(iframe_layer);
  auto* iframe_transform_node = GetTransformNode(iframe_layer);
  ASSERT_TRUE(iframe_transform_node);

  EXPECT_TRUE(iframe_transform_node->visible_frame_element_id);

  // Verify that after adding `pointer-events: none`, the subframe is no longer
  // considered a visible root.
  GetElementById("iframe")->SetInlineStyleProperty(
      CSSPropertyID::kPointerEvents, "none");

  UpdateAllLifecyclePhases();

  iframe_layer = CcLayerByOwnerNodeId(owner_node);
  ASSERT_TRUE(iframe_layer);
  iframe_transform_node = GetTransformNode(iframe_layer);
  ASSERT_TRUE(iframe_transform_node);

  EXPECT_FALSE(iframe_transform_node->visible_frame_element_id);
}

TEST_P(CompositingSimTest, DecompositedTransformWithChange) {
  InitializeWithHTML(R"HTML(
    <style>
      svg { overflow: hidden; }
      .initial { transform: rotate3d(0,0,1,10deg); }
      .changed { transform: rotate3d(0,0,1,0deg); }
    </style>
    <div style='will-change: transform;'>
      <svg id='svg' xmlns='http://www.w3.org/2000/svg' class='initial'>
        <line x1='50%' x2='50%' y1='0' y2='100%' stroke='blue'/>
        <line y1='50%' y2='50%' x1='0' x2='100%' stroke='blue'/>
      </svg>
    </div>
  )HTML");

  Compositor().BeginFrame();

  auto* svg_element_layer = CcLayerByDOMElementId("svg");
  EXPECT_FALSE(svg_element_layer->subtree_property_changed());

  auto* svg_element = GetElementById("svg");
  svg_element->setAttribute(html_names::kClassAttr, "changed");
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(svg_element_layer->subtree_property_changed());
}

// A simple repaint update should use a fast-path in PaintArtifactCompositor.
TEST_P(CompositingSimTest, BackgroundColorChangeUsesRepaintUpdate) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #target {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: white;
        }
      </style>
      <div id='target'></div>
  )HTML");

  Compositor().BeginFrame();

  EXPECT_EQ(CcLayerByDOMElementId("target")->background_color(), SK_ColorWHITE);

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Modifying paint in a simple way only requires a repaint update.
  auto* target_element = GetElementById("target");
  target_element->setAttribute(html_names::kStyleAttr, "background: black");
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kRepaint);

  // Though a repaint-only update was done, the background color should still
  // be updated.
  EXPECT_EQ(CcLayerByDOMElementId("target")->background_color(), SK_ColorBLACK);
}

// Similar to |BackgroundColorChangeUsesRepaintUpdate| but with multiple paint
// chunks being squashed into a single PendingLayer, and the background coming
// from the last paint chunk.
TEST_P(CompositingSimTest, MultipleChunkBackgroundColorChangeRepaintUpdate) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        div {
          position: absolute;
          width: 20px;
          height: 20px;
          top: 0px;
          left: 0px;
        }
        #a {
          background: lime;
        }
        #b {
          background: red;
          transform: translate(-100px, -100px);
        }
        #c {
          width: 800px;
          height: 600px;
          background: black;
        }
      </style>
      <div id="a"></div>
      <div id="b"></div>
      <!-- background color source -->
      <div id="c"></div>
  )HTML");

  Compositor().BeginFrame();

  auto* scrolling_contents = ScrollingContentsCcLayerByScrollElementId(
      RootCcLayer(),
      MainFrame().GetFrameView()->LayoutViewport()->GetScrollElementId());

  EXPECT_EQ(scrolling_contents->background_color(), SK_ColorBLACK);

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Modifying paint in a simple way only requires a repaint update.
  auto* background_element = GetElementById("c");
  background_element->setAttribute(html_names::kStyleAttr, "background: white");
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kRepaint);

  // Though a repaint-only update was done, the background color should still
  // be updated.
  EXPECT_EQ(scrolling_contents->background_color(), SK_ColorWHITE);
}

// Similar to |BackgroundColorChangeUsesRepaintUpdate| but with CompositeSVG.
// This test changes paint for a composited SVG element, as well as a regular
// HTML element in the presence of composited SVG.
TEST_P(CompositingSimTest, SVGColorChangeUsesRepaintUpdate) {
  ScopedCompositeSVGForTest enable_feature(true);
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        rect, div {
          width: 100px;
          height: 100px;
          will-change: transform;
        }
      </style>
      <svg>
        <rect fill="blue" />
        <rect id="rect" fill="blue" />
        <rect fill="blue" />
      </svg>
      <div id="div" style="background: blue;" />
      <svg>
        <rect fill="blue" />
      </svg>
  )HTML");

  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Modifying paint in a simple way only requires a repaint update.
  auto* rect_element = GetElementById("rect");
  rect_element->setAttribute(svg_names::kFillAttr, "black");
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kRepaint);

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Modifying paint in a simple way only requires a repaint update.
  auto* div_element = GetElementById("div");
  div_element->setAttribute(html_names::kStyleAttr, "background: black");
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kRepaint);
}

TEST_P(CompositingSimTest, ChangingOpaquenessRequiresFullUpdate) {
  // Contents opaque is set in different places in CAP (PAC) vs pre-CAP (CLM)
  // and we only want to test the PAC update here.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #target {
          width: 100px;
          height: 100px;
          will-change: transform;
          background: lightgreen;
        }
      </style>
      <div id="target"></div>
  )HTML");

  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());
  EXPECT_TRUE(CcLayerByDOMElementId("target")->contents_opaque());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // A change in opaqueness still requires a full update because opaqueness is
  // used during compositing to set the cc::Layer's contents opaque property
  // (see: PaintArtifactCompositor::CompositedLayerForPendingLayer).
  auto* target_element = GetElementById("target");
  target_element->setAttribute(html_names::kStyleAttr,
                               "background: rgba(1, 0, 0, 0.1)");
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);
  EXPECT_FALSE(CcLayerByDOMElementId("target")->contents_opaque());
}

TEST_P(CompositingSimTest, ChangingContentsOpaqueForTextRequiresFullUpdate) {
  // Contents opaque for text is set in different places in CAP (PAC) vs pre-CAP
  // (CLM) and we only want to test the PAC update here.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #target {
          width: 100px;
          height: 100px;
          will-change: transform;
        }
        #textContainer {
          width: 50px;
          height: 50px;
          padding: 5px;
          background: lightblue;
        }
      }
      </style>
      <div id="target">
        <div id="textContainer">
          mars
        </div>
      </div>
  )HTML");

  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());
  EXPECT_FALSE(CcLayerByDOMElementId("target")->contents_opaque());
  EXPECT_TRUE(CcLayerByDOMElementId("target")->contents_opaque_for_text());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // A change in opaqueness for text still requires a full update because
  // opaqueness is used during compositing to set the cc::Layer's contents
  // opaque for text property (see:
  // PaintArtifactCompositor::CompositedLayerForPendingLayer).
  auto* text_container_element = GetElementById("textContainer");
  text_container_element->setAttribute(html_names::kStyleAttr,
                                       "background: rgba(1, 0, 0, 0.1)");
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);
  EXPECT_FALSE(CcLayerByDOMElementId("target")->contents_opaque());
  EXPECT_FALSE(CcLayerByDOMElementId("target")->contents_opaque_for_text());
}

TEST_P(CompositingSimTest, FullCompositingUpdateReasons) {
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        div {
          width: 100px;
          height: 100px;
          will-change: transform;
          position: absolute;
        }
        #a {
          background: lightgreen;
          z-index: 10;
        }
        #b {
          background: lightblue;
          z-index: 20;
        }
      </style>
      <div id="a"></div>
      <div id="b"></div>
  )HTML");

  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Reordering paint chunks requires a full update. Overlap testing and the
  // order of synthetic effect layers are two examples of paint changes that
  // affect compositing decisions.
  auto* b_element = GetElementById("b");
  b_element->setAttribute(html_names::kStyleAttr, "z-index: 5");
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Removing a paint chunk requires a full update.
  b_element->setAttribute(html_names::kStyleAttr, "display: none");
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Adding a paint chunk requires a full update.
  b_element->setAttribute(html_names::kStyleAttr, "");
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Changing the size of a chunk affects overlap and requires a full update.
  b_element->setAttribute(html_names::kStyleAttr, "width: 101px");
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);
}

// Similar to |FullCompositingUpdateReasons| but for changes in CompositeSVG.
TEST_P(CompositingSimTest, FullCompositingUpdateReasonInCompositeSVG) {
  ScopedCompositeSVGForTest enable_feature(true);
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #rect {
          width: 100px;
          height: 100px;
          will-change: transform;
        }
      </style>
      <svg>
        <rect id="rect" fill="blue" />
      </svg>
  )HTML");

  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // Changing the size of a chunk affects overlap and requires a full update.
  auto* rect = GetElementById("rect");
  rect->setAttribute(html_names::kStyleAttr, "width: 101px");
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);
}

TEST_P(CompositingSimTest, FullCompositingUpdateForJustCreatedChunks) {
  ScopedCompositeSVGForTest enable_feature(true);
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        .firstLetterStyle:first-letter {
          background: red;
        }
        rect {
          width: 100px;
          height: 100px;
          fill: blue;
        }
      </style>
      <svg>
        <rect style="will-change: transform;"></rect>
        <rect id="target"></rect>
      </svg>
  )HTML");

  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // A new LayoutObject is "just created" and will not match existing chunks and
  // needs a full update. A first letter style adds a pseudo element which
  // results in rebuilding the #target LayoutObject.
  auto* target = GetElementById("target");
  target->setAttribute(html_names::kClassAttr, "firstLetterStyle");
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);
}

TEST_P(CompositingSimTest, FullCompositingUpdateForUncachableChunks) {
  ScopedCompositeSVGForTest enable_feature(true);
  InitializeWithHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        rect {
          width: 100px;
          height: 100px;
          fill: blue;
          will-change: transform;
        }
        div {
          width: 100px;
          height: 100px;
          background: lightblue;
        }
      </style>
      <svg>
        <rect id="rect"></rect>
      </svg>
      <div id="target"></div>
  )HTML");

  Compositor().BeginFrame();

  // Make the rect display item client uncachable. To avoid depending on when
  // this occurs in practice (see: |DisplayItemCacheSkipper|), this is done
  // directly.
  auto* rect = GetElementById("rect");
  auto* rect_client = static_cast<DisplayItemClient*>(rect->GetLayoutObject());
  rect_client->Invalidate(PaintInvalidationReason::kUncacheable);
  rect->setAttribute(html_names::kStyleAttr, "fill: green");
  Compositor().BeginFrame();

  // Initially, no update is needed.
  EXPECT_FALSE(paint_artifact_compositor()->NeedsUpdate());

  // Clear the previous update to ensure we record a new one in the next update.
  paint_artifact_compositor()->ClearPreviousUpdateForTesting();

  // A full update should be required due to the presence of uncacheable
  // paint chunks.
  auto* target = GetElementById("target");
  target->setAttribute(html_names::kStyleAttr, "background: lightgreen");
  Compositor().BeginFrame();
  EXPECT_EQ(paint_artifact_compositor()->PreviousUpdateForTesting(),
            PaintArtifactCompositor::PreviousUpdateType::kFull);
}

TEST_P(CompositingSimTest, DecompositeScrollerInHiddenIframe) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;
  SimRequest top_resource("https://example.com/top.html", "text/html");
  SimRequest middle_resource("https://example.com/middle.html", "text/html");
  SimRequest bottom_resource("https://example.com/bottom.html", "text/html");

  LoadURL("https://example.com/top.html");
  top_resource.Complete(R"HTML(
    <iframe id='middle' src='https://example.com/middle.html'></iframe>
  )HTML");
  middle_resource.Complete(R"HTML(
    <iframe id='bottom' src='bottom.html'></iframe>
  )HTML");
  bottom_resource.Complete(R"HTML(
    <div id='scroller' style='overflow:scroll;max-height:100px;background-color:#888'>
      <div style='height:1000px;'>Hello, world!</div>
    </div>
  )HTML");

  LocalFrame& middle_frame =
      *To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  LocalFrame& bottom_frame = *To<LocalFrame>(middle_frame.Tree().FirstChild());
  middle_frame.View()->BeginLifecycleUpdates();
  bottom_frame.View()->BeginLifecycleUpdates();
  Compositor().BeginFrame();
  LayoutBox* scroller = To<LayoutBox>(bottom_frame.GetDocument()
                                          ->getElementById("scroller")
                                          ->GetLayoutObject());
  ASSERT_TRUE(scroller->GetScrollableArea()->NeedsCompositedScrolling());

  // Hide the iframes. Scroller should be decomposited.
  GetDocument().getElementById("middle")->SetInlineStyleProperty(
      CSSPropertyID::kVisibility, CSSValueID::kHidden);
  Compositor().BeginFrame();
  EXPECT_FALSE(scroller->GetScrollableArea()->NeedsCompositedScrolling());
}

}  // namespace blink
