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

#include "core/rendering/RenderLayer.h"
#include "core/rendering/compositing/GraphicsLayerUpdater.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/transforms/TransformationMatrix.h"

namespace WebCore {

class KeyframeList;
class RenderLayerCompositor;

// A GraphicsLayerPaintInfo contains all the info needed to paint a partial subtree of RenderLayers into a GraphicsLayer.
struct GraphicsLayerPaintInfo {
    RenderLayer* renderLayer;

    LayoutRect compositedBounds;

    // At first, the m_squashingLayer's bounds/location are not known. The value offsetFromSquashingCLM is
    // an intermediate offset for a squashed RenderLayer, described with respect to the CompositedLayerMapping's
    // owning layer that would eventually have the m_squashingLayer. Once the shared GraphicsLayer's bounds are
    // known, then we can trivially convert this offset to m_squashingLayer's space.
    LayoutSize offsetFromSquashingCLM;

    // Offset describing where this squashed RenderLayer paints into the shared GraphicsLayer backing.
    IntSize offsetFromRenderer;
    LayoutSize subpixelAccumulation;

    GraphicsLayerPaintingPhase paintingPhase;

    bool isBackgroundLayer;

    bool isEquivalentForSquashing(const GraphicsLayerPaintInfo& other)
    {
        // FIXME: offsetFromRenderer and compositedBounds should not be checked here, because
        // they are not yet fixed at the time this function is used.
        return renderLayer == other.renderLayer
            && offsetFromSquashingCLM == other.offsetFromSquashingCLM
            && paintingPhase == other.paintingPhase
            && isBackgroundLayer == other.isBackgroundLayer;
    }
};

// CompositedLayerMapping keeps track of how RenderLayers of the render tree correspond to
// GraphicsLayers of the composited layer tree. Each instance of CompositedLayerMapping
// manages a small cluster of GraphicsLayers and the references to which RenderLayers
// and paint phases contribute to each GraphicsLayer.
//
// Currently (Oct. 2013) there is one CompositedLayerMapping for each RenderLayer,
// but this is likely to evolve soon.
class CompositedLayerMapping FINAL : public GraphicsLayerClient {
    WTF_MAKE_NONCOPYABLE(CompositedLayerMapping); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CompositedLayerMapping(RenderLayer&);
    virtual ~CompositedLayerMapping();

    RenderLayer& owningLayer() const { return m_owningLayer; }

    // Returns true if layer configuration changed.
    bool updateGraphicsLayerConfiguration(GraphicsLayerUpdater::UpdateType);
    // Update graphics layer position and bounds.
    void updateGraphicsLayerGeometry(GraphicsLayerUpdater::UpdateType);
    // Update whether layer needs blending.
    void updateContentsOpaque();

    GraphicsLayer* mainGraphicsLayer() const { return m_graphicsLayer.get(); }

    // Layer to clip children
    bool hasClippingLayer() const { return m_childContainmentLayer; }
    GraphicsLayer* clippingLayer() const { return m_childContainmentLayer.get(); }

    // Layer to get clipped by ancestor
    bool hasAncestorClippingLayer() const { return m_ancestorClippingLayer; }
    GraphicsLayer* ancestorClippingLayer() const { return m_ancestorClippingLayer.get(); }

    bool hasContentsLayer() const { return m_foregroundLayer; }
    GraphicsLayer* foregroundLayer() const { return m_foregroundLayer.get(); }

    GraphicsLayer* backgroundLayer() const { return m_backgroundLayer.get(); }
    bool backgroundLayerPaintsFixedRootBackground() const { return m_backgroundLayerPaintsFixedRootBackground; }

    bool hasScrollingLayer() const { return m_scrollingLayer; }
    GraphicsLayer* scrollingLayer() const { return m_scrollingLayer.get(); }
    GraphicsLayer* scrollingContentsLayer() const { return m_scrollingContentsLayer.get(); }

    bool hasMaskLayer() const { return m_maskLayer; }
    GraphicsLayer* maskLayer() const { return m_maskLayer.get(); }

    bool hasChildClippingMaskLayer() const { return m_childClippingMaskLayer; }
    GraphicsLayer* childClippingMaskLayer() const { return m_childClippingMaskLayer.get(); }

    GraphicsLayer* parentForSublayers() const;
    GraphicsLayer* childForSuperlayers() const;
    // localRootForOwningLayer does not include the m_squashingContainmentLayer, which is technically not associated with this CLM's owning layer.
    GraphicsLayer* localRootForOwningLayer() const;

    GraphicsLayer* childTransformLayer() const { return m_childTransformLayer.get(); }

    GraphicsLayer* squashingContainmentLayer() const { return m_squashingContainmentLayer.get(); }
    GraphicsLayer* squashingLayer() const { return m_squashingLayer.get(); }
    // Contains the bottommost layer in the hierarchy that can contain the children transform.
    GraphicsLayer* layerForChildrenTransform() const;

    // Returns true for a composited layer that has no backing store of its own, so
    // paints into some ancestor layer.
    bool paintsIntoCompositedAncestor() const { return !(m_requiresOwnBackingStoreForAncestorReasons || m_requiresOwnBackingStoreForIntrinsicReasons); }

    // Updates whether a backing store is needed based on the layer's compositing ancestor's
    // properties; returns true if the need for a backing store for ancestor reasons changed.
    bool updateRequiresOwnBackingStoreForAncestorReasons(const RenderLayer* compositingAncestor);

    // Updates whether a backing store is needed for intrinsic reasons (that is, based on the
    // layer's own properties or compositing reasons); returns true if the intrinsic need for
    // a backing store changed.
    bool updateRequiresOwnBackingStoreForIntrinsicReasons();

    void setContentsNeedDisplay();
    // r is in the coordinate space of the layer's render object
    void setContentsNeedDisplayInRect(const IntRect&);

    // Notification from the renderer that its content changed.
    void contentChanged(ContentChangeType);

    LayoutRect compositedBounds() const { return m_compositedBounds; }
    IntRect pixelSnappedCompositedBounds() const;
    void setCompositedBounds(const LayoutRect&);
    void updateCompositedBounds(GraphicsLayerUpdater::UpdateType);

    void updateAfterWidgetResize();
    void positionOverflowControlsLayers(const IntSize& offsetFromRoot);
    bool hasUnpositionedOverflowControlsLayers() const;

    // Returns true if the assignment actually changed the assigned squashing layer.
    bool updateSquashingLayerAssignment(RenderLayer*, LayoutSize offsetFromSquashingCLM, size_t nextSquashedLayerIndex);
    void removeRenderLayerFromSquashingGraphicsLayer(const RenderLayer*);

    void finishAccumulatingSquashingLayers(size_t nextSquashedLayerIndex);
    void updateRenderingContext();
    void updateShouldFlattenTransform();

    // GraphicsLayerClient interface
    virtual void notifyAnimationStarted(const GraphicsLayer*, double monotonicTime) OVERRIDE;
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& clip) OVERRIDE;
    virtual bool isTrackingRepaints() const OVERRIDE;

    PassOwnPtr<Vector<FloatRect> > collectTrackedRepaintRects() const;

#ifndef NDEBUG
    virtual void verifyNotPainting() OVERRIDE;
#endif

    LayoutRect contentsBox() const;

    GraphicsLayer* layerForHorizontalScrollbar() const { return m_layerForHorizontalScrollbar.get(); }
    GraphicsLayer* layerForVerticalScrollbar() const { return m_layerForVerticalScrollbar.get(); }
    GraphicsLayer* layerForScrollCorner() const { return m_layerForScrollCorner.get(); }

    void updateFilters(const RenderStyle*);
    bool canCompositeFilters() const { return m_canCompositeFilters; }

    void setBlendMode(blink::WebBlendMode);

    void setNeedsGraphicsLayerUpdate();
    bool shouldUpdateGraphicsLayer(GraphicsLayerUpdater::UpdateType updateType) const { return m_needToUpdateGraphicsLayer || updateType == GraphicsLayerUpdater::ForceUpdate; }
    GraphicsLayerUpdater::UpdateType updateTypeForChildren(GraphicsLayerUpdater::UpdateType) const;
    void clearNeedsGraphicsLayerUpdate();

#if !ASSERT_DISABLED
    void assertNeedsToUpdateGraphicsLayerBitsCleared();
#endif

    virtual String debugName(const GraphicsLayer*) OVERRIDE;

private:
    void createPrimaryGraphicsLayer();
    void destroyGraphicsLayers();

    PassOwnPtr<GraphicsLayer> createGraphicsLayer(CompositingReasons);
    bool toggleScrollbarLayerIfNeeded(OwnPtr<GraphicsLayer>&, bool needsLayer, CompositingReasons);

    RenderLayerModelObject* renderer() const { return m_owningLayer.renderer(); }
    RenderLayerCompositor* compositor() const { return m_owningLayer.compositor(); }

    void updateInternalHierarchy();
    bool updateClippingLayers(bool needsAncestorClip, bool needsDescendantClip);
    bool updateChildTransformLayer(bool needsChildTransformLayer);
    bool updateOverflowControlsLayers(bool needsHorizontalScrollbarLayer, bool needsVerticalScrollbarLayer, bool needsScrollCornerLayer);
    bool updateForegroundLayer(bool needsForegroundLayer);
    bool updateBackgroundLayer(bool needsBackgroundLayer);
    bool updateMaskLayer(bool needsMaskLayer);
    bool updateClippingMaskLayers(bool needsChildClippingMaskLayer);
    bool requiresHorizontalScrollbarLayer() const { return m_owningLayer.scrollableArea() && m_owningLayer.scrollableArea()->horizontalScrollbar(); }
    bool requiresVerticalScrollbarLayer() const { return m_owningLayer.scrollableArea() && m_owningLayer.scrollableArea()->verticalScrollbar(); }
    bool requiresScrollCornerLayer() const { return m_owningLayer.scrollableArea() && !m_owningLayer.scrollableArea()->scrollCornerAndResizerRect().isEmpty(); }
    bool updateScrollingLayers(bool scrollingLayers);
    void updateScrollParent(RenderLayer*);
    void updateClipParent(RenderLayer*);
    bool updateSquashingLayers(bool needsSquashingLayers);
    void updateDrawsContent();
    void updateChildrenTransform();
    void registerScrollingLayers();

    void adjustBoundsForSubPixelAccumulation(const RenderLayer* compositedAncestor, IntRect& localCompositingBounds, IntRect& relativeCompositingBounds, IntPoint& delta);

    void setBackgroundLayerPaintsFixedRootBackground(bool);

    GraphicsLayerPaintingPhase paintingPhaseForPrimaryLayer() const;

    LayoutSize contentOffsetInCompostingLayer() const;
    // Result is transform origin in pixels.
    FloatPoint3D computeTransformOrigin(const IntRect& borderBox) const;
    // Result is perspective origin in pixels.
    FloatPoint computePerspectiveOrigin(const IntRect& borderBox) const;

    void updateSquashingLayerGeometry(const IntPoint& delta);

    void updateOpacity(const RenderStyle*);
    void updateTransform(const RenderStyle*);
    void updateLayerBlendMode(const RenderStyle*);
    void updateIsRootForIsolatedGroup();
    void updateHasGpuRasterizationHint(const RenderStyle*);
    // Return the opacity value that this layer should use for compositing.
    float compositingOpacity(float rendererOpacity) const;

    bool isMainFrameRenderViewLayer() const;

    bool paintsChildren() const;

    // Returns true if this layer has content that needs to be rendered by painting into the backing store.
    bool containsPaintedContent() const;
    // Returns true if the RenderLayer just contains an image that we can composite directly.
    bool isDirectlyCompositedImage() const;
    void updateImageContents();

    Color rendererBackgroundColor() const;
    void updateBackgroundColor();
    void updateContentsRect();

    void updateCompositingReasons();

    bool hasVisibleNonCompositingDescendantLayers() const;

    bool shouldClipCompositedBounds() const;

    void paintsIntoCompositedAncestorChanged();

    void doPaintTask(GraphicsLayerPaintInfo&, GraphicsContext*, const IntRect& clip);

    RenderLayer& m_owningLayer;

    // The hierarchy of layers that is maintained by the CompositedLayerMapping looks like this:
    //
    //  + m_ancestorClippingLayer [OPTIONAL]
    //     + m_graphicsLayer
    //        + m_childContainmentLayer [OPTIONAL] <-OR-> m_scrollingLayer [OPTIONAL] <-OR-> m_childTransformLayer
    //                                                     + m_scrollingContentsLayer [OPTIONAL]
    //
    // We need an ancestor clipping layer if our clipping ancestor is not our ancestor in the
    // clipping tree. Here's what that might look like.
    //
    // Let A = the clipping ancestor,
    //     B = the clip descendant, and
    //     SC = the stacking context that is the ancestor of A and B in the stacking tree.
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
    // In this case B is clipped by another layer that doesn't happen to be its ancestor: A.
    // So we create an ancestor clipping layer for B, [+], which ensures that B is clipped
    // as if it had been A's descendant.
    OwnPtr<GraphicsLayer> m_ancestorClippingLayer; // Only used if we are clipped by an ancestor which is not a stacking context.
    OwnPtr<GraphicsLayer> m_graphicsLayer;
    OwnPtr<GraphicsLayer> m_childContainmentLayer; // Only used if we have clipping on a stacking context with compositing children.
    OwnPtr<GraphicsLayer> m_childTransformLayer; // Only used if we have perspective and no m_childContainmentLayer.
    OwnPtr<GraphicsLayer> m_scrollingLayer; // Only used if the layer is using composited scrolling.
    OwnPtr<GraphicsLayer> m_scrollingContentsLayer; // Only used if the layer is using composited scrolling.

    // This layer is also added to the hierarchy by the RLB, but in a different way than
    // the layers above. It's added to m_graphicsLayer as its mask layer (naturally) if
    // we have a mask, and isn't part of the typical hierarchy (it has no children).
    OwnPtr<GraphicsLayer> m_maskLayer; // Only used if we have a mask.
    OwnPtr<GraphicsLayer> m_childClippingMaskLayer; // Only used if we have to clip child layers or accelerated contents with border radius or clip-path.

    // There are two other (optional) layers whose painting is managed by the CompositedLayerMapping,
    // but whose position in the hierarchy is maintained by the RenderLayerCompositor. These
    // are the foreground and background layers. The foreground layer exists if we have composited
    // descendants with negative z-order. We need the extra layer in this case because the layer
    // needs to draw both below (for the background, say) and above (for the normal flow content, say)
    // the negative z-order descendants and this is impossible with a single layer. The RLC handles
    // inserting m_foregroundLayer in the correct position in our descendant list for us (right after
    // the neg z-order dsecendants).
    //
    // The background layer is only created if this is the root layer and our background is entirely
    // fixed. In this case we want to put the background in a separate composited layer so that when
    // we scroll, we don't have to re-raster the background into position. This layer is also inserted
    // into the tree by the RLC as it gets a special home. This layer becomes a descendant of the
    // frame clipping layer. That is:
    //   ...
    //     + frame clipping layer
    //       + m_backgroundLayer
    //       + frame scrolling layer
    //         + root content layer
    //
    // With the hierarchy set up like this, the root content layer is able to scroll without affecting
    // the background layer (or repainting).
    OwnPtr<GraphicsLayer> m_foregroundLayer; // Only used in cases where we need to draw the foreground separately.
    OwnPtr<GraphicsLayer> m_backgroundLayer; // Only used in cases where we need to draw the background separately.

    OwnPtr<GraphicsLayer> m_layerForHorizontalScrollbar;
    OwnPtr<GraphicsLayer> m_layerForVerticalScrollbar;
    OwnPtr<GraphicsLayer> m_layerForScrollCorner;

    // A squashing CLM has two possible squashing-related structures.
    //
    // If m_clippingAncestorLayer is present:
    //
    // m_clippingAncestorLayer
    //   + m_graphicsLayer
    //   + m_squashingLayer
    //
    // If not:
    //
    // m_squashingContainmentLayer
    //   + m_graphicsLayer
    //   + m_squashingLayer
    OwnPtr<GraphicsLayer> m_squashingContainmentLayer; // Only used if any squashed layers exist and m_squashingContainmentLayer is not present, to contain the squashed layers as siblings to the rest of the GraphicsLayer tree chunk.
    OwnPtr<GraphicsLayer> m_squashingLayer; // Only used if any squashed layers exist, this is the backing that squashed layers paint into.
    Vector<GraphicsLayerPaintInfo> m_squashedLayers;

    LayoutRect m_compositedBounds;

    bool m_artificiallyInflatedBounds : 1; // bounds had to be made non-zero to make transform-origin work
    bool m_isMainFrameRenderViewLayer : 1;
    bool m_requiresOwnBackingStoreForIntrinsicReasons : 1;
    bool m_requiresOwnBackingStoreForAncestorReasons : 1;
    bool m_canCompositeFilters : 1;
    bool m_backgroundLayerPaintsFixedRootBackground : 1;
    bool m_needToUpdateGraphicsLayer : 1;
    bool m_needToUpdateGraphicsLayerOfAllDecendants : 1;
};

} // namespace WebCore

#endif // CompositedLayerMapping_h
