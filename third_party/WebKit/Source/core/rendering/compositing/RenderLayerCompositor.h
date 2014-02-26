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
#include "core/rendering/compositing/CompositingReasonFinder.h"
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
    CompositingUpdateAfterStyleChange,
    CompositingUpdateAfterLayout,
    CompositingUpdateOnScroll,
    CompositingUpdateOnCompositedScroll,
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
    // FIXME: This constructor should take a reference.
    explicit RenderLayerCompositor(RenderView*);
    virtual ~RenderLayerCompositor();

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

    bool canRender3DTransforms() const;

    // Copy the accelerated compositing related flags from Settings
    void cacheAcceleratedCompositingFlags();

    // Called when the layer hierarchy needs to be updated (compositing layers have been
    // created, destroyed or re-parented).
    void setCompositingLayersNeedRebuild(bool needRebuild = true);
    bool compositingLayersNeedRebuild() const { return m_compositingLayersNeedRebuild; }

    // Updating properties required for determining if compositing is necessary.
    void updateCompositingRequirementsState();
    void setNeedsUpdateCompositingRequirementsState() { m_needsUpdateCompositingRequirementsState = true; }

    // Used to indicate that a compositing update will be needed for the next frame that gets drawn.
    void setNeedsCompositingUpdate(CompositingUpdateType);

    // Main entry point for a full update. As needed, this function will compute compositing requirements,
    // rebuild the composited layer tree, and/or update all the properties assocaited with each layer of the
    // composited layer tree.
    void updateCompositingLayers();

    enum UpdateLayerCompositingStateOptions {
        Normal,
        UseChickenEggHacks // Use this to trigger temporary chicken-egg hacks. See crbug.com/339892.
    };

    // Update the compositing dirty bits, based on the compositing-impacting properties of the layer.
    // (At the moment, it also has some legacy compatibility hacks.)
    void updateLayerCompositingState(RenderLayer*, UpdateLayerCompositingStateOptions = Normal);

    // Update the geometry for compositing children of compositingAncestor.
    void updateCompositingDescendantGeometry(RenderLayerStackingNode* compositingAncestor, RenderLayer*, bool compositedChildrenOnly);

    // Whether layer's compositedLayerMapping needs a GraphicsLayer to do clipping by an ancestor (non-stacking-context parent with overflow).
    bool clippedByAncestor(const RenderLayer*) const;
    // Whether layer's compositedLayerMapping needs a GraphicsLayer to clip z-order children of the given RenderLayer.
    bool clipsCompositingDescendants(const RenderLayer*) const;

    // Whether the given layer needs an extra 'contents' layer.
    bool needsContentsCompositingLayer(const RenderLayer*) const;

    bool supportsFixedRootBackgroundCompositing() const;
    bool needsFixedRootBackgroundLayer(const RenderLayer*) const;
    GraphicsLayer* fixedRootBackgroundLayer() const;

    // Return the bounding box required for compositing layer and its childern, relative to ancestorLayer.
    // If layerBoundingBox is not 0, on return it contains the bounding box of this layer only.
    LayoutRect calculateCompositedBounds(const RenderLayer*, const RenderLayer* ancestorLayer) const;

    // Repaint the appropriate layers when the given RenderLayer starts or stops being composited.
    void repaintOnCompositingChange(RenderLayer*);

    void repaintInCompositedAncestor(RenderLayer*, const LayoutRect&);

    // Notify us that a layer has been added or removed
    void layerWasAdded(RenderLayer* parent, RenderLayer* child);
    void layerWillBeRemoved(RenderLayer* parent, RenderLayer* child);

    // Get the nearest ancestor layer that has overflow or clip, but is not a stacking context
    RenderLayer* enclosingNonStackingClippingLayer(const RenderLayer* layer) const;

    // Repaint parts of all composited layers that intersect the given absolute rectangle (or the entire layer if the pointer is null).
    void repaintCompositedLayers(const IntRect* = 0);

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

    void clearMappingForAllRenderLayers();

    // Walk the tree looking for layers with 3d transforms. Useful in case you need
    // to know if there is non-affine content, e.g. for drawing into an image.
    bool has3DContent() const;

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
            , nextSquashedLayerIndex(0) { }

        void updateSquashingStateForNewMapping(CompositedLayerMappingPtr, bool hasNewCompositedLayerMapping, IntPoint newOffsetFromAbsoluteForSquashingCLM);

        // The most recent composited backing that the layer should squash onto if needed.
        CompositedLayerMappingPtr mostRecentMapping;
        bool hasMostRecentMapping;

        // Absolute coordinates of the compositedLayerMapping's owning layer. This is used for computing the correct
        // positions of renderlayers when they paint into the squashing layer.
        IntPoint offsetFromAbsoluteForSquashingCLM;

        // Counter that tracks what index the next RenderLayer would be if it gets squashed to the current squashing layer.
        size_t nextSquashedLayerIndex;
    };

    CompositingStateTransitionType computeCompositedLayerUpdate(const RenderLayer*);
    // Make updates to the layer based on viewport-constrained properties such as position:fixed. This can in turn affect
    // compositing.
    bool updateLayerIfViewportConstrained(RenderLayer*);

    // GraphicsLayerClient implementation
    virtual void notifyAnimationStarted(const GraphicsLayer*, double, double) OVERRIDE { }
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect&) OVERRIDE;

    virtual bool isTrackingRepaints() const OVERRIDE;

    // Whether the given RL needs to paint into its own separate backing (and hence would need its own CompositedLayerMapping).
    bool needsOwnBacking(const RenderLayer*) const;
    // Whether the layer could ever be composited.
    bool canBeComposited(const RenderLayer*) const;

    void updateDirectCompositingReasons(RenderLayer*);

    // Returns indirect reasons that a layer should be composited because of something in its subtree.
    CompositingReasons subtreeReasonsForCompositing(RenderObject*, bool hasCompositedDescendants, bool has3DTransformedDescendants) const;

    // Make or destroy the CompositedLayerMapping for this layer; returns true if the compositedLayerMapping changed.
    bool allocateOrClearCompositedLayerMapping(RenderLayer*);
    bool updateSquashingAssignment(RenderLayer*, SquashingState&);

    void clearMappingForRenderLayerIncludingDescendants(RenderLayer*);

    // Repaint the given rect (which is layer's coords), and regions of child layers that intersect that rect.
    void recursiveRepaintLayer(RenderLayer*, const IntRect* = 0);

    void addToOverlapMap(OverlapMap&, RenderLayer*, IntRect& layerBounds, bool& boundsComputed);
    void addToOverlapMapRecursive(OverlapMap&, RenderLayer*, RenderLayer* ancestorLayer = 0);

    // Forces an update for all frames of frame tree recursively. Used only when the mainFrame compositor is ready to
    // finish all deferred work.
    static void finishCompositingUpdateForFrameTree(LocalFrame*);

    // Returns true if any layer's compositing changed
    void computeCompositingRequirements(RenderLayer* ancestorLayer, RenderLayer*, OverlapMap*, struct CompositingRecursionData&, bool& descendantHas3DTransform, Vector<RenderLayer*>& unclippedDescendants);

    // Defines which RenderLayers will paint into which composited backings, by allocating and destroying CompositedLayerMappings as needed.
    void assignLayersToBackings(RenderLayer*, bool& layersChanged);
    void assignLayersToBackingsInternal(RenderLayer*, SquashingState&, bool& layersChanged);

    // Allocates, sets up hierarchy, and sets appropriate properties for the GraphicsLayers that correspond to a given
    // composited RenderLayer. Does nothing if the given RenderLayer does not have a CompositedLayerMapping.
    void updateGraphicsLayersMappedToRenderLayer(RenderLayer*);

    // Recurses down the tree, parenting descendant compositing layers and collecting an array of child layers for the current compositing layer.
    void rebuildCompositingLayerTree(RenderLayer*, Vector<GraphicsLayer*>& childGraphicsLayersOfEnclosingLayer, int depth);

    // Recurses down the tree, updating layer geometry only.
    void updateLayerTreeGeometry(RenderLayer*);

    // Hook compositing layers together
    void setCompositingParent(RenderLayer* childLayer, RenderLayer* parentLayer);
    void removeCompositedChildren(RenderLayer*);

    bool layerHas3DContent(const RenderLayer*) const;
    bool isRunningAcceleratedTransformAnimation(RenderObject*) const;

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

    FixedPositionViewportConstraints computeFixedViewportConstraints(RenderLayer*) const;
    StickyPositionViewportConstraints computeStickyViewportConstraints(RenderLayer*) const;

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

    RenderView* m_renderView;
    OwnPtr<GraphicsLayer> m_rootContentLayer;
    OwnPtr<GraphicsLayer> m_rootTransformLayer;

    CompositingReasonFinder m_compositingReasonFinder;

    bool m_hasAcceleratedCompositing;
    bool m_showRepaintCounter;

    // FIXME: This should absolutely not be mutable.
    mutable bool m_needsToRecomputeCompositingRequirements;
    bool m_needsToUpdateLayerTreeGeometry;

    bool m_compositing;
    bool m_compositingLayersNeedRebuild;
    bool m_forceCompositingMode;
    bool m_needsUpdateCompositingRequirementsState;

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
