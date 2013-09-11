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

#include "core/platform/animation/CSSAnimationData.h"
#include "core/platform/animation/KeyframeValueList.h"
#include "core/platform/graphics/Color.h"
#include "core/platform/graphics/FloatPoint.h"
#include "core/platform/graphics/FloatPoint3D.h"
#include "core/platform/graphics/FloatSize.h"
#include "core/platform/graphics/GraphicsLayerClient.h"
#include "core/platform/graphics/IntRect.h"
#include "core/platform/graphics/chromium/OpaqueRectTrackingContentLayerDelegate.h"
#include "core/platform/graphics/filters/FilterOperations.h"
#include "core/platform/graphics/transforms/TransformationMatrix.h"

#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

#include "public/platform/WebAnimationDelegate.h"
#include "public/platform/WebCompositingReasons.h"
#include "public/platform/WebContentLayer.h"
#include "public/platform/WebImageLayer.h"
#include "public/platform/WebLayerClient.h"
#include "public/platform/WebLayerScrollClient.h"
#include "public/platform/WebNinePatchLayer.h"
#include "public/platform/WebSolidColorLayer.h"

namespace WebKit {
class GraphicsLayerFactoryChromium;
class WebLayer;
}

namespace WebCore {

class FloatRect;
class GraphicsContext;
class GraphicsLayerFactory;
class Image;
class ScrollableArea;
class TextStream;

// FIXME: find a better home for this declaration.
class LinkHighlightClient {
public:
    virtual void invalidate() = 0;
    virtual void clearCurrentGraphicsLayer() = 0;
    virtual WebKit::WebLayer* layer() = 0;

protected:
    virtual ~LinkHighlightClient() { }
};

// GraphicsLayer is an abstraction for a rendering surface with backing store,
// which may have associated transformation and animations.

class GraphicsLayer : public GraphicsContextPainter, public WebKit::WebAnimationDelegate, public WebKit::WebLayerScrollClient, public WebKit::WebLayerClient {
    WTF_MAKE_NONCOPYABLE(GraphicsLayer); WTF_MAKE_FAST_ALLOCATED;
public:
    enum ContentsLayerPurpose {
        NoContentsLayer = 0,
        ContentsLayerForImage,
        ContentsLayerForNinePatch,
        ContentsLayerForVideo,
        ContentsLayerForCanvas,
    };

    static PassOwnPtr<GraphicsLayer> create(GraphicsLayerFactory*, GraphicsLayerClient*);

    virtual ~GraphicsLayer();

    GraphicsLayerClient* client() const { return m_client; }

    // WebKit::WebLayerClient implementation.
    virtual WebKit::WebString debugName(WebKit::WebLayer*) OVERRIDE;

    void setCompositingReasons(WebKit::WebCompositingReasons);
    WebKit::WebCompositingReasons compositingReasons() const { return m_compositingReasons; }

    GraphicsLayer* parent() const { return m_parent; };
    void setParent(GraphicsLayer*); // Internal use only.

    // Returns true if the layer has the given layer as an ancestor (excluding self).
    bool hasAncestor(GraphicsLayer*) const;

    const Vector<GraphicsLayer*>& children() const { return m_children; }
    // Returns true if the child list changed.
    bool setChildren(const Vector<GraphicsLayer*>&);

    // Add child layers. If the child is already parented, it will be removed from its old parent.
    void addChild(GraphicsLayer*);
    void addChildAtIndex(GraphicsLayer*, int index);
    void addChildAbove(GraphicsLayer*, GraphicsLayer* sibling);
    void addChildBelow(GraphicsLayer*, GraphicsLayer* sibling);
    bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild);

    void removeAllChildren();
    void removeFromParent();

    GraphicsLayer* maskLayer() const { return m_maskLayer; }
    void setMaskLayer(GraphicsLayer*);

    // The given layer will replicate this layer and its children; the replica renders behind this layer.
    void setReplicatedByLayer(GraphicsLayer*);
    // Whether this layer is being replicated by another layer.
    bool isReplicated() const { return m_replicaLayer; }
    // The layer that replicates this layer (if any).
    GraphicsLayer* replicaLayer() const { return m_replicaLayer; }
    // The layer being replicated.
    GraphicsLayer* replicatedLayer() const { return m_replicatedLayer; }

    const FloatPoint& replicatedLayerPosition() const { return m_replicatedLayerPosition; }
    void setReplicatedLayerPosition(const FloatPoint& p) { m_replicatedLayerPosition = p; }

    enum ShouldSetNeedsDisplay {
        DontSetNeedsDisplay,
        SetNeedsDisplay
    };

    // Offset is origin of the renderer minus origin of the graphics layer (so either zero or negative).
    IntSize offsetFromRenderer() const { return m_offsetFromRenderer; }
    void setOffsetFromRenderer(const IntSize&, ShouldSetNeedsDisplay = SetNeedsDisplay);

    // The position of the layer (the location of its top-left corner in its parent)
    const FloatPoint& position() const { return m_position; }
    void setPosition(const FloatPoint&);

    // Anchor point: (0, 0) is top left, (1, 1) is bottom right. The anchor point
    // affects the origin of the transforms.
    const FloatPoint3D& anchorPoint() const { return m_anchorPoint; }
    void setAnchorPoint(const FloatPoint3D&);

    // The size of the layer.
    const FloatSize& size() const { return m_size; }
    void setSize(const FloatSize&);

    // The boundOrigin affects the offset at which content is rendered, and sublayers are positioned.
    const FloatPoint& boundsOrigin() const { return m_boundsOrigin; }
    void setBoundsOrigin(const FloatPoint& origin) { m_boundsOrigin = origin; }

    const TransformationMatrix& transform() const { return m_transform; }
    void setTransform(const TransformationMatrix&);

    const TransformationMatrix& childrenTransform() const { return m_childrenTransform; }
    void setChildrenTransform(const TransformationMatrix&);

    bool preserves3D() const { return m_preserves3D; }
    void setPreserves3D(bool);

    bool masksToBounds() const { return m_masksToBounds; }
    void setMasksToBounds(bool);

    bool drawsContent() const { return m_drawsContent; }
    void setDrawsContent(bool);

    bool contentsAreVisible() const { return m_contentsVisible; }
    void setContentsVisible(bool);

    // For special cases, e.g. drawing missing tiles on Android.
    // The compositor should never paint this color in normal cases because the RenderLayer
    // will paint background by itself.
    const Color& backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const Color&);

    // opaque means that we know the layer contents have no alpha
    bool contentsOpaque() const { return m_contentsOpaque; }
    void setContentsOpaque(bool);

    bool backfaceVisibility() const { return m_backfaceVisibility; }
    void setBackfaceVisibility(bool visible);

    float opacity() const { return m_opacity; }
    void setOpacity(float);

    const FilterOperations& filters() const { return m_filters; }

    // Returns true if filter can be rendered by the compositor
    bool setFilters(const FilterOperations&);
    void setBackgroundFilters(const FilterOperations&);

    // Some GraphicsLayers paint only the foreground or the background content
    GraphicsLayerPaintingPhase paintingPhase() const { return m_paintingPhase; }
    void setPaintingPhase(GraphicsLayerPaintingPhase phase) { m_paintingPhase = phase; }

    void setNeedsDisplay();
    // mark the given rect (in layer coords) as needing dispay. Never goes deep.
    void setNeedsDisplayInRect(const FloatRect&);

    void setContentsNeedsDisplay();

    // Set that the position/size of the contents (image or video).
    IntRect contentsRect() const { return m_contentsRect; }
    void setContentsRect(const IntRect&);

    // Transitions are identified by a special animation name that cannot clash with a keyframe identifier.
    static String animationNameForTransition(AnimatedPropertyID);

    // Return true if the animation is handled by the compositing system. If this returns
    // false, the animation will be run by AnimationController.
    // These methods handle both transitions and keyframe animations.
    bool addAnimation(const KeyframeValueList&, const IntSize& /*boxSize*/, const CSSAnimationData*, const String& /*animationName*/, double /*timeOffset*/);
    void pauseAnimation(const String& /*animationName*/, double /*timeOffset*/);
    void removeAnimation(const String& /*animationName*/);

    void suspendAnimations(double time);
    void resumeAnimations();

    // Layer contents
    void setContentsToImage(Image*);
    void setContentsToNinePatch(Image*, const IntRect& aperture);
    bool shouldDirectlyCompositeImage(Image*) const { return true; }
    void setContentsToMedia(WebKit::WebLayer*); // video or plug-in
    // Pass an invalid color to remove the contents layer.
    void setContentsToSolidColor(const Color&) { }
    void setContentsToCanvas(WebKit::WebLayer*);
    // FIXME: webkit.org/b/109658
    // Should unify setContentsToMedia and setContentsToCanvas
    void setContentsToPlatformLayer(WebKit::WebLayer* layer) { setContentsToMedia(layer); }
    bool hasContentsLayer() const { return m_contentsLayer; }

    // Callback from the underlying graphics system to draw layer contents.
    void paintGraphicsLayerContents(GraphicsContext&, const IntRect& clip);
    // Callback from the underlying graphics system when the layer has been displayed
    void layerDidDisplay(WebKit::WebLayer*) { }

    // For hosting this GraphicsLayer in a native layer hierarchy.
    WebKit::WebLayer* platformLayer() const;

    enum CompositingCoordinatesOrientation { CompositingCoordinatesTopDown, CompositingCoordinatesBottomUp };

    // Flippedness of the contents of this layer. Does not affect sublayer geometry.
    void setContentsOrientation(CompositingCoordinatesOrientation orientation) { m_contentsOrientation = orientation; }
    CompositingCoordinatesOrientation contentsOrientation() const { return m_contentsOrientation; }

    void dumpLayer(TextStream&, int indent = 0, LayerTreeFlags = LayerTreeNormal) const;

    int paintCount() const { return m_paintCount; }

    // z-position is the z-equivalent of position(). It's only used for debugging purposes.
    float zPosition() const { return m_zPosition; }
    void setZPosition(float);

    void distributeOpacity(float);
    float accumulatedOpacity() const;

    // If the exposed rect of this layer changes, returns true if this or descendant layers need a flush,
    // for example to allocate new tiles.
    bool visibleRectChangeRequiresFlush(const FloatRect& /* clipRect */) const { return false; }

    // Return a string with a human readable form of the layer tree, If debug is true
    // pointers for the layers and timing data will be included in the returned string.
    String layerTreeAsText(LayerTreeFlags = LayerTreeNormal) const;

    // Return an estimate of the backing store memory cost (in bytes). May be incorrect for tiled layers.
    double backingStoreMemoryEstimate() const;

    void resetTrackedRepaints();
    void addRepaintRect(const FloatRect&);

    static bool supportsBackgroundColorContent()
    {
        return false;
    }

    void setLinkHighlight(LinkHighlightClient*);
    // Exposed for tests
    LinkHighlightClient* linkHighlight() { return m_linkHighlight; }

    void setScrollableArea(ScrollableArea*, bool isMainFrame);
    ScrollableArea* scrollableArea() const { return m_scrollableArea; }

    WebKit::WebContentLayer* contentLayer() const { return m_layer.get(); }

    // Exposed for tests. FIXME - name is too similar to contentLayer(), very error prone.
    WebKit::WebLayer* contentsLayer() const { return m_contentsLayer; }

    static void registerContentsLayer(WebKit::WebLayer*);
    static void unregisterContentsLayer(WebKit::WebLayer*);

    // GraphicsContextPainter implementation.
    virtual void paint(GraphicsContext&, const IntRect& clip) OVERRIDE;

    // WebAnimationDelegate implementation.
    virtual void notifyAnimationStarted(double startTime) OVERRIDE;
    virtual void notifyAnimationFinished(double finishTime) OVERRIDE;

    // WebLayerScrollClient implementation.
    virtual void didScroll() OVERRIDE;

protected:
    explicit GraphicsLayer(GraphicsLayerClient*);
    // GraphicsLayerFactoryChromium that wants to create a GraphicsLayer need to be friends.
    friend class WebKit::GraphicsLayerFactoryChromium;

private:
    // Adds a child without calling updateChildList(), so that adding children
    // can be batched before updating.
    void addChildInternal(GraphicsLayer*);

    // This method is used by platform GraphicsLayer classes to clear the filters
    // when compositing is not done in hardware. It is not virtual, so the caller
    // needs to notifiy the change to the platform layer as needed.
    void clearFilters() { m_filters.clear(); }

    // Given a KeyframeValueList containing filterOperations, return true if the operations are valid.
    static int validateFilterOperations(const KeyframeValueList&);

    // Given a list of TransformAnimationValues, see if all the operations for each keyframe match. If so
    // return the index of the KeyframeValueList entry that has that list of operations (it may not be
    // the first entry because some keyframes might have an empty transform and those match any list).
    // If the lists don't match return -1. On return, if hasBigRotation is true, functions contain
    // rotations of >= 180 degrees
    static int validateTransformOperations(const KeyframeValueList&, bool& hasBigRotation);

    void setReplicatedLayer(GraphicsLayer* layer) { m_replicatedLayer = layer; }

    int incrementPaintCount() { return ++m_paintCount; }

    static void writeIndent(TextStream&, int indent);

    void dumpProperties(TextStream&, int indent, LayerTreeFlags) const;
    void dumpAdditionalProperties(TextStream&, int /*indent*/, LayerTreeFlags) const { }

    // Helper functions used by settors to keep layer's the state consistent.
    void updateChildList();
    void updateLayerIsDrawable();
    void updateContentsRect();

    void setContentsTo(ContentsLayerPurpose, WebKit::WebLayer*);
    void setupContentsLayer(WebKit::WebLayer*);
    void clearContentsLayerIfUnregistered();
    WebKit::WebLayer* contentsLayerIfRegistered();

    GraphicsLayerClient* m_client;

    // Offset from the owning renderer
    IntSize m_offsetFromRenderer;

    // Position is relative to the parent GraphicsLayer
    FloatPoint m_position;
    FloatPoint3D m_anchorPoint;
    FloatSize m_size;
    FloatPoint m_boundsOrigin;

    TransformationMatrix m_transform;
    TransformationMatrix m_childrenTransform;

    Color m_backgroundColor;
    float m_opacity;
    float m_zPosition;

    FilterOperations m_filters;

    bool m_contentsOpaque : 1;
    bool m_preserves3D: 1;
    bool m_backfaceVisibility : 1;
    bool m_masksToBounds : 1;
    bool m_drawsContent : 1;
    bool m_contentsVisible : 1;

    GraphicsLayerPaintingPhase m_paintingPhase;
    CompositingCoordinatesOrientation m_contentsOrientation; // affects orientation of layer contents

    Vector<GraphicsLayer*> m_children;
    GraphicsLayer* m_parent;

    GraphicsLayer* m_maskLayer; // Reference to mask layer. We don't own this.

    GraphicsLayer* m_replicaLayer; // A layer that replicates this layer. We only allow one, for now.
                                   // The replica is not parented; this is the primary reference to it.
    GraphicsLayer* m_replicatedLayer; // For a replica layer, a reference to the original layer.
    FloatPoint m_replicatedLayerPosition; // For a replica layer, the position of the replica.

    IntRect m_contentsRect;

    int m_paintCount;

    OwnPtr<WebKit::WebContentLayer> m_layer;
    OwnPtr<WebKit::WebImageLayer> m_imageLayer;
    OwnPtr<WebKit::WebNinePatchLayer> m_ninePatchLayer;
    WebKit::WebLayer* m_contentsLayer;
    // We don't have ownership of m_contentsLayer, but we do want to know if a given layer is the
    // same as our current layer in setContentsTo(). Since m_contentsLayer may be deleted at this point,
    // we stash an ID away when we know m_contentsLayer is alive and use that for comparisons from that point
    // on.
    int m_contentsLayerId;

    LinkHighlightClient* m_linkHighlight;

    OwnPtr<OpaqueRectTrackingContentLayerDelegate> m_opaqueRectTrackingContentLayerDelegate;

    ContentsLayerPurpose m_contentsLayerPurpose;

    typedef HashMap<String, int> AnimationIdMap;
    AnimationIdMap m_animationIdMap;

    ScrollableArea* m_scrollableArea;
    WebKit::WebCompositingReasons m_compositingReasons;
};


} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showGraphicsLayerTree(const WebCore::GraphicsLayer* layer);
#endif

#endif // GraphicsLayer_h
