/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef PaintLayerCompositor_h
#define PaintLayerCompositor_h

#include <memory>
#include "core/CoreExport.h"
#include "core/dom/DocumentLifecycle.h"
#include "core/paint/compositing/CompositingReasonFinder.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class PaintLayer;
class GraphicsLayer;
class IntPoint;
class JSONObject;
class LayoutEmbeddedContent;
class Page;
class Scrollbar;
class ScrollingCoordinator;
class VisualViewport;

enum CompositingUpdateType {
  kCompositingUpdateNone,
  kCompositingUpdateAfterGeometryChange,
  kCompositingUpdateAfterCompositingInputChange,
  kCompositingUpdateRebuildTree,
};

enum CompositingStateTransitionType {
  kNoCompositingStateChange,
  kAllocateOwnCompositedLayerMapping,
  kRemoveOwnCompositedLayerMapping,
  kPutInSquashingLayer,
  kRemoveFromSquashingLayer
};

// PaintLayerCompositor maintains document-level compositing state and is the
// entry point of the "compositing update" lifecycle stage.  There is one PLC
// per LayoutView.
//
// The compositing update, implemented by PaintLayerCompositor and friends,
// decides for each PaintLayer whether it should get a CompositedLayerMapping,
// and asks each CLM to set up its GraphicsLayers.
//
// When root layer scrolling is disabled, PaintLayerCompositor also directly
// manages GraphicsLayers related to FrameView scrolling.  See VisualViewport.h
// for a diagram of how these layers are wired.
//
// When root layer scrolling is enabled, PaintLayerCompositor does not create
// any of its own GraphicsLayers.  Instead the LayoutView's CLM is wired
// directly to the scroll layer of the visual viewport.
//
// In Slimming Paint v2, PaintLayerCompositor will be eventually replaced by
// PaintArtifactCompositor.

class CORE_EXPORT PaintLayerCompositor final : public GraphicsLayerClient {
  USING_FAST_MALLOC(PaintLayerCompositor);

 public:
  explicit PaintLayerCompositor(LayoutView&);
  ~PaintLayerCompositor() override;

  void UpdateIfNeededRecursive(DocumentLifecycle::LifecycleState target_state);

  // Return true if this LayoutView is in "compositing mode" (i.e. has one or
  // more composited Layers)
  bool InCompositingMode() const;
  // FIXME: Replace all callers with inCompositingMode and remove this function.
  bool StaleInCompositingMode() const;
  // This will make a compositing layer at the root automatically, and hook up
  // to the native view/window system.
  void SetCompositingModeEnabled(bool);

  // Returns true if the accelerated compositing is enabled
  bool HasAcceleratedCompositing() const {
    return has_accelerated_compositing_;
  }

  bool PreferCompositingToLCDTextEnabled() const;

  bool RootShouldAlwaysComposite() const;

  // Copy the accelerated compositing related flags from Settings
  void UpdateAcceleratedCompositingSettings();

  // Used to indicate that a compositing update will be needed for the next
  // frame that gets drawn.
  void SetNeedsCompositingUpdate(CompositingUpdateType);

  void DidLayout();

  // Whether layer's compositedLayerMapping needs a GraphicsLayer to clip
  // z-order children of the given Layer.
  bool ClipsCompositingDescendants(const PaintLayer*) const;

  // Whether the given layer needs an extra 'contents' layer.
  bool NeedsContentsCompositingLayer(const PaintLayer*) const;

  bool NeedsFixedRootBackgroundLayer() const;
  GraphicsLayer* FixedRootBackgroundLayer() const;
  void SetNeedsUpdateFixedBackground() {
    needs_update_fixed_background_ = true;
  }

  // Issue paint invalidations of the appropriate layers when the given Layer
  // starts or stops being composited.
  void PaintInvalidationOnCompositingChange(PaintLayer*);

  void FullyInvalidatePaint();

  PaintLayer* RootLayer() const;

  GraphicsLayer* ContainerLayer() const { return container_layer_.get(); }
  GraphicsLayer* FrameScrollLayer() const { return scroll_layer_.get(); }
  GraphicsLayer* RootContentLayer() const { return root_content_layer_.get(); }
  GraphicsLayer* LayerForHorizontalScrollbar() const {
    return layer_for_horizontal_scrollbar_.get();
  }
  GraphicsLayer* LayerForVerticalScrollbar() const {
    return layer_for_vertical_scrollbar_.get();
  }
  GraphicsLayer* LayerForScrollCorner() const {
    return layer_for_scroll_corner_.get();
  }

  // In root layer scrolling mode, returns the LayoutView's main GraphicsLayer.
  // In non-RLS mode, returns the outermost PaintLayerCompositor layer.
  GraphicsLayer* RootGraphicsLayer() const;

  // In root layer scrolling mode, this is the LayoutView's scroll layer.
  // In non-RLS mode, this is the same as frameScrollLayer().
  GraphicsLayer* ScrollLayer() const;

  void UpdateRootLayerPosition();

  void SetIsInWindow(bool);

  static PaintLayerCompositor* FrameContentsCompositor(LayoutEmbeddedContent&);
  // Return true if the layers changed.
  static bool AttachFrameContentLayersToIframeLayer(LayoutEmbeddedContent&);

  // Update the geometry of the layers used for clipping and scrolling in
  // frames.
  void FrameViewDidChangeLocation(const IntPoint& contents_offset);
  void FrameViewDidChangeSize();
  void FrameViewDidScroll();
  void FrameViewScrollbarsExistenceDidChange();
  void RootFixedBackgroundsChanged();

  bool ScrollingLayerDidChange(PaintLayer*);

  std::unique_ptr<JSONObject> LayerTreeAsJSON(LayerTreeFlags) const;

  void SetTracksRasterInvalidations(bool);

  String DebugName(const GraphicsLayer*) const override;
  DocumentLifecycle& Lifecycle() const;

  void UpdatePotentialCompositingReasonsFromStyle(PaintLayer*);

  // Whether the layer could ever be composited.
  bool CanBeComposited(const PaintLayer*) const;

  // FIXME: Move allocateOrClearCompositedLayerMapping to
  // CompositingLayerAssigner once we've fixed the compositing chicken/egg
  // issues.
  bool AllocateOrClearCompositedLayerMapping(
      PaintLayer*,
      CompositingStateTransitionType composited_layer_update);

  bool InOverlayFullscreenVideo() const { return in_overlay_fullscreen_video_; }

 private:
#if DCHECK_IS_ON()
  void AssertNoUnresolvedDirtyBits();
#endif

  void UpdateIfNeededRecursiveInternal(
      DocumentLifecycle::LifecycleState target_state);

  // GraphicsLayerClient implementation
  bool NeedsRepaint(const GraphicsLayer&) const { return true; }
  IntRect ComputeInterestRect(const GraphicsLayer*,
                              const IntRect&) const override;
  void PaintContents(const GraphicsLayer*,
                     GraphicsContext&,
                     GraphicsLayerPaintingPhase,
                     const IntRect& interest_rect) const override;

  bool IsTrackingRasterInvalidations() const override;

  void UpdateWithoutAcceleratedCompositing(CompositingUpdateType);
  void UpdateIfNeeded(DocumentLifecycle::LifecycleState target_state);

  void EnsureRootLayer();
  void DestroyRootLayer();

  void AttachRootLayer();
  void DetachRootLayer();

  void AttachCompositorTimeline();
  void DetachCompositorTimeline();

  void UpdateOverflowControlsLayers();

  Page* GetPage() const;

  ScrollingCoordinator* GetScrollingCoordinator() const;

  void EnableCompositingModeIfNeeded();

  bool RequiresHorizontalScrollbarLayer() const;
  bool RequiresVerticalScrollbarLayer() const;
  bool RequiresScrollCornerLayer() const;
  void ShowScrollbarLayersIfNeeded();

  void ApplyOverlayFullscreenVideoAdjustmentIfNeeded();

  void UpdateContainerSizes();

  // Checks the given graphics layer against the compositor's horizontal and
  // vertical scrollbar graphics layers, returning the associated Scrollbar
  // instance if any, else nullptr.
  Scrollbar* GraphicsLayerToScrollbar(const GraphicsLayer*) const;

  bool IsMainFrame() const;
  VisualViewport& GetVisualViewport() const;
  GraphicsLayer* ParentForContentLayers() const;

  LayoutView& layout_view_;

  CompositingReasonFinder compositing_reason_finder_;

  CompositingUpdateType pending_update_type_;

  bool has_accelerated_compositing_;
  bool compositing_;

  // The root layer doesn't composite if it's a non-scrollable frame.
  // So, after a layout we set this dirty bit to know that we need
  // to recompute whether the root layer should composite even if
  // none of its descendants composite.
  // FIXME: Get rid of all the callers of setCompositingModeEnabled
  // except the one in updateIfNeeded, then rename this to
  // m_compositingDirty.
  bool root_should_always_composite_dirty_;
  bool needs_update_fixed_background_;
  bool is_tracking_raster_invalidations_;  // Used for testing.
  bool in_overlay_fullscreen_video_;

  enum RootLayerAttachment {
    kRootLayerUnattached,
    kRootLayerPendingAttachViaChromeClient,
    kRootLayerAttachedViaChromeClient,
    kRootLayerAttachedViaEnclosingFrame
  };
  RootLayerAttachment root_layer_attachment_;

  // Outermost layer, holds overflow controls and the container layer
  std::unique_ptr<GraphicsLayer> overflow_controls_host_layer_;

  // Clips for iframe content
  std::unique_ptr<GraphicsLayer> container_layer_;

  // Scrolls with the FrameView
  std::unique_ptr<GraphicsLayer> scroll_layer_;

  // Innermost layer, parent of LayoutView main GraphicsLayer
  std::unique_ptr<GraphicsLayer> root_content_layer_;

  // Layers for overflow controls
  std::unique_ptr<GraphicsLayer> layer_for_horizontal_scrollbar_;
  std::unique_ptr<GraphicsLayer> layer_for_vertical_scrollbar_;
  std::unique_ptr<GraphicsLayer> layer_for_scroll_corner_;
};

}  // namespace blink

#endif  // PaintLayerCompositor_h
