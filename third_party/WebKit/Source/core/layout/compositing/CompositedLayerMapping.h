/*
 * Copyright (C) 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef CompositedLayerMapping_h
#define CompositedLayerMapping_h

#include <memory>
#include "core/layout/compositing/GraphicsLayerUpdater.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerPaintingInfo.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class PaintLayerCompositor;

// A GraphicsLayerPaintInfo contains all the info needed to paint a partial
// subtree of Layers into a GraphicsLayer.
struct GraphicsLayerPaintInfo {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  PaintLayer* paint_layer;

  LayoutRect composited_bounds;

  // The clip rect to apply, in the local coordinate space of the squashed
  // layer, when painting it.
  IntRect local_clip_rect_for_squashed_layer;

  // Offset describing where this squashed Layer paints into the shared
  // GraphicsLayer backing.
  IntSize offset_from_layout_object;
  bool offset_from_layout_object_set;

  GraphicsLayerPaintInfo()
      : paint_layer(nullptr), offset_from_layout_object_set(false) {}
};

enum GraphicsLayerUpdateScope {
  kGraphicsLayerUpdateNone,
  kGraphicsLayerUpdateLocal,
  kGraphicsLayerUpdateSubtree,
};

// CompositedLayerMapping keeps track of how PaintLayers correspond to
// GraphicsLayers of the composited layer tree. Each instance of
// CompositedLayerMapping manages a small cluster of GraphicsLayers and the
// references to which Layers and paint phases contribute to each GraphicsLayer.
//
// - If a PaintLayer is composited,
//   - if it paints into its own backings (GraphicsLayers), it owns a
//     CompositedLayerMapping (PaintLayer::compositedLayerMapping()) to keep
//     track the backings;
//   - if it paints into grouped backing (i.e. it's squashed), it has a pointer
//     (PaintLayer::groupedMapping()) to the CompositedLayerMapping into which
//     the PaintLayer is squashed;
// - Otherwise the PaintLayer doesn't own or directly reference any
//   CompositedLayerMapping.
class CORE_EXPORT CompositedLayerMapping final : public GraphicsLayerClient {
  WTF_MAKE_NONCOPYABLE(CompositedLayerMapping);
  USING_FAST_MALLOC(CompositedLayerMapping);

 public:
  explicit CompositedLayerMapping(PaintLayer&);
  ~CompositedLayerMapping() override;

  PaintLayer& OwningLayer() const { return owning_layer_; }

  bool UpdateGraphicsLayerConfiguration();
  void UpdateGraphicsLayerGeometry(
      const PaintLayer* compositing_container,
      const PaintLayer* compositing_stacking_context,
      Vector<PaintLayer*>& layers_needing_paint_invalidation);

  // Update whether background paints onto scrolling contents layer.
  void UpdateBackgroundPaintsOntoScrollingContentsLayer();

  // Update whether layer needs blending.
  void UpdateContentsOpaque();

  void UpdateRasterizationPolicy();

  GraphicsLayer* MainGraphicsLayer() const { return graphics_layer_.get(); }

  // Layer to clip children
  bool HasClippingLayer() const { return child_containment_layer_.get(); }
  GraphicsLayer* ClippingLayer() const {
    return child_containment_layer_.get();
  }

  // Layer to get clipped by ancestor
  bool HasAncestorClippingLayer() const {
    return ancestor_clipping_layer_.get();
  }
  GraphicsLayer* AncestorClippingLayer() const {
    return ancestor_clipping_layer_.get();
  }

  GraphicsLayer* AncestorClippingMaskLayer() const {
    return ancestor_clipping_mask_layer_.get();
  }

  GraphicsLayer* ForegroundLayer() const { return foreground_layer_.get(); }

  GraphicsLayer* BackgroundLayer() const { return background_layer_.get(); }

  GraphicsLayer* DecorationOutlineLayer() const {
    return decoration_outline_layer_.get();
  }

  bool BackgroundLayerPaintsFixedRootBackground() const {
    return background_layer_paints_fixed_root_background_;
  }

  bool HasScrollingLayer() const { return scrolling_layer_.get(); }
  GraphicsLayer* ScrollingLayer() const { return scrolling_layer_.get(); }
  GraphicsLayer* ScrollingContentsLayer() const {
    return scrolling_contents_layer_.get();
  }

  bool HasMaskLayer() const { return mask_layer_.get(); }
  GraphicsLayer* MaskLayer() const { return mask_layer_.get(); }

  bool HasChildClippingMaskLayer() const {
    return child_clipping_mask_layer_.get();
  }
  GraphicsLayer* ChildClippingMaskLayer() const {
    return child_clipping_mask_layer_.get();
  }

  GraphicsLayer* ParentForSublayers() const;
  GraphicsLayer* ChildForSuperlayers() const;
  void SetSublayers(const GraphicsLayerVector&);

  bool HasChildTransformLayer() const { return child_transform_layer_.get(); }
  GraphicsLayer* ChildTransformLayer() const {
    return child_transform_layer_.get();
  }

  GraphicsLayer* SquashingContainmentLayer() const {
    return squashing_containment_layer_.get();
  }
  GraphicsLayer* SquashingLayer() const { return squashing_layer_.get(); }

  void SetSquashingContentsNeedDisplay();
  void SetContentsNeedDisplay();
  // LayoutRect is in the coordinate space of the layer's layout object.
  void SetContentsNeedDisplayInRect(const LayoutRect&,
                                    PaintInvalidationReason,
                                    const DisplayItemClient&);
  // Invalidates just the non-scrolling content layers.
  void SetNonScrollingContentsNeedDisplayInRect(const LayoutRect&,
                                                PaintInvalidationReason,
                                                const DisplayItemClient&);
  // Invalidates just scrolling content layers.
  void SetScrollingContentsNeedDisplayInRect(const LayoutRect&,
                                             PaintInvalidationReason,
                                             const DisplayItemClient&);

  // Notification from the layoutObject that its content changed.
  void ContentChanged(ContentChangeType);

  LayoutRect CompositedBounds() const { return composited_bounds_; }
  IntRect PixelSnappedCompositedBounds() const;

  void PositionOverflowControlsLayers();

  // Returns true if the assignment actually changed the assigned squashing
  // layer.
  bool UpdateSquashingLayerAssignment(PaintLayer* squashed_layer,
                                      size_t next_squashed_layer_index);
  void RemoveLayerFromSquashingGraphicsLayer(const PaintLayer*);
#if DCHECK_IS_ON()
  bool VerifyLayerInSquashingVector(const PaintLayer*);
#endif

  void FinishAccumulatingSquashingLayers(
      size_t next_squashed_layer_index,
      Vector<PaintLayer*>& layers_needing_paint_invalidation);
  void UpdateRenderingContext();
  void UpdateShouldFlattenTransform();
  void UpdateElementIdAndCompositorMutableProperties();

  // GraphicsLayerClient interface
  void InvalidateTargetElementForTesting() override;
  IntRect ComputeInterestRect(
      const GraphicsLayer*,
      const IntRect& previous_interest_rect) const override;
  LayoutSize SubpixelAccumulation() const final;
  bool NeedsRepaint(const GraphicsLayer&) const override;
  void PaintContents(const GraphicsLayer*,
                     GraphicsContext&,
                     GraphicsLayerPaintingPhase,
                     const IntRect& interest_rect) const override;

  bool IsTrackingRasterInvalidations() const override;

#if DCHECK_IS_ON()
  void VerifyNotPainting() override;
#endif

  LayoutRect ContentsBox() const;

  GraphicsLayer* LayerForHorizontalScrollbar() const {
    return layer_for_horizontal_scrollbar_.get();
  }
  GraphicsLayer* LayerForVerticalScrollbar() const {
    return layer_for_vertical_scrollbar_.get();
  }
  GraphicsLayer* LayerForScrollCorner() const {
    return layer_for_scroll_corner_.get();
  }

  // Returns true if the overflow controls cannot be positioned within this
  // CLM's internal hierarchy without incorrectly stacking under some
  // scrolling content. If this returns true, these controls must be
  // repositioned in the graphics layer tree to ensure that they stack above
  // scrolling content.
  bool NeedsToReparentOverflowControls() const;

  // Removes the overflow controls host layer from its parent and positions it
  // so that it can be inserted as a sibling to this CLM without changing
  // position.
  GraphicsLayer* DetachLayerForOverflowControls();

  void UpdateFilters(const ComputedStyle&);
  void UpdateBackdropFilters(const ComputedStyle&);

  void SetBlendMode(WebBlendMode);

  bool NeedsGraphicsLayerUpdate() {
    return pending_update_scope_ > kGraphicsLayerUpdateNone;
  }
  void SetNeedsGraphicsLayerUpdate(GraphicsLayerUpdateScope scope) {
    pending_update_scope_ = std::max(
        static_cast<GraphicsLayerUpdateScope>(pending_update_scope_), scope);
  }
  void ClearNeedsGraphicsLayerUpdate() {
    pending_update_scope_ = kGraphicsLayerUpdateNone;
  }

  GraphicsLayerUpdater::UpdateType UpdateTypeForChildren(
      GraphicsLayerUpdater::UpdateType) const;

#if DCHECK_IS_ON()
  void AssertNeedsToUpdateGraphicsLayerBitsCleared() {
    DCHECK_EQ(pending_update_scope_,
              static_cast<unsigned>(kGraphicsLayerUpdateNone));
  }
#endif

  String DebugName(const GraphicsLayer*) const override;

  LayoutSize ContentOffsetInCompositingLayer() const;

  LayoutPoint SquashingOffsetFromTransformedAncestor() {
    return squashing_layer_offset_from_transformed_ancestor_;
  }

  // If there is a squashed layer painting into this CLM that is an ancestor of
  // the given LayoutObject, return it. Otherwise return nullptr.
  const GraphicsLayerPaintInfo* ContainingSquashedLayer(
      const LayoutObject*,
      unsigned max_squashed_layer_index);

  void UpdateScrollingBlockSelection();

  void AdjustForCompositedScrolling(const GraphicsLayer*,
                                    IntSize& offset) const;

  // Returns true for layers with scrollable overflow which have a background
  // that can be painted into the composited scrolling contents layer (i.e.
  // the background can scroll with the content). When the background is also
  // opaque this allows us to composite the scroller even on low DPI as we can
  // draw with subpixel anti-aliasing.
  bool BackgroundPaintsOntoScrollingContentsLayer() {
    return background_paints_onto_scrolling_contents_layer_;
  }

  bool DrawsBackgroundOntoContentLayer() {
    return draws_background_onto_content_layer_;
  }

 private:
  IntRect RecomputeInterestRect(const GraphicsLayer*) const;
  static bool InterestRectChangedEnoughToRepaint(
      const IntRect& previous_interest_rect,
      const IntRect& new_interest_rect,
      const IntSize& layer_size);

  static const GraphicsLayerPaintInfo* ContainingSquashedLayer(
      const LayoutObject*,
      const Vector<GraphicsLayerPaintInfo>& layers,
      unsigned max_squashed_layer_index);

  // Paints the scrollbar part associated with the given graphics layer into the
  // given context.
  void PaintScrollableArea(const GraphicsLayer*,
                           GraphicsContext&,
                           const IntRect& interest_rect) const;
  // Returns whether the given layer is part of the scrollable area, if any,
  // associated with this mapping.
  bool IsScrollableAreaLayer(const GraphicsLayer*) const;

  // Helper methods to updateGraphicsLayerGeometry:
  void ComputeGraphicsLayerParentLocation(
      const PaintLayer* compositing_container,
      const IntRect& ancestor_compositing_bounds,
      IntPoint& graphics_layer_parent_location);
  void UpdateSquashingLayerGeometry(
      const IntPoint& graphics_layer_parent_location,
      const PaintLayer* compositing_container,
      Vector<GraphicsLayerPaintInfo>& layers,
      GraphicsLayer*,
      LayoutPoint* offset_from_transformed_ancestor,
      Vector<PaintLayer*>& layers_needing_paint_invalidation);
  void UpdateMainGraphicsLayerGeometry(
      const IntRect& relative_compositing_bounds,
      const IntRect& local_compositing_bounds,
      const IntPoint& graphics_layer_parent_location);
  void UpdateAncestorClippingLayerGeometry(
      const PaintLayer* compositing_container,
      const IntPoint& snapped_offset_from_composited_ancestor,
      IntPoint& graphics_layer_parent_location);
  void UpdateOverflowControlsHostLayerGeometry(
      const PaintLayer* compositing_stacking_context,
      const PaintLayer* compositing_container,
      IntPoint graphics_layer_parent_location);
  void UpdateChildContainmentLayerGeometry(
      const IntRect& clipping_box,
      const IntRect& local_compositing_bounds);
  void UpdateChildTransformLayerGeometry();
  void UpdateMaskLayerGeometry();
  void UpdateTransformGeometry(
      const IntPoint& snapped_offset_from_composited_ancestor,
      const IntRect& relative_compositing_bounds);
  void UpdateForegroundLayerGeometry(
      const FloatSize& relative_compositing_bounds_size,
      const IntRect& clipping_box);
  void UpdateBackgroundLayerGeometry(
      const FloatSize& relative_compositing_bounds_size);
  void UpdateDecorationOutlineLayerGeometry(
      const FloatSize& relative_compositing_bounds_size);
  void UpdateScrollingLayerGeometry(const IntRect& local_compositing_bounds);
  void UpdateChildClippingMaskLayerGeometry();
  void UpdateStickyConstraints(const ComputedStyle&);

  void CreatePrimaryGraphicsLayer();
  void DestroyGraphicsLayers();

  std::unique_ptr<GraphicsLayer> CreateGraphicsLayer(
      CompositingReasons,
      SquashingDisallowedReasons = kSquashingDisallowedReasonsNone);
  bool ToggleScrollbarLayerIfNeeded(std::unique_ptr<GraphicsLayer>&,
                                    bool needs_layer,
                                    CompositingReasons);

  LayoutBoxModelObject& GetLayoutObject() const {
    return owning_layer_.GetLayoutObject();
  }
  PaintLayerCompositor* Compositor() const {
    return owning_layer_.Compositor();
  }

  void UpdateInternalHierarchy();
  void UpdatePaintingPhases();
  bool UpdateClippingLayers(bool needs_ancestor_clip,
                            bool needs_descendant_clip);
  bool UpdateClippingLayers(bool needs_ancestor_clip,
                            bool needs_ancestor_clipping_mask,
                            bool needs_descendant_clip);
  bool UpdateChildTransformLayer(bool needs_child_transform_layer);
  bool UpdateOverflowControlsLayers(bool needs_horizontal_scrollbar_layer,
                                    bool needs_vertical_scrollbar_layer,
                                    bool needs_scroll_corner_layer,
                                    bool needs_ancestor_clip);
  bool UpdateForegroundLayer(bool needs_foreground_layer);
  bool UpdateBackgroundLayer(bool needs_background_layer);
  bool UpdateDecorationOutlineLayer(bool needs_decoration_outline_layer);
  bool UpdateMaskLayer(bool needs_mask_layer);
  void UpdateChildClippingMaskLayer(bool needs_child_clipping_mask_layer);
  bool RequiresHorizontalScrollbarLayer() const {
    return owning_layer_.GetScrollableArea() &&
           owning_layer_.GetScrollableArea()->HorizontalScrollbar();
  }
  bool RequiresVerticalScrollbarLayer() const {
    return owning_layer_.GetScrollableArea() &&
           owning_layer_.GetScrollableArea()->VerticalScrollbar();
  }
  bool RequiresScrollCornerLayer() const {
    return owning_layer_.GetScrollableArea() &&
           !owning_layer_.GetScrollableArea()
                ->ScrollCornerAndResizerRect()
                .IsEmpty();
  }
  bool UpdateScrollingLayers(bool scrolling_layers);
  void UpdateScrollParent(const PaintLayer*);
  void UpdateClipParent(const PaintLayer* scroll_parent);
  bool UpdateSquashingLayers(bool needs_squashing_layers);
  void UpdateDrawsContent();
  void UpdateChildrenTransform();
  void UpdateCompositedBounds();
  void RegisterScrollingLayers();

  // Also sets subpixelAccumulation on the layer.
  void ComputeBoundsOfOwningLayer(
      const PaintLayer* composited_ancestor,
      IntRect& local_compositing_bounds,
      IntRect& compositing_bounds_relative_to_composited_ancestor,
      LayoutPoint& offset_from_composited_ancestor,
      IntPoint& snapped_offset_from_composited_ancestor);

  void SetBackgroundLayerPaintsFixedRootBackground(bool);

  GraphicsLayerPaintingPhase PaintingPhaseForPrimaryLayer() const;

  // Result is transform origin in pixels.
  FloatPoint3D ComputeTransformOrigin(const IntRect& border_box) const;

  void UpdateOpacity(const ComputedStyle&);
  void UpdateTransform(const ComputedStyle&);
  void UpdateLayerBlendMode(const ComputedStyle&);
  void UpdateIsRootForIsolatedGroup();
  // Return the opacity value that this layer should use for compositing.
  float CompositingOpacity(float layout_object_opacity) const;

  bool PaintsChildren() const;

  // Returns true if this layer has content that needs to be displayed by
  // painting into the backing store.
  bool ContainsPaintedContent() const;
  // Returns true if the Layer just contains an image that we can composite
  // directly.
  bool IsDirectlyCompositedImage() const;
  void UpdateImageContents();

  Color LayoutObjectBackgroundColor() const;
  void UpdateBackgroundColor();
  void UpdateContentsRect();
  void UpdateContentsOffsetInCompositingLayer(
      const IntPoint& snapped_offset_from_composited_ancestor,
      const IntPoint& graphics_layer_parent_location);
  void UpdateAfterPartResize();
  void UpdateCompositingReasons();

  static bool HasVisibleNonCompositingDescendant(PaintLayer* parent);

  void DoPaintTask(const GraphicsLayerPaintInfo&,
                   const GraphicsLayer&,
                   const PaintLayerFlags&,
                   GraphicsContext&,
                   const IntRect& clip) const;

  // Computes the background clip rect for the given squashed layer, up to any
  // containing layer that is squashed into the same squashing layer and
  // contains this squashed layer's clipping ancestor.  The clip rect is
  // returned in the coordinate space of the given squashed layer.  If there is
  // no such containing layer, returns the infinite rect.
  // FIXME: unify this code with the code that sets up m_ancestorClippingLayer.
  // They are doing very similar things.
  static IntRect LocalClipRectForSquashedLayer(
      const PaintLayer& reference_layer,
      const GraphicsLayerPaintInfo&,
      const Vector<GraphicsLayerPaintInfo>& layers);

  // Conservatively check that a sequence of border-radius clips do not clip
  // this layer. The compositing_ancestor is the nearest compositing ancestor
  // layer and we can stop checking clips at that layer because higher layer
  // clips will be applied elsewhere.
  // This is a fast approximate test. Depending on the shape of the child and
  // the size of the clips, this method may return false when in fact
  // the child is not clipped. We accept the approximation because most border
  // radii are small and the outcome is used to reduce the number of layers,
  // not influence correctness.
  bool AncestorRoundedCornersWontClip(const PaintLayer* compositing_ancestor);

  // Return true in |owningLayerIsClipped| iff |m_owningLayer|'s compositing
  // ancestor is not a descendant (inclusive) of the clipping container for
  // |m_owningLayer|. Return true in |owningLayerIsMasked| iff
  // |owningLayerIsClipped| is true and |m_owningLayer|'s compositing ancestor
  // is not a descendant (inclusive) of a container that applies a mask for
  // |m_owningLayer|.
  void OwningLayerClippedOrMaskedByLayerNotAboveCompositedAncestor(
      const PaintLayer* scroll_parent,
      bool& owning_layer_is_clipped,
      bool& owning_layer_is_masked);

  const PaintLayer* ScrollParent();

  // Clear the groupedMapping entry on the layer at the given index, only if
  // that layer does not appear earlier in the set of layers for this object.
  bool InvalidateLayerIfNoPrecedingEntry(size_t);

  PaintLayer& owning_layer_;

  // The hierarchy of layers that is maintained by the CompositedLayerMapping
  // looks like this:
  //
  //  + m_ancestorClippingLayer [OPTIONAL]
  //    + m_graphicsLayer
  //      + m_childTransformLayer [OPTIONAL]
  //      | + m_childContainmentLayer [OPTIONAL]
  //      |   <-OR->
  //      |   (m_scrollingLayer + m_scrollingContentsLayer) [OPTIONAL]
  //      + m_overflowControlsAncestorClippingLayer [OPTIONAL]
  //      | + m_overflowControlsHostLayer [OPTIONAL]
  //      |   + m_layerForVerticalScrollbar [OPTIONAL]
  //      |   + m_layerForHorizontalScrollbar [OPTIONAL]
  //      |   + m_layerForScrollCorner [OPTIONAL]
  //      + m_decorationOutlineLayer [OPTIONAL]
  // The overflow controls may need to be repositioned in the graphics layer
  // tree by the RLC to ensure that they stack above scrolling content.
  //
  // We need an ancestor clipping layer if our clipping ancestor is not our
  // ancestor in the clipping tree. Here's what that might look like.
  //
  // Let A = the clipping ancestor,
  //     B = the clip descendant, and
  //     SC = the stacking context that is the ancestor of A and B in the
  //          stacking tree.
  //
  // SC
  //  + A = m_graphicsLayer
  //  |  + m_childContainmentLayer
  //  |     + ...
  //  ...
  //  |
  //  + B = m_ancestorClippingLayer [+]
  //     + m_graphicsLayer
  //        + ...
  //
  // In this case B is clipped by another layer that doesn't happen to be its
  // ancestor: A.  So we create an ancestor clipping layer for B, [+], which
  // ensures that B is clipped as if it had been A's descendant.
  // In addition, the m_ancestorClippingLayer will have an associated
  // mask layer if the ancestor, A, has a border radius that requires a
  // rounded corner clip rect. The mask is not part of the layer tree; rather
  // it is attached to the m_ancestorClippingLayer itself.
  //
  // Layers that require a CSS mask also have a mask layer attached to them.

  // Only used if we are clipped by an ancestor which is not a stacking context.
  std::unique_ptr<GraphicsLayer> ancestor_clipping_layer_;

  // Only used is there is an m_ancestorClippingLayer that also needs to apply
  // a clipping mask (for CSS clips or border radius).
  std::unique_ptr<GraphicsLayer> ancestor_clipping_mask_layer_;

  std::unique_ptr<GraphicsLayer> graphics_layer_;

  // Only used if we have clipping on a stacking context with compositing
  // children.
  std::unique_ptr<GraphicsLayer> child_containment_layer_;

  // Only used if we have perspective.
  std::unique_ptr<GraphicsLayer> child_transform_layer_;

  // Only used if the layer is using composited scrolling.
  std::unique_ptr<GraphicsLayer> scrolling_layer_;

  // Only used if the layer is using composited scrolling.
  std::unique_ptr<GraphicsLayer> scrolling_contents_layer_;

  // This layer is also added to the hierarchy by the RLB, but in a different
  // way than the layers above. It's added to m_graphicsLayer as its mask layer
  // (naturally) if we have a mask, and isn't part of the typical hierarchy (it
  // has no children).
  // Only used if we have a mask.
  std::unique_ptr<GraphicsLayer> mask_layer_;

  // Only used if we have to clip child layers or accelerated contents with
  // border radius or clip-path.
  std::unique_ptr<GraphicsLayer> child_clipping_mask_layer_;

  // There are two other (optional) layers whose painting is managed by the
  // CompositedLayerMapping, but whose position in the hierarchy is maintained
  // by the PaintLayerCompositor. These are the foreground and background
  // layers. The foreground layer exists if we have composited descendants with
  // negative z-order. We need the extra layer in this case because the layer
  // needs to draw both below (for the background, say) and above (for the
  // normal flow content, say) the negative z-order descendants and this is
  // impossible with a single layer. The RLC handles inserting m_foregroundLayer
  // in the correct position in our descendant list for us (right after the neg
  // z-order dsecendants).
  //
  // The background layer is only created if this is the root layer and our
  // background is entirely fixed. In this case we want to put the background in
  // a separate composited layer so that when we scroll, we don't have to
  // re-raster the background into position. This layer is also inserted into
  // the tree by the RLC as it gets a special home. This layer becomes a
  // descendant of the frame clipping layer. That is:
  //   ...
  //     + frame clipping layer
  //       + m_backgroundLayer
  //       + frame scrolling layer
  //         + root content layer
  //
  // With the hierarchy set up like this, the root content layer is able to
  // scroll without affecting the background layer (or paint invalidation).

  // Only used in cases where we need to draw the foreground separately.
  std::unique_ptr<GraphicsLayer> foreground_layer_;

  // Only used in cases where we need to draw the background separately.
  std::unique_ptr<GraphicsLayer> background_layer_;

  std::unique_ptr<GraphicsLayer> layer_for_horizontal_scrollbar_;
  std::unique_ptr<GraphicsLayer> layer_for_vertical_scrollbar_;
  std::unique_ptr<GraphicsLayer> layer_for_scroll_corner_;

  // This layer contains the scrollbar and scroll corner layers and clips them
  // to the border box bounds of our LayoutObject. It is usually added to
  // m_graphicsLayer, but may be reparented by GraphicsLayerTreeBuilder to
  // ensure that scrollbars appear above scrolling content.
  std::unique_ptr<GraphicsLayer> overflow_controls_host_layer_;

  // The reparented overflow controls sometimes need to be clipped by a
  // non-ancestor. In just the same way we need an ancestor clipping layer to
  // clip this CLM's internal hierarchy, we add another layer to clip the
  // overflow controls. We could combine this with m_overflowControlsHostLayer,
  // but that would require manually intersecting their clips, and shifting the
  // overflow controls to compensate for this clip's offset. By using a separate
  // layer, the overflow controls can remain ignorant of ancestor clipping.
  std::unique_ptr<GraphicsLayer> overflow_controls_ancestor_clipping_layer_;

  // DecorationLayer which paints outline.
  std::unique_ptr<GraphicsLayer> decoration_outline_layer_;

  // A squashing CLM has two possible squashing-related structures.
  //
  // If m_ancestorClippingLayer is present:
  //
  // m_ancestorClippingLayer
  //   + m_graphicsLayer
  //   + m_squashingLayer
  //
  // If not:
  //
  // m_squashingContainmentLayer
  //   + m_graphicsLayer
  //   + m_squashingLayer
  //
  // Stacking children of a squashed layer receive graphics layers that are
  // parented to the compositd ancestor of the squashed layer (i.e. nearest
  // enclosing composited layer that is not
  // squashed).

  // Only used if any squashed layers exist and m_squashingContainmentLayer is
  // not present, to contain the squashed layers as siblings to the rest of the
  // GraphicsLayer tree chunk.
  std::unique_ptr<GraphicsLayer> squashing_containment_layer_;

  // Only used if any squashed layers exist, this is the backing that squashed
  // layers paint into.
  std::unique_ptr<GraphicsLayer> squashing_layer_;
  Vector<GraphicsLayerPaintInfo> squashed_layers_;
  LayoutPoint squashing_layer_offset_from_transformed_ancestor_;

  LayoutRect composited_bounds_;

  LayoutSize content_offset_in_compositing_layer_;

  // We keep track of the scrolling contents offset, so that when it changes we
  // can notify the ScrollingCoordinator, which passes on main-thread scrolling
  // updates to the compositor.
  DoubleSize scrolling_contents_offset_;

  unsigned content_offset_in_compositing_layer_dirty_ : 1;

  unsigned pending_update_scope_ : 2;
  unsigned is_main_frame_layout_view_layer_ : 1;

  unsigned background_layer_paints_fixed_root_background_ : 1;
  unsigned scrolling_contents_are_empty_ : 1;

  // Keep track of whether the background is painted onto the scrolling contents
  // layer for invalidations.
  unsigned background_paints_onto_scrolling_contents_layer_ : 1;

  // Solid color border boxes may be painted into both the scrolling contents
  // layer and the graphics layer because the scrolling contents layer is
  // clipped by the padding box.
  unsigned background_paints_onto_graphics_layer_ : 1;

  bool draws_background_onto_content_layer_;

  friend class CompositedLayerMappingTest;
};

}  // namespace blink

#endif  // CompositedLayerMapping_h
