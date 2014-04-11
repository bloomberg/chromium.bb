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

#ifndef RenderLayerCompositor_h
#define RenderLayerCompositor_h

#include "core/page/ChromeClient.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/compositing/CompositingPropertyUpdater.h"
#include "core/rendering/compositing/CompositingReasonFinder.h"
#include "core/rendering/compositing/GraphicsLayerUpdater.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "wtf/HashMap.h"

namespace WebCore {

class FixedPositionViewportConstraints;
class GraphicsLayer;
class RenderEmbeddedObject;
class RenderLayerStackingNode;
class RenderPart;
class RenderVideo;
class ScrollingCoordinator;
class StickyPositionViewportConstraints;

enum CompositingUpdateType {
    CompositingUpdateNone,
    CompositingUpdateAfterStyleChange,
    CompositingUpdateAfterLayout,
    CompositingUpdateOnScroll,
    CompositingUpdateOnCompositedScroll,
    CompositingUpdateAfterCanvasContextChange,
};

// RenderLayerCompositor manages the hierarchy of
// composited RenderLayers. It determines which RenderLayers
// become compositing, and creates and maintains a hierarchy of
// GraphicsLayers based on the RenderLayer painting order.
//
// There is one RenderLayerCompositor per RenderView.

class RenderLayerCompositor FINAL : public GraphicsLayerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RenderLayerCompositor(RenderView&);
    virtual ~RenderLayerCompositor();

    void updateIfNeededRecursive();

    // Return true if this RenderView is in "compositing mode" (i.e. has one or more
    // composited RenderLayers)
    bool inCompositingMode() const { return m_compositing; }
    // This will make a compositing layer at the root automatically, and hook up to
    // the native view/window system.
    void enableCompositingMode(bool enable = true);

    bool inForcedCompositingMode() const { return m_forceCompositingMode; }

    // Returns true if the accelerated compositing is enabled
    bool hasAcceleratedCompositing() const { return m_hasAcceleratedCompositing; }
    bool layerSquashingEnabled() const;

    bool legacyOrCurrentAcceleratedCompositingForOverflowScrollEnabled() const;
    bool legacyAcceleratedCompositingForOverflowScrollEnabled() const;

    bool acceleratedCompositingForOverflowScrollEnabled() const;

    bool canRender3DTransforms() const;

    void updateForceCompositingMode();

    // Copy the accelerated compositing related flags from Settings
    void updateAcceleratedCompositingSettings();

    // Called when the layer hierarchy needs to be updated (compositing layers have been
    // created, destroyed or re-parented).
    void setCompositingLayersNeedRebuild();

    // Updating properties required for determining if compositing is necessary.
    void updateCompositingRequirementsState();
    void setNeedsUpdateCompositingRequirementsState() { m_needsUpdateCompositingRequirementsState = true; }

    // Used to indicate that a compositing update will be needed for the next frame that gets drawn.
    void setNeedsCompositingUpdate(CompositingUpdateType);

    enum UpdateLayerCompositingStateOptions {
        Normal,
        UseChickenEggHacks, // Use this to trigger temporary chicken-egg hacks. See crbug.com/339892.
    };

    // Update the compositing dirty bits, based on the compositing-impacting properties of the layer.
    // (At the moment, it also has some legacy compatibility hacks.)
    void updateLayerCompositingState(RenderLayer*, UpdateLayerCompositingStateOptions = Normal);

    // Whether layer's compositedLayerMapping needs a GraphicsLayer to do clipping by an ancestor (non-stacking-context parent with overflow).
    bool clippedByAncestor(const RenderLayer*) const;
    // Whether layer's compositedLayerMapping needs a GraphicsLayer to clip z-order children of the given RenderLayer.
    bool clipsCompositingDescendants(const RenderLayer*) const;

    // Whether the given layer needs an extra 'contents' layer.
    bool needsContentsCompositingLayer(const RenderLayer*) const;

    bool supportsFixedRootBackgroundCompositing() const;
    bool needsFixedRootBackgroundLayer(const RenderLayer*) const;
    GraphicsLayer* fixedRootBackgroundLayer() const;
    void setNeedsUpdateFixedBackground() { m_needsUpdateFixedBackground = true; }

    // Repaint the appropriate layers when the given RenderLayer starts or stops being composited.
    void repaintOnCompositingChange(RenderLayer*);

    void repaintInCompositedAncestor(RenderLayer*, const LayoutRect&);

    // Notify us that a layer has been added or removed
    void layerWasAdded(RenderLayer* parent, RenderLayer* child);
    void layerWillBeRemoved(RenderLayer* parent, RenderLayer* child);

    void repaintCompositedLayers();

    RenderLayer* rootRenderLayer() const;
    GraphicsLayer* rootGraphicsLayer() const;
    GraphicsLayer* scrollLayer() const;
    GraphicsLayer* containerLayer() const;

    // We don't always have a root transform layer. This function lazily allocates one
    // and returns it as required.
    GraphicsLayer* ensureRootTransformLayer();

    enum RootLayerAttachment {
        RootLayerUnattached,
        RootLayerAttachedViaChromeClient,
        RootLayerAttachedViaEnclosingFrame
    };

    RootLayerAttachment rootLayerAttachment() const { return m_rootLayerAttachment; }
    void updateRootLayerAttachment();
    void updateRootLayerPosition();

    void setIsInWindow(bool);

    static RenderLayerCompositor* frameContentsCompositor(RenderPart*);
    // Return true if the layers changed.
    static bool parentFrameContentLayers(RenderPart*);

    // Update the geometry of the layers used for clipping and scrolling in frames.
    void frameViewDidChangeLocation(const IntPoint& contentsOffset);
    void frameViewDidChangeSize();
    void frameViewDidScroll();
    void frameViewScrollbarsExistenceDidChange();
    void rootFixedBackgroundsChanged();

    bool scrollingLayerDidChange(RenderLayer*);

    String layerTreeAsText(LayerTreeFlags);

    GraphicsLayer* layerForHorizontalScrollbar() const { return m_layerForHorizontalScrollbar.get(); }
    GraphicsLayer* layerForVerticalScrollbar() const { return m_layerForVerticalScrollbar.get(); }
    GraphicsLayer* layerForScrollCorner() const { return m_layerForScrollCorner.get(); }

    void updateViewportConstraintStatus(RenderLayer*);
    void removeViewportConstrainedLayer(RenderLayer*);

    void addOutOfFlowPositionedLayer(RenderLayer*);
    void removeOutOfFlowPositionedLayer(RenderLayer*);

    void resetTrackedRepaintRects();
    void setTracksRepaints(bool);

    void setNeedsToRecomputeCompositingRequirements() { m_needsToRecomputeCompositingRequirements = true; }

    virtual String debugName(const GraphicsLayer*) OVERRIDE;

    void updateStyleDeterminedCompositingReasons(RenderLayer*);

    void scheduleAnimationIfNeeded();

    // Whether the layer could ever be composited.
    bool canBeComposited(const RenderLayer*) const;

private:
    class OverlapMap;

    enum CompositingStateTransitionType {
        NoCompositingStateChange,
        AllocateOwnCompositedLayerMapping,
        RemoveOwnCompositedLayerMapping,
        PutInSquashingLayer,
        RemoveFromSquashingLayer
    };

    struct SquashingState {
        SquashingState()
            : mostRecentMapping(0)
            , hasMostRecentMapping(false)
            , nextSquashedLayerIndex(0)
            , totalAreaOfSquashedRects(0) { }

        void updateSquashingStateForNewMapping(CompositedLayerMappingPtr, bool hasNewCompositedLayerMapping, LayoutPoint newOffsetFromAbsoluteForSquashingCLM);

        // The most recent composited backing that the layer should squash onto if needed.
        CompositedLayerMappingPtr mostRecentMapping;
        bool hasMostRecentMapping;

        // Absolute coordinates of the compositedLayerMapping's owning layer. This is used for computing the correct
        // positions of renderlayers when they paint into the squashing layer.
        LayoutPoint offsetFromAbsoluteForSquashingCLM;

        // Counter that tracks what index the next RenderLayer would be if it gets squashed to the current squashing layer.
        size_t nextSquashedLayerIndex;

        // The absolute bounding rect of all the squashed layers.
        IntRect boundingRect;

        // This is simply the sum of the areas of the squashed rects. This can be very skewed if the rects overlap,
        // but should be close enough to drive a heuristic.
        uint64_t totalAreaOfSquashedRects;
    };

    bool hasUnresolvedDirtyBits();

    bool squashingWouldExceedSparsityTolerance(const RenderLayer* candidate, const SquashingState&);
    bool canSquashIntoCurrentSquashingOwner(const RenderLayer* candidate, const SquashingState&);

    CompositingStateTransitionType computeCompositedLayerUpdate(RenderLayer*);
    // Make updates to the layer based on viewport-constrained properties such as position:fixed. This can in turn affect
    // compositing.
    bool updateLayerIfViewportConstrained(RenderLayer*);

    // GraphicsLayerClient implementation
    virtual void notifyAnimationStarted(const GraphicsLayer*, double) OVERRIDE { }
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect&) OVERRIDE;

    virtual bool isTrackingRepaints() const OVERRIDE;

    // Whether the given RL needs to paint into its own separate backing (and hence would need its own CompositedLayerMapping).
    bool needsOwnBacking(const RenderLayer*) const;

    void updateDirectCompositingReasons(RenderLayer*);

    void updateIfNeeded();

    // Make or destroy the CompositedLayerMapping for this layer; returns true if the compositedLayerMapping changed.
    bool allocateOrClearCompositedLayerMapping(RenderLayer*, CompositingStateTransitionType compositedLayerUpdate);
    bool updateSquashingAssignment(RenderLayer*, SquashingState&, CompositingStateTransitionType compositedLayerUpdate);

    void recursiveRepaintLayer(RenderLayer*);

    void computeCompositingRequirements(RenderLayer* ancestorLayer, RenderLayer*, OverlapMap&, struct CompositingRecursionData&, bool& descendantHas3DTransform, Vector<RenderLayer*>& unclippedDescendants, IntRect& absoluteDecendantBoundingBox);

    // Defines which RenderLayers will paint into which composited backings, by allocating and destroying CompositedLayerMappings as needed.
    void assignLayersToBackings(RenderLayer*, bool& layersChanged);
    void assignLayersToBackingsInternal(RenderLayer*, SquashingState&, bool& layersChanged);

    // Hook compositing layers together
    void setCompositingParent(RenderLayer* childLayer, RenderLayer* parentLayer);
    void removeCompositedChildren(RenderLayer*);

    bool hasAnyAdditionalCompositedLayers(const RenderLayer* rootLayer) const;

    void ensureRootLayer();
    void destroyRootLayer();

    void attachRootLayer(RootLayerAttachment);
    void detachRootLayer();

    bool isMainFrame() const;

    void updateOverflowControlsLayers();

    void notifyIFramesOfCompositingChange();

    Page* page() const;

    GraphicsLayerFactory* graphicsLayerFactory() const;
    ScrollingCoordinator* scrollingCoordinator() const;

    void addViewportConstrainedLayer(RenderLayer*);

    bool requiresHorizontalScrollbarLayer() const;
    bool requiresVerticalScrollbarLayer() const;
    bool requiresScrollCornerLayer() const;
#if USE(RUBBER_BANDING)
    bool requiresOverhangLayers() const;
#endif

    void applyUpdateLayerCompositingStateChickenEggHacks(RenderLayer*, CompositingStateTransitionType compositedLayerUpdate);
    void assignLayersToBackingsForReflectionLayer(RenderLayer* reflectionLayer, bool& layersChanged);

private:
    DocumentLifecycle& lifecycle() const;

    RenderView& m_renderView;
    OwnPtr<GraphicsLayer> m_rootContentLayer;
    OwnPtr<GraphicsLayer> m_rootTransformLayer;

    CompositingReasonFinder m_compositingReasonFinder;

    CompositingUpdateType m_pendingUpdateType;

    bool m_hasAcceleratedCompositing;
    bool m_showRepaintCounter;

    bool m_needsToRecomputeCompositingRequirements;

    bool m_compositing;
    bool m_compositingLayersNeedRebuild;
    bool m_forceCompositingMode;
    bool m_needsUpdateCompositingRequirementsState;
    bool m_needsUpdateFixedBackground;

    bool m_isTrackingRepaints; // Used for testing.

    RootLayerAttachment m_rootLayerAttachment;

    // Enclosing container layer, which clips for iframe content
    OwnPtr<GraphicsLayer> m_containerLayer;
    OwnPtr<GraphicsLayer> m_scrollLayer;

    HashSet<RenderLayer*> m_viewportConstrainedLayers;
    HashSet<RenderLayer*> m_viewportConstrainedLayersNeedingUpdate;

    // This is used in updateCompositingRequirementsState to avoid full tree
    // walks while determining if layers have unclipped descendants.
    HashSet<RenderLayer*> m_outOfFlowPositionedLayers;

    // Enclosing layer for overflow controls and the clipping layer
    OwnPtr<GraphicsLayer> m_overflowControlsHostLayer;

    // Layers for overflow controls
    OwnPtr<GraphicsLayer> m_layerForHorizontalScrollbar;
    OwnPtr<GraphicsLayer> m_layerForVerticalScrollbar;
    OwnPtr<GraphicsLayer> m_layerForScrollCorner;
#if USE(RUBBER_BANDING)
    OwnPtr<GraphicsLayer> m_layerForOverhangShadow;
#endif
};

} // namespace WebCore

#endif // RenderLayerCompositor_h
