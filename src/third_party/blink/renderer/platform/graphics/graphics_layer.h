/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_LAYER_H_

#include <memory>

#include "base/dcheck_is_on.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/compositing/layers_as_json.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer_client.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidator.h"
#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/graphics/squashing_disallowed_reasons.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
class DisplayItemList;
class PictureLayer;
}  // namespace cc

namespace blink {

class PaintController;
class RasterInvalidationTracking;
class RasterInvalidator;
struct PreCompositedLayerInfo;

typedef Vector<GraphicsLayer*, 64> GraphicsLayerVector;

// GraphicsLayer is an abstraction for a rendering surface with backing store,
// which may have associated transformation and animations.
class PLATFORM_EXPORT GraphicsLayer : public DisplayItemClient,
                                      public LayerAsJSONClient,
                                      private cc::ContentLayerClient {
  USING_FAST_MALLOC(GraphicsLayer);

 public:
  explicit GraphicsLayer(GraphicsLayerClient&);
  GraphicsLayer(const GraphicsLayer&) = delete;
  GraphicsLayer& operator=(const GraphicsLayer&) = delete;
  ~GraphicsLayer() override;

  GraphicsLayerClient& Client() const { return client_; }

  void SetCompositingReasons(CompositingReasons reasons) {
    compositing_reasons_ = reasons;
  }
  CompositingReasons GetCompositingReasons() const {
    return compositing_reasons_;
  }

  SquashingDisallowedReasons GetSquashingDisallowedReasons() const {
    return squashing_disallowed_reasons_;
  }
  void SetSquashingDisallowedReasons(SquashingDisallowedReasons reasons) {
    squashing_disallowed_reasons_ = reasons;
  }

  void SetOwnerNodeId(DOMNodeId id) { owner_node_id_ = id; }

  GraphicsLayer* Parent() const { return parent_; }
  void SetParent(GraphicsLayer*);  // Internal use only.

  const Vector<GraphicsLayer*>& Children() const { return children_; }
  // Returns true if the child list changed.
  bool SetChildren(const GraphicsLayerVector&);

  // Add child layers. If the child is already parented, it will be removed from
  // its old parent.
  void AddChild(GraphicsLayer*);

  void RemoveAllChildren();
  void RemoveFromParent();

  // The offset is the origin of the layoutObject minus the origin of the
  // graphics layer (so either zero or negative).
  IntSize OffsetFromLayoutObject() const { return offset_from_layout_object_; }
  void SetOffsetFromLayoutObject(const IntSize&);

  // The size of the layer.
  const gfx::Size& Size() const;
  void SetSize(const gfx::Size&);

  bool DrawsContent() const { return draws_content_; }
  void SetDrawsContent(bool);

  // False if no hit test data will be recorded onto this GraphicsLayer.
  // This is different from |DrawsContent| because hit test data are internal
  // to blink and are not copied to the cc::Layer's display list.
  bool PaintsHitTest() const { return paints_hit_test_; }
  void SetPaintsHitTest(bool);

  bool PaintsContentOrHitTest() const {
    return draws_content_ || paints_hit_test_;
  }

  bool ContentsAreVisible() const { return contents_visible_; }
  void SetContentsVisible(bool);

  void SetHitTestable(bool);
  bool IsHitTestable() const { return hit_testable_; }

  // Some GraphicsLayers paint only the foreground or the background content
  GraphicsLayerPaintingPhase PaintingPhase() const { return painting_phase_; }
  void SetPaintingPhase(GraphicsLayerPaintingPhase);

  void InvalidateContents();

  // Set that the position/size of the contents (image or video).
  void SetContentsRect(const IntRect&);

  void SetContentsToCcLayer(scoped_refptr<cc::Layer> contents_layer);
  bool HasContentsLayer() const { return ContentsLayer(); }
  cc::Layer* ContentsLayer() const { return contents_layer_.get(); }

  const IntRect& ContentsRect() const { return contents_rect_; }

  // For hosting this GraphicsLayer in a native layer hierarchy.
  cc::PictureLayer& CcLayer() const { return *layer_; }

  bool IsTrackingRasterInvalidations() const;
  void UpdateTrackingRasterInvalidations();
  void ResetTrackedRasterInvalidations();
  bool HasTrackedRasterInvalidations() const;
  RasterInvalidationTracking* GetRasterInvalidationTracking() const;
  void TrackRasterInvalidation(const DisplayItemClient&,
                               const IntRect&,
                               PaintInvalidationReason);

  // Returns true if any layer is repainted.
  bool PaintRecursively(GraphicsContext&,
                        Vector<PreCompositedLayerInfo>&,
                        PaintController::CycleScope& cycle_scope,
                        PaintBenchmarkMode = PaintBenchmarkMode::kNormal);

  PaintController& GetPaintController() const;

  void SetElementId(const CompositorElementId&);

  // DisplayItemClient methods
  String DebugName() const final { return client_.DebugName(this); }
  DOMNodeId OwnerNodeId() const final { return owner_node_id_; }

  // LayerAsJSONClient implementation.
  void AppendAdditionalInfoAsJSON(LayerTreeFlags,
                                  const cc::Layer&,
                                  JSONObject&) const override;

  bool HasLayerState() const { return layer_state_.get(); }
  void SetLayerState(const PropertyTreeStateOrAlias&,
                     const IntPoint& layer_offset);
  PropertyTreeStateOrAlias GetPropertyTreeState() const {
    return layer_state_->state.GetPropertyTreeState();
  }
  IntPoint GetOffsetFromTransformNode() const { return layer_state_->offset; }

  void SetContentsLayerState(const PropertyTreeStateOrAlias&,
                             const IntPoint& layer_offset);
  PropertyTreeStateOrAlias GetContentsPropertyTreeState() const {
    return contents_layer_state_
               ? contents_layer_state_->state.GetPropertyTreeState()
               : GetPropertyTreeState();
  }
  IntPoint GetContentsOffsetFromTransformNode() const {
    return contents_layer_state_ ? contents_layer_state_->offset
                                 : GetOffsetFromTransformNode();
  }

  void SetNeedsCheckRasterInvalidation() {
    needs_check_raster_invalidation_ = true;
  }

  void PaintForTesting(const IntRect& interest_rect, bool record_debug_info);

  void SetShouldCreateLayersAfterPaint(bool);
  bool ShouldCreateLayersAfterPaint() const {
    return should_create_layers_after_paint_;
  }

  // Whether this GraphicsLayer is repainted in the last Paint().
  bool Repainted() const { return repainted_; }

  size_t ApproximateUnsharedMemoryUsageRecursive() const;

 protected:
  String DebugName(const cc::Layer*) const;

 private:
  friend class CompositedLayerMappingTest;
  friend class GraphicsLayerTest;

  // cc::ContentLayerClient implementation.
  gfx::Rect PaintableRegion() const final;
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList() final;
  bool FillsBoundsCompletely() const final { return false; }

  void ClearPaintStateRecursively();
  void Paint(Vector<PreCompositedLayerInfo>&,
             PaintBenchmarkMode,
             PaintController::CycleScope*,
             const IntRect* interest_rect = nullptr);

  // Adds a child without calling NotifyChildListChange(), so that adding
  // children can be batched before updating.
  void AddChildInternal(GraphicsLayer*);

#if DCHECK_IS_ON()
  bool HasAncestor(GraphicsLayer*) const;
#endif

  // Helper functions used by settors to keep layer's the state consistent.
  void NotifyChildListChange();
  void UpdateLayerIsDrawable();
  void UpdateContentsLayerBounds();

  void SetContentsTo(scoped_refptr<cc::Layer>);

  RasterInvalidator& EnsureRasterInvalidator();
  void InvalidateRaster(const gfx::Rect&);

  GraphicsLayerClient& client_;

  // Offset from the owning layoutObject
  IntSize offset_from_layout_object_;

  TransformationMatrix transform_;

  bool draws_content_ : 1;
  bool paints_hit_test_ : 1;
  bool contents_visible_ : 1;
  bool hit_testable_ : 1;
  bool needs_check_raster_invalidation_ : 1;
  bool raster_invalidated_ : 1;
  // True if the cc::Layers for this GraphicsLayer should be created after
  // paint (in PaintArtifactCompositor). This depends on the display item list
  // and is updated after CommitNewDisplayItems.
  bool should_create_layers_after_paint_ : 1;
  bool repainted_ : 1;

  GraphicsLayerPaintingPhase painting_phase_;

  Vector<GraphicsLayer*> children_;
  GraphicsLayer* parent_;

  IntRect contents_rect_;

  scoped_refptr<cc::PictureLayer> layer_;
  scoped_refptr<cc::Layer> contents_layer_;
  scoped_refptr<cc::DisplayItemList> cc_display_item_list_;

  SquashingDisallowedReasons squashing_disallowed_reasons_ =
      SquashingDisallowedReason::kNone;

  mutable std::unique_ptr<PaintController> paint_controller_;

  // Used only when CullRectUpdate is not enabled.
  IntRect previous_interest_rect_;

  struct LayerState {
    // In theory, it's unnecessary to use RefCountedPropertyTreeState because
    // when it's used, the state should always reference current paint property
    // nodes in ObjectPaintProperties. This is to workaround under-invalidation
    // of layer state.
    RefCountedPropertyTreeState state;
    IntPoint offset;
  };
  std::unique_ptr<LayerState> layer_state_;
  std::unique_ptr<LayerState> contents_layer_state_;

  std::unique_ptr<RasterInvalidator> raster_invalidator_;
  RasterInvalidator::RasterInvalidationFunction raster_invalidation_function_;

  DOMNodeId owner_node_id_ = kInvalidDOMNodeId;
  CompositingReasons compositing_reasons_ = CompositingReason::kNone;
};

// Iterates all graphics layers that should be seen by the compositor in
// pre-order. |GraphicsLayerType&| matches |GraphicsLayer&| or
// |const GraphicsLayer&|. |GraphicsLayerFunction| accepts a GraphicsLayerType
// parameter, and returns a bool to indicate if the recursion should continue.
template <typename GraphicsLayerType,
          typename GraphicsLayerFunction,
          typename ContentsLayerFunction>
void ForAllGraphicsLayers(
    GraphicsLayerType& layer,
    const GraphicsLayerFunction& graphics_layer_function,
    const ContentsLayerFunction& contents_layer_function) {
  if (!graphics_layer_function(layer))
    return;

  if (auto* contents_layer = layer.ContentsLayer())
    contents_layer_function(layer, *contents_layer);

  for (auto* child : layer.Children()) {
    ForAllGraphicsLayers(*child, graphics_layer_function,
                         contents_layer_function);
  }
}

// Unlike ForAllGraphicsLayers, here |GraphicsLayerFunction| should return void.
template <typename GraphicsLayerType,
          typename GraphicsLayerFunction,
          typename ContentsLayerFunction>
void ForAllActiveGraphicsLayers(
    GraphicsLayerType& layer,
    const GraphicsLayerFunction& graphics_layer_function,
    const ContentsLayerFunction& contents_layer_function) {
  ForAllGraphicsLayers(
      layer,
      [&graphics_layer_function](GraphicsLayerType& layer) -> bool {
        if (layer.Client().ShouldSkipPaintingSubtree())
          return false;
        if (layer.PaintsContentOrHitTest() || layer.IsHitTestable())
          graphics_layer_function(layer);
        return true;
      },
      contents_layer_function);
}

template <typename GraphicsLayerType, typename Function>
void ForAllActiveGraphicsLayers(GraphicsLayerType& layer,
                                const Function& function) {
  ForAllActiveGraphicsLayers(layer, function,
                             [](GraphicsLayerType&, const cc::Layer&) {});
}

template <typename GraphicsLayerType, typename Function>
void ForAllPaintingGraphicsLayers(GraphicsLayerType& layer,
                                  const Function& function) {
  ForAllActiveGraphicsLayers(layer, [&function](GraphicsLayerType& layer) {
    if (layer.PaintsContentOrHitTest())
      function(layer);
  });
}

}  // namespace blink

#if DCHECK_IS_ON()
// Outside the blink namespace for ease of invocation from gdb.
PLATFORM_EXPORT void showGraphicsLayerTree(const blink::GraphicsLayer*);
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_LAYER_H_
