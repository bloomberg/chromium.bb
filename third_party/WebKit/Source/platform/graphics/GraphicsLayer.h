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

#ifndef GraphicsLayer_h
#define GraphicsLayer_h

#include <memory>
#include "cc/layers/layer_client.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/graphics/GraphicsLayerDebugInfo.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/heap/Handle.h"
#include "platform/transforms/TransformationMatrix.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebContentLayer.h"
#include "public/platform/WebContentLayerClient.h"
#include "public/platform/WebImageLayer.h"
#include "public/platform/WebLayerStickyPositionConstraint.h"
#include "public/platform/WebScrollBoundaryBehavior.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class CompositorFilterOperations;
class Image;
class JSONObject;
class LinkHighlight;
class PaintController;
struct RasterInvalidationTracking;
class ScrollableArea;
class WebLayer;

typedef Vector<GraphicsLayer*, 64> GraphicsLayerVector;

// GraphicsLayer is an abstraction for a rendering surface with backing store,
// which may have associated transformation and animations.
class PLATFORM_EXPORT GraphicsLayer : public cc::LayerClient,
                                      public DisplayItemClient,
                                      private WebContentLayerClient {
  WTF_MAKE_NONCOPYABLE(GraphicsLayer);
  USING_FAST_MALLOC(GraphicsLayer);

 public:
  static std::unique_ptr<GraphicsLayer> Create(GraphicsLayerClient*);

  ~GraphicsLayer() override;

  GraphicsLayerClient* Client() const { return client_; }

  GraphicsLayerDebugInfo& DebugInfo();

  void SetCompositingReasons(CompositingReasons);
  CompositingReasons GetCompositingReasons() const {
    return debug_info_.GetCompositingReasons();
  }
  void SetSquashingDisallowedReasons(SquashingDisallowedReasons);
  void SetOwnerNodeId(int);

  GraphicsLayer* Parent() const { return parent_; }
  void SetParent(GraphicsLayer*);  // Internal use only.

  const Vector<GraphicsLayer*>& Children() const { return children_; }
  // Returns true if the child list changed.
  bool SetChildren(const GraphicsLayerVector&);

  // Add child layers. If the child is already parented, it will be removed from
  // its old parent.
  void AddChild(GraphicsLayer*);
  void AddChildBelow(GraphicsLayer*, GraphicsLayer* sibling);

  void RemoveAllChildren();
  void RemoveFromParent();

  GraphicsLayer* MaskLayer() const { return mask_layer_; }
  void SetMaskLayer(GraphicsLayer*);

  GraphicsLayer* ContentsClippingMaskLayer() const {
    return contents_clipping_mask_layer_;
  }
  void SetContentsClippingMaskLayer(GraphicsLayer*);

  enum ShouldSetNeedsDisplay { kDontSetNeedsDisplay, kSetNeedsDisplay };

  // The offset is the origin of the layoutObject minus the origin of the
  // graphics layer (so either zero or negative).
  IntSize OffsetFromLayoutObject() const {
    return FlooredIntSize(offset_from_layout_object_);
  }
  void SetOffsetFromLayoutObject(const IntSize&,
                                 ShouldSetNeedsDisplay = kSetNeedsDisplay);
  LayoutSize OffsetFromLayoutObjectWithSubpixelAccumulation() const;

  // The double version is only used in updateScrollingLayerGeometry() for
  // detecting a scroll offset change at floating point precision.
  DoubleSize OffsetDoubleFromLayoutObject() const {
    return offset_from_layout_object_;
  }
  void SetOffsetDoubleFromLayoutObject(
      const DoubleSize&,
      ShouldSetNeedsDisplay = kSetNeedsDisplay);

  // The position of the layer (the location of its top-left corner in its
  // parent).
  const FloatPoint& GetPosition() const { return position_; }
  void SetPosition(const FloatPoint&);

  const FloatPoint3D& TransformOrigin() const { return transform_origin_; }
  void SetTransformOrigin(const FloatPoint3D&);

  // The size of the layer.
  const FloatSize& Size() const { return size_; }
  void SetSize(const FloatSize&);

  const TransformationMatrix& Transform() const { return transform_; }
  void SetTransform(const TransformationMatrix&);
  void SetShouldFlattenTransform(bool);
  void SetRenderingContext(int id);

  bool MasksToBounds() const;
  void SetMasksToBounds(bool);

  bool DrawsContent() const { return draws_content_; }
  void SetDrawsContent(bool);

  bool ContentsAreVisible() const { return contents_visible_; }
  void SetContentsVisible(bool);

  void SetScrollParent(WebLayer*);
  void SetClipParent(WebLayer*);

  // For special cases, e.g. drawing missing tiles on Android.
  // The compositor should never paint this color in normal cases because the
  // Layer will paint the background by itself.
  Color BackgroundColor() const { return background_color_; }
  void SetBackgroundColor(const Color&);

  // opaque means that we know the layer contents have no alpha
  bool ContentsOpaque() const { return contents_opaque_; }
  void SetContentsOpaque(bool);

  bool BackfaceVisibility() const { return backface_visibility_; }
  void SetBackfaceVisibility(bool visible);

  float Opacity() const { return opacity_; }
  void SetOpacity(float);

  void SetBlendMode(WebBlendMode);
  void SetIsRootForIsolatedGroup(bool);

  void SetShouldHitTest(bool);

  void SetFilters(CompositorFilterOperations);
  void SetBackdropFilters(CompositorFilterOperations);

  void SetStickyPositionConstraint(const WebLayerStickyPositionConstraint&);

  void SetFilterQuality(SkFilterQuality);

  // Some GraphicsLayers paint only the foreground or the background content
  GraphicsLayerPaintingPhase PaintingPhase() const { return painting_phase_; }
  void SetPaintingPhase(GraphicsLayerPaintingPhase);

  void SetNeedsDisplay();
  // Mark the given rect (in layer coords) as needing display. Never goes deep.
  void SetNeedsDisplayInRect(const IntRect&,
                             PaintInvalidationReason,
                             const DisplayItemClient&);

  void SetContentsNeedsDisplay();

  // Set that the position/size of the contents (image or video).
  void SetContentsRect(const IntRect&);

  // Layer contents
  void SetContentsToImage(
      Image*,
      RespectImageOrientationEnum = kDoNotRespectImageOrientation);
  void SetContentsToPlatformLayer(WebLayer* layer) { SetContentsTo(layer); }
  bool HasContentsLayer() const { return contents_layer_; }

  // For hosting this GraphicsLayer in a native layer hierarchy.
  WebLayer* PlatformLayer() const;

  int PaintCount() const { return paint_count_; }

  // Return a string with a human readable form of the layer tree. If debug is
  // true, pointers for the layers and timing data will be included in the
  // returned string.
  String GetLayerTreeAsTextForTesting(LayerTreeFlags = kLayerTreeNormal) const;

  std::unique_ptr<JSONObject> LayerTreeAsJSON(LayerTreeFlags) const;

  void SetTracksRasterInvalidations(bool);
  bool IsTrackingOrCheckingRasterInvalidations() const {
    return RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
           is_tracking_raster_invalidations_;
  }

  void ResetTrackedRasterInvalidations();
  bool HasTrackedRasterInvalidations() const;
  const RasterInvalidationTracking* GetRasterInvalidationTracking() const;
  void TrackRasterInvalidation(const DisplayItemClient&,
                               const IntRect&,
                               PaintInvalidationReason);

  void AddLinkHighlight(LinkHighlight*);
  void RemoveLinkHighlight(LinkHighlight*);
  // Exposed for tests
  unsigned NumLinkHighlights() { return link_highlights_.size(); }
  LinkHighlight* GetLinkHighlight(int i) { return link_highlights_[i]; }

  void SetScrollableArea(ScrollableArea*);
  ScrollableArea* GetScrollableArea() const { return scrollable_area_; }

  WebContentLayer* ContentLayer() const { return layer_.get(); }

  static void RegisterContentsLayer(WebLayer*);
  static void UnregisterContentsLayer(WebLayer*);

  IntRect InterestRect();
  void Paint(const IntRect* interest_rect,
             GraphicsContext::DisabledMode = GraphicsContext::kNothingDisabled);

  // cc::LayerClient implementation.
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> TakeDebugInfo(
      cc::Layer*) override;
  void didUpdateMainThreadScrollingReasons() override;
  void didChangeScrollbarsHidden(bool);

  PaintController& GetPaintController() const;

  // Exposed for tests.
  WebLayer* ContentsLayer() const { return contents_layer_; }

  void SetElementId(const CompositorElementId&);
  CompositorElementId GetElementId() const;

  void SetCompositorMutableProperties(uint32_t);

  WebContentLayerClient& WebContentLayerClientForTesting() { return *this; }

  // DisplayItemClient methods
  String DebugName() const final { return client_->DebugName(this); }
  LayoutRect VisualRect() const override;

  void SetHasWillChangeTransformHint(bool);

  void SetScrollBoundaryBehavior(const WebScrollBoundaryBehavior&);
  void SetIsResizedByBrowserControls(bool);

 protected:
  String DebugName(cc::Layer*) const;
  bool ShouldFlattenTransform() const { return should_flatten_transform_; }

  explicit GraphicsLayer(GraphicsLayerClient*);
  // for testing
  friend class CompositedLayerMappingTest;
  friend class PaintControllerPaintTestBase;

 private:
  // WebContentLayerClient implementation.
  gfx::Rect PaintableRegion() final { return InterestRect(); }
  void PaintContents(WebDisplayItemList*,
                     PaintingControlSetting = kPaintDefaultBehavior) final;
  size_t ApproximateUnsharedMemoryUsage() const final;

  // Returns true if PaintController::paintArtifact() changed and needs commit.
  bool PaintWithoutCommit(
      const IntRect* interest_rect,
      GraphicsContext::DisabledMode = GraphicsContext::kNothingDisabled);

  // Adds a child without calling updateChildList(), so that adding children
  // can be batched before updating.
  void AddChildInternal(GraphicsLayer*);

#if DCHECK_IS_ON()
  bool HasAncestor(GraphicsLayer*) const;
#endif

  void IncrementPaintCount() { ++paint_count_; }

  // Helper functions used by settors to keep layer's the state consistent.
  void UpdateChildList();
  void UpdateLayerIsDrawable();
  void UpdateContentsRect();

  void SetContentsTo(WebLayer*);
  void SetupContentsLayer(WebLayer*);
  void ClearContentsLayerIfUnregistered();
  WebLayer* ContentsLayerIfRegistered();

  typedef HashMap<int, int> RenderingContextMap;
  std::unique_ptr<JSONObject> LayerTreeAsJSONInternal(
      LayerTreeFlags,
      RenderingContextMap&) const;
  std::unique_ptr<JSONObject> LayerAsJSONInternal(
      LayerTreeFlags,
      RenderingContextMap&,
      const FloatPoint& = FloatPoint()) const;
  void AddTransformJSONProperties(JSONObject&, RenderingContextMap&) const;
  class LayersAsJSONArray;

  sk_sp<PaintRecord> CaptureRecord();

  GraphicsLayerClient* client_;

  // Offset from the owning layoutObject
  DoubleSize offset_from_layout_object_;

  // Position is relative to the parent GraphicsLayer
  FloatPoint position_;
  FloatSize size_;

  TransformationMatrix transform_;
  FloatPoint3D transform_origin_;

  Color background_color_;
  float opacity_;

  WebBlendMode blend_mode_;

  bool has_transform_origin_ : 1;
  bool contents_opaque_ : 1;
  bool should_flatten_transform_ : 1;
  bool backface_visibility_ : 1;
  bool draws_content_ : 1;
  bool contents_visible_ : 1;
  bool is_root_for_isolated_group_ : 1;
  bool should_hit_test_ : 1;

  bool has_scroll_parent_ : 1;
  bool has_clip_parent_ : 1;

  bool painted_ : 1;

  bool is_tracking_raster_invalidations_ : 1;

  GraphicsLayerPaintingPhase painting_phase_;

  Vector<GraphicsLayer*> children_;
  GraphicsLayer* parent_;

  GraphicsLayer* mask_layer_;  // Reference to mask layer. We don't own this.
  GraphicsLayer* contents_clipping_mask_layer_;  // Reference to clipping mask
                                                 // layer. We don't own this.

  IntRect contents_rect_;

  int paint_count_;

  std::unique_ptr<WebContentLayer> layer_;
  std::unique_ptr<WebImageLayer> image_layer_;
  WebLayer* contents_layer_;
  // We don't have ownership of m_contentsLayer, but we do want to know if a
  // given layer is the same as our current layer in setContentsTo(). Since
  // |m_contentsLayer| may be deleted at this point, we stash an ID away when we
  // know |m_contentsLayer| is alive and use that for comparisons from that
  // point on.
  int contents_layer_id_;

  Vector<LinkHighlight*> link_highlights_;

  WeakPersistent<ScrollableArea> scrollable_area_;
  GraphicsLayerDebugInfo debug_info_;
  int rendering_context3d_;

  mutable std::unique_ptr<PaintController> paint_controller_;

  IntRect previous_interest_rect_;
};

// ObjectPaintInvalidatorWithContext::InvalidatePaintRectangleWithContext uses
// this to reduce differences between layout test results of SPv1 and SPv2.
class PLATFORM_EXPORT ScopedSetNeedsDisplayInRectForTrackingOnly {
 public:
  ScopedSetNeedsDisplayInRectForTrackingOnly() {
    DCHECK(!s_enabled_);
    s_enabled_ = true;
  }
  ~ScopedSetNeedsDisplayInRectForTrackingOnly() {
    DCHECK(s_enabled_);
    s_enabled_ = false;
  }

 private:
  friend class GraphicsLayer;
  static bool s_enabled_;
};

}  // namespace blink

#ifndef NDEBUG
// Outside the blink namespace for ease of invocation from gdb.
void PLATFORM_EXPORT showGraphicsLayerTree(const blink::GraphicsLayer*);
void PLATFORM_EXPORT showGraphicsLayers(const blink::GraphicsLayer*);
#endif

#endif  // GraphicsLayer_h
