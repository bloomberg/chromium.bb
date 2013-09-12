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

#ifndef RenderLayerBacking_h
#define RenderLayerBacking_h

#include "core/platform/graphics/FloatPoint.h"
#include "core/platform/graphics/FloatPoint3D.h"
#include "core/platform/graphics/GraphicsLayer.h"
#include "core/platform/graphics/GraphicsLayerClient.h"
#include "core/platform/graphics/transforms/TransformationMatrix.h"
#include "core/rendering/RenderLayer.h"

namespace WebCore {

class KeyframeList;
class RenderLayerCompositor;

enum CompositingLayerType {
    NormalCompositingLayer, // non-tiled layer with backing store
    MediaCompositingLayer, // layer that contains an image, video, webGL or plugin
    ContainerCompositingLayer // layer with no backing store
};


// A GraphicsLayerPaintInfo contains all the info needed to paint a partial subtree of RenderLayers into a GraphicsLayer.
struct GraphicsLayerPaintInfo {
    RenderLayer* renderLayer;

    IntRect compositedBounds;

    IntSize offsetFromRenderer;

    GraphicsLayerPaintingPhase paintingPhase;

    bool isBackgroundLayer;
};

// RenderLayerBacking controls the compositing behavior for a single RenderLayer.
// It holds the various GraphicsLayers, and makes decisions about intra-layer rendering
// optimizations.
//
// There is one RenderLayerBacking for each RenderLayer that is composited.

class RenderLayerBacking : public GraphicsLayerClient {
    WTF_MAKE_NONCOPYABLE(RenderLayerBacking); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RenderLayerBacking(RenderLayer*);
    ~RenderLayerBacking();

    RenderLayer* owningLayer() const { return m_owningLayer; }

    enum UpdateAfterLayoutFlag {
        CompositingChildrenOnly = 1 << 0,
        NeedsFullRepaint = 1 << 1,
        IsUpdateRoot = 1 << 2
    };
    typedef unsigned UpdateAfterLayoutFlags;
    void updateAfterLayout(UpdateAfterLayoutFlags);

    // Returns true if layer configuration changed.
    bool updateGraphicsLayerConfiguration();
    // Update graphics layer position and bounds.
    void updateGraphicsLayerGeometry(); // make private
    // Update whether layer needs blending.
    void updateContentsOpaque();

    GraphicsLayer* graphicsLayer() const { return m_graphicsLayer.get(); }

    // Layer to clip children
    bool hasClippingLayer() const { return m_childContainmentLayer; }
    GraphicsLayer* clippingLayer() const { return m_childContainmentLayer.get(); }

    // Layer to get clipped by ancestor
    bool hasAncestorClippingLayer() const { return m_ancestorClippingLayer; }
    GraphicsLayer* ancestorClippingLayer() const { return m_ancestorClippingLayer.get(); }

    bool hasAncestorScrollClippingLayer() const { return m_ancestorScrollClippingLayer; }
    GraphicsLayer* ancestorScrollClippingLayer() const { return m_ancestorScrollClippingLayer.get(); }

    bool hasContentsLayer() const { return m_foregroundLayer != 0; }
    GraphicsLayer* foregroundLayer() const { return m_foregroundLayer.get(); }

    GraphicsLayer* backgroundLayer() const { return m_backgroundLayer.get(); }
    bool backgroundLayerPaintsFixedRootBackground() const { return m_backgroundLayerPaintsFixedRootBackground; }

    bool hasScrollingLayer() const { return m_scrollingLayer; }
    GraphicsLayer* scrollingLayer() const { return m_scrollingLayer.get(); }
    GraphicsLayer* scrollingContentsLayer() const { return m_scrollingContentsLayer.get(); }

    bool hasMaskLayer() const { return m_maskLayer != 0; }

    GraphicsLayer* parentForSublayers() const;
    GraphicsLayer* childForSuperlayers() const;

    // Returns true for a composited layer that has no backing store of its own, so
    // paints into some ancestor layer.
    bool paintsIntoCompositedAncestor() const { return !m_requiresOwnBackingStore; }

    void setRequiresOwnBackingStore(bool);

    void setContentsNeedDisplay();
    // r is in the coordinate space of the layer's render object
    void setContentsNeedDisplayInRect(const IntRect&);

    // Notification from the renderer that its content changed.
    void contentChanged(ContentChangeType);

    // Interface to start, finish, suspend and resume animations and transitions
    bool startTransition(double, CSSPropertyID, const RenderStyle* fromStyle, const RenderStyle* toStyle);
    void transitionPaused(double timeOffset, CSSPropertyID);
    void transitionFinished(CSSPropertyID);

    bool startAnimation(double timeOffset, const CSSAnimationData* anim, const KeyframeList& keyframes);
    void animationPaused(double timeOffset, const String& name);
    void animationFinished(const String& name);

    void suspendAnimations(double time = 0);
    void resumeAnimations();

    IntRect compositedBounds() const;
    void setCompositedBounds(const IntRect&);
    void updateCompositedBounds();

    void updateAfterWidgetResize();
    void positionOverflowControlsLayers(const IntSize& offsetFromRoot);
    bool hasUnpositionedOverflowControlsLayers() const;

    // GraphicsLayerClient interface
    virtual void notifyAnimationStarted(const GraphicsLayer*, double startTime) OVERRIDE;

    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& clip) OVERRIDE;

    virtual void didCommitChangesForLayer(const GraphicsLayer*) const OVERRIDE;
    virtual bool getCurrentTransform(const GraphicsLayer*, TransformationMatrix&) const OVERRIDE;

    virtual bool isTrackingRepaints() const OVERRIDE;

#ifndef NDEBUG
    virtual void verifyNotPainting();
#endif

    IntRect contentsBox() const;
    IntRect backgroundBox() const;

    // For informative purposes only.
    CompositingLayerType compositingLayerType() const;

    GraphicsLayer* layerForHorizontalScrollbar() const { return m_layerForHorizontalScrollbar.get(); }
    GraphicsLayer* layerForVerticalScrollbar() const { return m_layerForVerticalScrollbar.get(); }
    GraphicsLayer* layerForScrollCorner() const { return m_layerForScrollCorner.get(); }

    void updateFilters(const RenderStyle*);
    bool canCompositeFilters() const { return m_canCompositeFilters; }

    // Return an estimate of the backing store area (in pixels) allocated by this object's GraphicsLayers.
    double backingStoreMemoryEstimate() const;

    void setBlendMode(BlendMode);

    virtual String debugName(const GraphicsLayer*) OVERRIDE;

private:
    void createPrimaryGraphicsLayer();
    void destroyGraphicsLayers();

    PassOwnPtr<GraphicsLayer> createGraphicsLayer(CompositingReasons);

    RenderLayerModelObject* renderer() const { return m_owningLayer->renderer(); }
    RenderLayerCompositor* compositor() const { return m_owningLayer->compositor(); }

    void updateInternalHierarchy();
    bool updateClippingLayers(bool needsAncestorClip, bool needsDescendantClip, bool needsScrollClip);
    bool updateOverflowControlsLayers(bool needsHorizontalScrollbarLayer, bool needsVerticalScrollbarLayer, bool needsScrollCornerLayer);
    bool updateForegroundLayer(bool needsForegroundLayer);
    bool updateBackgroundLayer(bool needsBackgroundLayer);
    bool updateMaskLayer(bool needsMaskLayer);
    bool requiresHorizontalScrollbarLayer() const { return m_owningLayer->horizontalScrollbar(); }
    bool requiresVerticalScrollbarLayer() const { return m_owningLayer->verticalScrollbar(); }
    bool requiresScrollCornerLayer() const { return !m_owningLayer->scrollCornerAndResizerRect().isEmpty(); }
    bool updateScrollingLayers(bool scrollingLayers);
    void updateScrollParent(RenderLayer*);
    void updateClipParent(RenderLayer*);
    void updateDrawsContent(bool isSimpleContainer);
    void registerScrollingLayers();

    void setBackgroundLayerPaintsFixedRootBackground(bool);

    GraphicsLayerPaintingPhase paintingPhaseForPrimaryLayer() const;

    IntSize contentOffsetInCompostingLayer() const;
    // Result is transform origin in pixels.
    FloatPoint3D computeTransformOrigin(const IntRect& borderBox) const;
    // Result is perspective origin in pixels.
    FloatPoint computePerspectiveOrigin(const IntRect& borderBox) const;

    void updateOpacity(const RenderStyle*);
    void updateTransform(const RenderStyle*);
    void updateLayerBlendMode(const RenderStyle*);
    // Return the opacity value that this layer should use for compositing.
    float compositingOpacity(float rendererOpacity) const;

    bool isMainFrameRenderViewLayer() const;

    bool paintsBoxDecorations() const;
    bool paintsChildren() const;

    // Returns true if this compositing layer has no visible content.
    bool isSimpleContainerCompositingLayer() const;
    // Returns true if this layer has content that needs to be rendered by painting into the backing store.
    bool containsPaintedContent(bool isSimpleContainer) const;
    // Returns true if the RenderLayer just contains an image that we can composite directly.
    bool isDirectlyCompositedImage() const;
    void updateImageContents();

    Color rendererBackgroundColor() const;
    void updateBackgroundColor(bool isSimpleContainer);
    void updateContentsRect(bool isSimpleContainer);

    void updateCompositingReasons();

    bool hasVisibleNonCompositingDescendantLayers() const;

    bool shouldClipCompositedBounds() const;

    void doPaintTask(GraphicsLayerPaintInfo&, GraphicsContext*, const IntRect& clip);

    static CSSPropertyID graphicsLayerToCSSProperty(AnimatedPropertyID);
    static AnimatedPropertyID cssToGraphicsLayerProperty(CSSPropertyID);

    RenderLayer* m_owningLayer;

    // The hierarchy of layers that is maintained by the RenderLayerBacking looks like this:
    //
    // m_ancestorScrollClippingLayer [OPTIONAL]
    //  + m_ancestorClippingLayer [OPTIONAL]
    //     + m_graphicsLayer
    //        + m_childContainmentLayer [OPTIONAL] <-OR-> m_scrollingLayer [OPTIONAL]
    //                                                     + m_scrollingContentsLayer [OPTIONAL]
    //
    // We need an ancestor scroll clipping layer if we have a "scroll parent". That is, if
    // our scrolling ancestor is not our ancestor in the stacking tree. Similarly, we need
    // an ancestor clipping layer if our clipping ancestor is not our ancestor in the clipping
    // tree. Here's what that might look like.
    //
    // Let A = the scrolling ancestor,
    //     B = the clipping ancestor,
    //     C = the scroll/clip descendant, and
    //     SC = the stacking context that is the ancestor of A, B and C in the stacking tree.
    //
    // SC
    //  + A = m_graphicsLayer
    //  |      + m_scrollingLayer [*]
    //  |         + m_scrollingContentsLayer [+]
    //  |            + ...
    //  ...
    //  |
    //  + B = m_graphicsLayer
    //  |      + m_childContainmentLayer
    //  |         + ...
    //  ...
    //  |
    //  + C = m_ancestorScrollClippingLayer [**]
    //         + m_ancestorClippingLayer [++]
    //            + m_graphicsLayer
    //               + ...
    //
    // Note that [*] and [**] exist for the same reason: to clip scrolling content. That is,
    // when we scroll A, in fact [+] and [++] are moved and are clipped to [*] and [**],
    // respectively.
    //
    // Now, it may also be the case that C is also clipped by another layer that doesn't happen
    // to be its scrolling ancestor. B, in this case. When this happens, we create an
    // ancestor clipping layer for C, [++]. Unlike the scroll clipping layer, [**], which must
    // stay put during a scroll to do its job, the ancestor clipping layer [++] does move with
    // the content it clips.
    OwnPtr<GraphicsLayer> m_ancestorScrollClippingLayer; // Used if and only if we have a scroll parent.
    OwnPtr<GraphicsLayer> m_ancestorClippingLayer; // Only used if we are clipped by an ancestor which is not a stacking context.
    OwnPtr<GraphicsLayer> m_graphicsLayer;
    OwnPtr<GraphicsLayer> m_childContainmentLayer; // Only used if we have clipping on a stacking context with compositing children.
    OwnPtr<GraphicsLayer> m_scrollingLayer; // Only used if the layer is using composited scrolling.
    OwnPtr<GraphicsLayer> m_scrollingContentsLayer; // Only used if the layer is using composited scrolling.

    // This layer is also added to the hierarchy by the RLB, but in a different way than
    // the layers above. It's added to m_graphicsLayer as its mask layer (naturally) if
    // we have a mask, and isn't part of the typical hierarchy (it has no children).
    OwnPtr<GraphicsLayer> m_maskLayer; // Only used if we have a mask.

    // There are two other (optional) layers whose painting is managed by the RenderLayerBacking,
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

    IntRect m_compositedBounds;

    bool m_artificiallyInflatedBounds; // bounds had to be made non-zero to make transform-origin work
    bool m_boundsConstrainedByClipping;
    bool m_isMainFrameRenderViewLayer;
    bool m_requiresOwnBackingStore;
    bool m_canCompositeFilters;
    bool m_backgroundLayerPaintsFixedRootBackground;
};

} // namespace WebCore

#endif // RenderLayerBacking_h
