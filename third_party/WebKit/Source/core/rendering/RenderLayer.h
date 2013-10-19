/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#ifndef RenderLayer_h
#define RenderLayer_h

#include "core/rendering/CompositingReasons.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderLayerClipper.h"
#include "core/rendering/RenderLayerRepainter.h"
#include "core/rendering/RenderLayerScrollableArea.h"
#include "core/rendering/RenderLayerStackingNode.h"

#include "wtf/OwnPtr.h"

#include "core/rendering/RenderLayerFilterInfo.h"

namespace WebCore {

class FilterEffectRenderer;
class FilterOperations;
class HitTestRequest;
class HitTestResult;
class HitTestingTransformState;
class PlatformEvent;
class RenderFlowThread;
class RenderGeometryMap;
class CompositedLayerMapping;
class RenderLayerCompositor;
class RenderReplica;
class RenderScrollbarPart;
class RenderStyle;
class RenderView;
class Scrollbar;
class TransformationMatrix;

enum BorderRadiusClippingRule { IncludeSelfForBorderRadius, DoNotIncludeSelfForBorderRadius };

class RenderLayer {
public:
    friend class RenderReplica;
    // FIXME: Needed until we move all the necessary bits to the new class.
    friend class RenderLayerStackingNode;
    // FIXME: Needed until we move all the necessary bits to the new class.
    friend class RenderLayerScrollableArea;

    RenderLayer(RenderLayerModelObject*);
    ~RenderLayer();

    String debugName() const;

    RenderLayerModelObject* renderer() const { return m_renderer; }
    RenderBox* renderBox() const { return m_renderer && m_renderer->isBox() ? toRenderBox(m_renderer) : 0; }
    RenderLayer* parent() const { return m_parent; }
    RenderLayer* previousSibling() const { return m_previous; }
    RenderLayer* nextSibling() const { return m_next; }
    RenderLayer* firstChild() const { return m_first; }
    RenderLayer* lastChild() const { return m_last; }

    void addChild(RenderLayer* newChild, RenderLayer* beforeChild = 0);
    RenderLayer* removeChild(RenderLayer*);

    void removeOnlyThisLayer();
    void insertOnlyThisLayer();

    void styleChanged(StyleDifference, const RenderStyle* oldStyle);

    bool isSelfPaintingLayer() const { return m_isSelfPaintingLayer; }

    bool cannotBlitToWindow() const;

    bool isTransparent() const;
    RenderLayer* transparentPaintingAncestor();
    void beginTransparencyLayers(GraphicsContext*, const RenderLayer* rootLayer, const LayoutRect& paintDirtyRect, PaintBehavior);

    bool hasReflection() const { return renderer()->hasReflection(); }
    bool isReflection() const { return renderer()->isReplica(); }
    RenderReplica* reflection() const { return m_reflection; }
    RenderLayer* reflectionLayer() const;

    const RenderLayer* root() const
    {
        const RenderLayer* curr = this;
        while (curr->parent())
            curr = curr->parent();
        return curr;
    }

    const LayoutPoint& location() const { return m_topLeft; }
    void setLocation(const LayoutPoint& p) { m_topLeft = p; }

    const IntSize& size() const { return m_layerSize; }
    void setSize(const IntSize& size) { m_layerSize = size; }

    LayoutRect rect() const { return LayoutRect(location(), size()); }

    // Scrolling methods for layers that can scroll their overflow.
    void scrollRectToVisible(const LayoutRect&, const ScrollAlignment& alignX, const ScrollAlignment& alignY);

    LayoutRect getRectToExpose(const LayoutRect& visibleRect, const LayoutRect& exposeRect, const ScrollAlignment& alignX, const ScrollAlignment& alignY);

    // isPointInResizeControl() is used for testing if a pointer/touch position is in the resize control
    // area.
    bool isPointInResizeControl(const IntPoint& absolutePoint, ResizerHitTestType resizerHitTestType) const;
    bool hitTestOverflowControls(HitTestResult&, const IntPoint& localPoint);
    IntSize offsetFromResizeCorner(const IntPoint& absolutePoint) const;

    void paintOverflowControls(GraphicsContext*, const IntPoint&, const IntRect& damageRect, bool paintingOverlayControls = false);
    void paintScrollCorner(GraphicsContext*, const IntPoint&, const IntRect& damageRect);
    void paintResizer(GraphicsContext*, const IntPoint&, const IntRect& damageRect);

    void updateScrollInfoAfterLayout();

    bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1);
    void autoscroll(const IntPoint&);

    void resize(const PlatformEvent&, const LayoutSize&);
    bool inResizeMode() const;
    void setInResizeMode(bool);

    bool isRootLayer() const { return m_isRootLayer; }

    RenderLayerCompositor* compositor() const;

    // Notification from the renderer that its content changed (e.g. current frame of image changed).
    // Allows updates of layer content without repainting.
    void contentChanged(ContentChangeType);

    bool canRender3DTransforms() const;

    enum UpdateLayerPositionsFlag {
        CheckForRepaint = 1 << 0,
        NeedsFullRepaintInBacking = 1 << 1,
        IsCompositingUpdateRoot = 1 << 2,
        UpdateCompositingLayers = 1 << 3,
        UpdatePagination = 1 << 4
    };
    typedef unsigned UpdateLayerPositionsFlags;
    static const UpdateLayerPositionsFlags defaultFlags = CheckForRepaint | IsCompositingUpdateRoot | UpdateCompositingLayers;

    void updateLayerPositionsAfterLayout(const RenderLayer* rootLayer, UpdateLayerPositionsFlags);

    void updateLayerPositionsAfterOverflowScroll();
    void updateLayerPositionsAfterDocumentScroll();


    bool isPaginated() const { return m_isPaginated; }
    RenderLayer* enclosingPaginationLayer() const { return m_enclosingPaginationLayer; }

    void updateTransform();

    void updateBlendMode();

    const LayoutSize& offsetForInFlowPosition() const { return m_offsetForInFlowPosition; }

    void addBlockSelectionGapsBounds(const LayoutRect&);
    void clearBlockSelectionGapsBounds();
    void repaintBlockSelectionGaps();

    RenderLayerStackingNode* stackingNode() { return m_stackingNode.get(); }
    const RenderLayerStackingNode* stackingNode() const { return m_stackingNode.get(); }

    RenderLayer* ancestorStackingContainerLayer() const;
    RenderLayerStackingNode* ancestorStackingContainerNode() const;
    RenderLayerStackingNode* ancestorStackingNode() const;

    // Gets the enclosing stacking container for this layer, possibly the layer
    // itself, if it is a stacking container.
    RenderLayer* enclosingStackingContainerLayer() { return m_stackingNode->isStackingContainer() ? this : ancestorStackingContainerLayer(); }

    bool subtreeIsInvisible() const { return !hasVisibleContent() && !hasVisibleDescendant(); }

    // FIXME: We should ASSERT(!m_visibleContentStatusDirty) here, but see https://bugs.webkit.org/show_bug.cgi?id=71044
    // ditto for hasVisibleDescendant(), see https://bugs.webkit.org/show_bug.cgi?id=71277
    bool hasVisibleContent() const { return m_hasVisibleContent; }
    bool hasVisibleDescendant() const { return m_hasVisibleDescendant; }

    void setHasVisibleContent();
    void dirtyVisibleContentStatus();

    bool hasBoxDecorationsOrBackground() const;
    bool hasVisibleBoxDecorations() const;
    // Returns true if this layer has visible content (ignoring any child layers).
    bool isVisuallyNonEmpty() const;
    // True if this layer container renderers that paint.
    bool hasNonEmptyChildRenderers() const;

    // FIXME: We should ASSERT(!m_hasSelfPaintingLayerDescendantDirty); here but we hit the same bugs as visible content above.
    // Part of the issue is with subtree relayout: we don't check if our ancestors have some descendant flags dirty, missing some updates.
    bool hasSelfPaintingLayerDescendant() const { return m_hasSelfPaintingLayerDescendant; }

    // FIXME: We should ASSERT(!m_hasOutOfFlowPositionedDescendantDirty) here. See above.
    bool hasOutOfFlowPositionedDescendant() const { return m_hasOutOfFlowPositionedDescendant; }

    void setHasOutOfFlowPositionedDescendant(bool hasDescendant) { m_hasOutOfFlowPositionedDescendant = hasDescendant; }
    void setHasOutOfFlowPositionedDescendantDirty(bool dirty) { m_hasOutOfFlowPositionedDescendantDirty = dirty; }

    bool childLayerHasBlendMode() const { ASSERT(!m_childLayerHasBlendModeStatusDirty); return m_childLayerHasBlendMode; }

    bool hasUnclippedDescendant() const { return m_hasUnclippedDescendant; }
    void setHasUnclippedDescendant(bool hasDescendant) { m_hasUnclippedDescendant = hasDescendant; }
    void updateHasUnclippedDescendant();
    bool isUnclippedDescendant() const { return m_isUnclippedDescendant; }

    // Gets the nearest enclosing positioned ancestor layer (also includes
    // the <html> layer and the root layer).
    RenderLayer* enclosingPositionedAncestor() const;

    // Returns the nearest enclosing layer that is scrollable.
    RenderLayer* enclosingScrollableLayer() const;

    // The layer relative to which clipping rects for this layer are computed.
    RenderLayer* clippingRootForPainting() const;

    // Enclosing compositing layer; if includeSelf is true, may return this.
    RenderLayer* enclosingCompositingLayer(bool includeSelf = true) const;
    RenderLayer* enclosingCompositingLayerForRepaint(bool includeSelf = true) const;
    // Ancestor compositing layer, excluding this.
    RenderLayer* ancestorCompositingLayer() const { return enclosingCompositingLayer(false); }

    // Ancestor scrolling layer at or above our containing block.
    RenderLayer* ancestorScrollingLayer() const;

    RenderLayer* enclosingFilterLayer(bool includeSelf = true) const;
    RenderLayer* enclosingFilterRepaintLayer() const;
    bool hasAncestorWithFilterOutsets() const;

    bool canUseConvertToLayerCoords() const
    {
        // These RenderObjects have an impact on their layers without the renderers knowing about it.
        return !renderer()->hasColumns() && !renderer()->hasTransform() && !renderer()->isSVGRoot();
    }

    void convertToPixelSnappedLayerCoords(const RenderLayer* ancestorLayer, IntPoint& location) const;
    void convertToPixelSnappedLayerCoords(const RenderLayer* ancestorLayer, IntRect&) const;
    void convertToLayerCoords(const RenderLayer* ancestorLayer, LayoutPoint& location) const;
    void convertToLayerCoords(const RenderLayer* ancestorLayer, LayoutRect&) const;

    int zIndex() const { return renderer()->style()->zIndex(); }

    enum PaintLayerFlag {
        PaintLayerHaveTransparency = 1,
        PaintLayerAppliedTransform = 1 << 1,
        PaintLayerTemporaryClipRects = 1 << 2,
        PaintLayerPaintingReflection = 1 << 3,
        PaintLayerPaintingOverlayScrollbars = 1 << 4,
        PaintLayerPaintingCompositingBackgroundPhase = 1 << 5,
        PaintLayerPaintingCompositingForegroundPhase = 1 << 6,
        PaintLayerPaintingCompositingMaskPhase = 1 << 7,
        PaintLayerPaintingCompositingScrollingPhase = 1 << 8,
        PaintLayerPaintingOverflowContents = 1 << 9,
        PaintLayerPaintingRootBackgroundOnly = 1 << 10,
        PaintLayerPaintingSkipRootBackground = 1 << 11,
        PaintLayerPaintingChildClippingMaskPhase = 1 << 12,
        PaintLayerPaintingCompositingAllPhases = (PaintLayerPaintingCompositingBackgroundPhase | PaintLayerPaintingCompositingForegroundPhase | PaintLayerPaintingCompositingMaskPhase)
    };

    typedef unsigned PaintLayerFlags;

    // The two main functions that use the layer system.  The paint method
    // paints the layers that intersect the damage rect from back to
    // front.  The hitTest method looks for mouse events by walking
    // layers that intersect the point from front to back.
    void paint(GraphicsContext*, const LayoutRect& damageRect, PaintBehavior = PaintBehaviorNormal, RenderObject* paintingRoot = 0,
        RenderRegion* = 0, PaintLayerFlags = 0);
    bool hitTest(const HitTestRequest&, HitTestResult&);
    bool hitTest(const HitTestRequest&, const HitTestLocation&, HitTestResult&);
    void paintOverlayScrollbars(GraphicsContext*, const LayoutRect& damageRect, PaintBehavior, RenderObject* paintingRoot = 0);

    // This method figures out our layerBounds in coordinates relative to
    // |rootLayer}.  It also computes our background and foreground clip rects
    // for painting/event handling.
    // Pass offsetFromRoot if known.
    void calculateRects(const ClipRectsContext&, const LayoutRect& paintDirtyRect, LayoutRect& layerBounds,
        ClipRect& backgroundRect, ClipRect& foregroundRect, ClipRect& outlineRect, const LayoutPoint* offsetFromRoot = 0) const;

    LayoutRect childrenClipRect() const; // Returns the foreground clip rect of the layer in the document's coordinate space.
    LayoutRect selfClipRect() const; // Returns the background clip rect of the layer in the document's coordinate space.
    LayoutRect localClipRect() const; // Returns the background clip rect of the layer in the local coordinate space.

    // Pass offsetFromRoot if known.
    bool intersectsDamageRect(const LayoutRect& layerBounds, const LayoutRect& damageRect, const RenderLayer* rootLayer, const LayoutPoint* offsetFromRoot = 0) const;

    enum CalculateLayerBoundsFlag {
        IncludeSelfTransform = 1 << 0,
        UseLocalClipRectIfPossible = 1 << 1,
        IncludeLayerFilterOutsets = 1 << 2,
        ExcludeHiddenDescendants = 1 << 3,
        DontConstrainForMask = 1 << 4,
        IncludeCompositedDescendants = 1 << 5,
        UseFragmentBoxes = 1 << 6,
        DefaultCalculateLayerBoundsFlags =  IncludeSelfTransform | UseLocalClipRectIfPossible | IncludeLayerFilterOutsets | UseFragmentBoxes
    };
    typedef unsigned CalculateLayerBoundsFlags;

    // Bounding box relative to some ancestor layer. Pass offsetFromRoot if known.
    LayoutRect boundingBox(const RenderLayer* rootLayer, CalculateLayerBoundsFlags = 0, const LayoutPoint* offsetFromRoot = 0) const;
    // Bounding box in the coordinates of this layer.
    LayoutRect localBoundingBox(CalculateLayerBoundsFlags = 0) const;
    // Pixel snapped bounding box relative to the root.
    IntRect absoluteBoundingBox() const;

    // Bounds used for layer overlap testing in RenderLayerCompositor.
    LayoutRect overlapBounds() const { return overlapBoundsIncludeChildren() ? calculateLayerBounds(this) : localBoundingBox(); }

    // If true, this layer's children are included in its bounds for overlap testing.
    // We can't rely on the children's positions if this layer has a filter that could have moved the children's pixels around.
    bool overlapBoundsIncludeChildren() const { return hasFilter() && renderer()->style()->filter().hasFilterThatMovesPixels(); }

    // Can pass offsetFromRoot if known.
    IntRect calculateLayerBounds(const RenderLayer* ancestorLayer, const LayoutPoint* offsetFromRoot = 0, CalculateLayerBoundsFlags = DefaultCalculateLayerBoundsFlags) const;

    // WARNING: This method returns the offset for the parent as this is what updateLayerPositions expects.
    LayoutPoint computeOffsetFromRoot(bool& hasLayerOffset) const;

    LayoutUnit staticInlinePosition() const { return m_staticInlinePosition; }
    LayoutUnit staticBlockPosition() const { return m_staticBlockPosition; }

    void setStaticInlinePosition(LayoutUnit position) { m_staticInlinePosition = position; }
    void setStaticBlockPosition(LayoutUnit position) { m_staticBlockPosition = position; }

    bool hasTransform() const { return renderer()->hasTransform(); }
    // Note that this transform has the transform-origin baked in.
    TransformationMatrix* transform() const { return m_transform.get(); }
    // currentTransform computes a transform which takes accelerated animations into account. The
    // resulting transform has transform-origin baked in. If the layer does not have a transform,
    // returns the identity matrix.
    TransformationMatrix currentTransform(RenderStyle::ApplyTransformOrigin = RenderStyle::IncludeTransformOrigin) const;
    TransformationMatrix renderableTransform(PaintBehavior) const;

    // Get the perspective transform, which is applied to transformed sublayers.
    // Returns true if the layer has a -webkit-perspective.
    // Note that this transform has the perspective-origin baked in.
    TransformationMatrix perspectiveTransform() const;
    FloatPoint perspectiveOrigin() const;
    bool preserves3D() const { return renderer()->style()->transformStyle3D() == TransformStyle3DPreserve3D; }
    bool has3DTransform() const { return m_transform && !m_transform->isAffine(); }

    void filterNeedsRepaint();
    bool hasFilter() const { return renderer()->hasFilter(); }

    bool hasBlendMode() const;

    void* operator new(size_t);
    // Only safe to call from RenderLayerModelObject::destroyLayer()
    void operator delete(void*);

    CompositingState compositingState() const;

    // NOTE: If you are accessing the CompositedLayerMapping as a boolean condition to determine the state of compositing for this layer,
    // then you may have incorrect logic. Use compositingState() instead.
    CompositedLayerMapping* compositedLayerMapping() const { return m_compositedLayerMapping.get(); }
    CompositedLayerMapping* ensureCompositedLayerMapping();
    void clearCompositedLayerMapping(bool layerBeingDestroyed = false);
    bool adjustForForceCompositedScrollingMode(bool) const;

    bool hasCompositedMask() const;
    bool hasCompositedClippingMask() const;
    bool needsCompositedScrolling() const;

    RenderLayer* scrollParent() const;
    RenderLayer* clipParent() const;

    bool needsCompositingLayersRebuiltForClip(const RenderStyle* oldStyle, const RenderStyle* newStyle) const;
    bool needsCompositingLayersRebuiltForOverflow(const RenderStyle* oldStyle, const RenderStyle* newStyle) const;
    bool needsCompositingLayersRebuiltForFilters(const RenderStyle* oldStyle, const RenderStyle* newStyle, bool didPaintWithFilters) const;

    bool paintsWithTransparency(PaintBehavior paintBehavior) const
    {
        return isTransparent() && ((paintBehavior & PaintBehaviorFlattenCompositingLayers) || compositingState() != PaintsIntoOwnBacking);
    }

    bool paintsWithTransform(PaintBehavior) const;

    // Returns true if background phase is painted opaque in the given rect.
    // The query rect is given in local coordinates.
    bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const;

    bool containsDirtyOverlayScrollbars() const { return m_containsDirtyOverlayScrollbars; }
    void setContainsDirtyOverlayScrollbars(bool dirtyScrollbars) { m_containsDirtyOverlayScrollbars = dirtyScrollbars; }

    bool isCSSCustomFilterEnabled() const;

    FilterOperations computeFilterOperations(const RenderStyle*);
    bool paintsWithFilters() const;
    bool requiresFullLayerImageForFilters() const;
    FilterEffectRenderer* filterRenderer() const
    {
        RenderLayerFilterInfo* filterInfo = this->filterInfo();
        return filterInfo ? filterInfo->renderer() : 0;
    }

    RenderLayerFilterInfo* filterInfo() const { return hasFilterInfo() ? RenderLayerFilterInfo::filterInfoForRenderLayer(this) : 0; }
    RenderLayerFilterInfo* ensureFilterInfo() { return RenderLayerFilterInfo::createFilterInfoForRenderLayerIfNeeded(this); }
    void removeFilterInfoIfNeeded()
    {
        if (hasFilterInfo())
            RenderLayerFilterInfo::removeFilterInfoForRenderLayer(this);
    }

    bool hasFilterInfo() const { return m_hasFilterInfo; }
    void setHasFilterInfo(bool hasFilterInfo) { m_hasFilterInfo = hasFilterInfo; }

    void updateFilters(const RenderStyle* oldStyle, const RenderStyle* newStyle);

    Node* enclosingElement() const;

    bool isInTopLayer() const;
    bool isInTopLayerSubtree() const;

    enum ViewportConstrainedNotCompositedReason {
        NoNotCompositedReason,
        NotCompositedForBoundsOutOfView,
        NotCompositedForNonViewContainer,
        NotCompositedForNoVisibleContent,
        NotCompositedForUnscrollableAncestors,
    };

    void setViewportConstrainedNotCompositedReason(ViewportConstrainedNotCompositedReason reason) { m_compositingProperties.viewportConstrainedNotCompositedReason = reason; }
    ViewportConstrainedNotCompositedReason viewportConstrainedNotCompositedReason() const { return static_cast<ViewportConstrainedNotCompositedReason>(m_compositingProperties.viewportConstrainedNotCompositedReason); }

    bool isOutOfFlowRenderFlowThread() const { return renderer()->isOutOfFlowRenderFlowThread(); }

    bool scrollsWithRespectTo(const RenderLayer*) const;

    enum ForceNeedsCompositedScrollingMode {
        DoNotForceCompositedScrolling = 0,
        CompositedScrollingAlwaysOn = 1,
        CompositedScrollingAlwaysOff = 2
    };

    void setForceNeedsCompositedScrolling(ForceNeedsCompositedScrollingMode);

    void addLayerHitTestRects(LayerHitTestRects&) const;

    // FIXME: This should probably return a ScrollableArea but a lot of internal methods are mistakenly exposed.
    RenderLayerScrollableArea* scrollableArea() const { return m_scrollableArea.get(); }
    RenderLayerRepainter& repainter() { return m_repainter; }
    RenderLayerClipper& clipper() { return m_clipper; }

    inline bool isPositionedContainer() const
    {
        // FIXME: This is not in sync with containingBlock.
        // RenderObject::canContainFixedPositionedObject() should probably be used
        // instead.
        RenderLayerModelObject* layerRenderer = renderer();
        return isRootLayer() || layerRenderer->isPositioned() || hasTransform();
    }

private:
    bool hasOverflowControls() const;

    void setIsUnclippedDescendant(bool isUnclippedDescendant) { m_isUnclippedDescendant = isUnclippedDescendant; }

    void setAncestorChainHasSelfPaintingLayerDescendant();
    void dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

    void setAncestorChainHasOutOfFlowPositionedDescendant();
    void dirtyAncestorChainHasOutOfFlowPositionedDescendantStatus();

    bool acceleratedCompositingForOverflowScrollEnabled() const;
    // FIXME: This is a temporary flag and should be removed once accelerated
    // overflow scroll is ready (crbug.com/254111).
    bool compositorDrivenAcceleratedScrollingEnabled() const;

    void clipToRect(RenderLayer* rootLayer, GraphicsContext*, const LayoutRect& paintDirtyRect, const ClipRect&,
                    BorderRadiusClippingRule = IncludeSelfForBorderRadius);
    void restoreClip(GraphicsContext*, const LayoutRect& paintDirtyRect, const ClipRect&);

    void updateSelfPaintingLayer();
    void updateVisibilityAfterStyleChange(const RenderStyle* oldStyle);

    void updateOutOfFlowPositioned(const RenderStyle* oldStyle);

    bool setNeedsCompositedScrolling(bool);
    void didUpdateNeedsCompositedScrolling();

    // Returns true if the position changed.
    bool updateLayerPosition();

    void updateLayerPositions(RenderGeometryMap* = 0, UpdateLayerPositionsFlags = defaultFlags);

    enum UpdateLayerPositionsAfterScrollFlag {
        NoFlag = 0,
        IsOverflowScroll = 1 << 0,
        HasSeenViewportConstrainedAncestor = 1 << 1,
        HasSeenAncestorWithOverflowClip = 1 << 2,
        HasChangedAncestor = 1 << 3
    };
    typedef unsigned UpdateLayerPositionsAfterScrollFlags;
    void updateLayerPositionsAfterScroll(RenderGeometryMap*, UpdateLayerPositionsAfterScrollFlags = NoFlag);

    void setNextSibling(RenderLayer* next) { m_next = next; }
    void setPreviousSibling(RenderLayer* prev) { m_previous = prev; }
    void setParent(RenderLayer* parent);
    void setFirstChild(RenderLayer* first) { m_first = first; }
    void setLastChild(RenderLayer* last) { m_last = last; }

    LayoutPoint renderBoxLocation() const { return renderer()->isBox() ? toRenderBox(renderer())->location() : LayoutPoint(); }

    struct LayerPaintingInfo {
        LayerPaintingInfo(RenderLayer* inRootLayer, const LayoutRect& inDirtyRect, PaintBehavior inPaintBehavior, const LayoutSize& inSubPixelAccumulation, RenderObject* inPaintingRoot = 0, RenderRegion*inRegion = 0, OverlapTestRequestMap* inOverlapTestRequests = 0)
            : rootLayer(inRootLayer)
            , paintingRoot(inPaintingRoot)
            , paintDirtyRect(inDirtyRect)
            , subPixelAccumulation(inSubPixelAccumulation)
            , region(inRegion)
            , overlapTestRequests(inOverlapTestRequests)
            , paintBehavior(inPaintBehavior)
            , clipToDirtyRect(true)
        { }
        RenderLayer* rootLayer;
        RenderObject* paintingRoot; // only paint descendants of this object
        LayoutRect paintDirtyRect; // relative to rootLayer;
        LayoutSize subPixelAccumulation;
        RenderRegion* region; // May be null.
        OverlapTestRequestMap* overlapTestRequests; // May be null.
        PaintBehavior paintBehavior;
        bool clipToDirtyRect;
    };

    void paintLayer(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintLayerContentsAndReflection(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintLayerByApplyingTransform(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags, const LayoutPoint& translationOffset = LayoutPoint());
    void paintLayerContents(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintList(Vector<RenderLayer*>*, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintPaginatedChildLayer(RenderLayer* childLayer, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintChildLayerIntoColumns(RenderLayer* childLayer, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags, const Vector<RenderLayer*>& columnLayers, size_t columnIndex);

    void collectFragments(LayerFragments&, const RenderLayer* rootLayer, RenderRegion*, const LayoutRect& dirtyRect,
        ClipRectsType, OverlayScrollbarSizeRelevancy inOverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize,
        ShouldRespectOverflowClip = RespectOverflowClip, const LayoutPoint* offsetFromRoot = 0, const LayoutRect* layerBoundingBox = 0);
    void updatePaintingInfoForFragments(LayerFragments&, const LayerPaintingInfo&, PaintLayerFlags, bool shouldPaintContent, const LayoutPoint* offsetFromRoot);
    void paintBackgroundForFragments(const LayerFragments&, GraphicsContext*, GraphicsContext* transparencyLayerContext,
        const LayoutRect& transparencyPaintDirtyRect, bool haveTransparency, const LayerPaintingInfo&, PaintBehavior, RenderObject* paintingRootForRenderer);
    void paintForegroundForFragments(const LayerFragments&, GraphicsContext*, GraphicsContext* transparencyLayerContext,
        const LayoutRect& transparencyPaintDirtyRect, bool haveTransparency, const LayerPaintingInfo&, PaintBehavior, RenderObject* paintingRootForRenderer,
        bool selectionOnly, bool forceBlackText);
    void paintForegroundForFragmentsWithPhase(PaintPhase, const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, PaintBehavior, RenderObject* paintingRootForRenderer);
    void paintOutlineForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, PaintBehavior, RenderObject* paintingRootForRenderer);
    void paintOverflowControlsForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&);
    void paintMaskForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, RenderObject* paintingRootForRenderer);
    void paintChildClippingMaskForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, RenderObject* paintingRootForRenderer);
    void paintTransformedLayerIntoFragments(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);

    RenderLayer* hitTestLayer(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
                              const LayoutRect& hitTestRect, const HitTestLocation&, bool appliedTransform,
                              const HitTestingTransformState* transformState = 0, double* zOffset = 0);
    RenderLayer* hitTestLayerByApplyingTransform(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest&, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState* = 0, double* zOffset = 0,
        const LayoutPoint& translationOffset = LayoutPoint());
    RenderLayer* hitTestList(Vector<RenderLayer*>*, RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result,
                             const LayoutRect& hitTestRect, const HitTestLocation&,
                             const HitTestingTransformState* transformState, double* zOffsetForDescendants, double* zOffset,
                             const HitTestingTransformState* unflattenedTransformState, bool depthSortDescendants);
    RenderLayer* hitTestPaginatedChildLayer(RenderLayer* childLayer, RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result,
                                            const LayoutRect& hitTestRect, const HitTestLocation&,
                                            const HitTestingTransformState* transformState, double* zOffset);
    RenderLayer* hitTestChildLayerColumns(RenderLayer* childLayer, RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result,
                                          const LayoutRect& hitTestRect, const HitTestLocation&,
                                          const HitTestingTransformState* transformState, double* zOffset,
                                          const Vector<RenderLayer*>& columnLayers, size_t columnIndex);

    PassRefPtr<HitTestingTransformState> createLocalTransformState(RenderLayer* rootLayer, RenderLayer* containerLayer,
                            const LayoutRect& hitTestRect, const HitTestLocation&,
                            const HitTestingTransformState* containerTransformState,
                            const LayoutPoint& translationOffset = LayoutPoint()) const;

    bool hitTestContents(const HitTestRequest&, HitTestResult&, const LayoutRect& layerBounds, const HitTestLocation&, HitTestFilter) const;
    bool hitTestContentsForFragments(const LayerFragments&, const HitTestRequest&, HitTestResult&, const HitTestLocation&, HitTestFilter, bool& insideClipRect) const;
    RenderLayer* hitTestTransformedLayerInFragments(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest&, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState* = 0, double* zOffset = 0);

    bool listBackgroundIsKnownToBeOpaqueInRect(const Vector<RenderLayer*>*, const LayoutRect&) const;

    bool shouldBeSelfPaintingLayer() const;

    // Start of ScrollableArea interface
    // To be moved to RenderLayerScrollableArea
    void updateNeedsCompositedScrolling();

public:
    bool usesCompositedScrolling() const;

    // End of ScrollableArea interface

private:
    // Rectangle encompassing the scroll corner and resizer rect.
    IntRect scrollCornerAndResizerRect() const;

    void updateCompositingLayersAfterScroll();

    // FIXME: We should only create the stacking node if needed.
    bool requiresStackingNode() const { return true; }
    void updateStackingNode();

    // FIXME: We could lazily allocate our ScrollableArea based on style properties ('overflow', ...)
    // but for now, we are always allocating it for RenderBox as it's safer.
    bool requiresScrollableArea() const { return renderBox(); }
    void updateScrollableArea();

    void updateScrollableAreaSet(bool hasOverflow);

    // Returns true our scrollable area is in the FrameView's collection of scrollable areas. This can
    // only happen if we're both scrollable, and we do in fact overflow.
    bool scrollsOverflow() const;

    void dirtyAncestorChainVisibleDescendantStatus();
    void setAncestorChainHasVisibleDescendant();

    void dirtyAncestorChainBlendedDescendantStatus();
    void setAncestorChainBlendedDescendant();

    void updateDescendantDependentFlags();

    // This flag is computed by RenderLayerCompositor, which knows more about 3d hierarchies than we do.
    void setHas3DTransformedDescendant(bool b) { m_has3DTransformedDescendant = b; }
    bool has3DTransformedDescendant() const { return m_has3DTransformedDescendant; }

    void dirty3DTransformedDescendantStatus();
    // Both updates the status, and returns true if descendants of this have 3d.
    bool update3DTransformedDescendantStatus();

    void createReflection();
    void removeReflection();

    void updateReflectionStyle();
    bool paintingInsideReflection() const { return m_paintingInsideReflection; }
    void setPaintingInsideReflection(bool b) { m_paintingInsideReflection = b; }

    void updateOrRemoveFilterClients();
    void updateOrRemoveFilterEffectRenderer();

    void parentClipRects(const ClipRectsContext&, ClipRects&) const;
    ClipRect backgroundClipRect(const ClipRectsContext&) const;

    LayoutRect paintingExtent(const RenderLayer* rootLayer, const LayoutRect& paintDirtyRect, PaintBehavior);

    RenderLayer* enclosingTransformedAncestor() const;

    // If IntSize is not given, then we must incur additional overhead to instantiate a RenderGeometryMap
    // and compute the correct offset ourselves.
    void positionOverflowControls();
    void positionOverflowControls(const IntSize&);

    void updatePagination();

    // FIXME: Temporary. Remove when new columns come online.
    bool useRegionBasedColumns() const;

    bool hasCompositingDescendant() const { return m_compositingProperties.hasCompositingDescendant; }
    void setHasCompositingDescendant(bool b)  { m_compositingProperties.hasCompositingDescendant = b; }

    void setCompositingReasons(CompositingReasons reasons) { m_compositingProperties.compositingReasons = reasons; }
    CompositingReasons compositingReasons() const { return m_compositingProperties.compositingReasons; }

    friend class CompositedLayerMapping;
    friend class RenderLayerCompositor;
    friend class RenderLayerModelObject;

protected:
    unsigned m_isSelfPaintingLayer : 1;

    // If have no self-painting descendants, we don't have to walk our children during painting. This can lead to
    // significant savings, especially if the tree has lots of non-self-painting layers grouped together (e.g. table cells).
    unsigned m_hasSelfPaintingLayerDescendant : 1;
    unsigned m_hasSelfPaintingLayerDescendantDirty : 1;

    unsigned m_hasOutOfFlowPositionedDescendant : 1;
    unsigned m_hasOutOfFlowPositionedDescendantDirty : 1;

    // This is true if we have an out-of-flow positioned descendant whose
    // containing block is our ancestor. If this is the case, the descendant
    // may fall outside of our clip preventing things like opting into
    // composited scrolling (which causes clipping of all descendants).
    unsigned m_hasUnclippedDescendant : 1;

    unsigned m_isUnclippedDescendant : 1;

    unsigned m_needsCompositedScrolling : 1;
    unsigned m_willUseCompositedScrollingHasBeenRecorded : 1;

    unsigned m_isScrollableAreaHasBeenRecorded : 1;

    const unsigned m_isRootLayer : 1;

    unsigned m_usedTransparency : 1; // Tracks whether we need to close a transparent layer, i.e., whether
                                 // we ended up painting this layer or any descendants (and therefore need to
                                 // blend).

    unsigned m_childLayerHasBlendMode : 1;
    unsigned m_childLayerHasBlendModeStatusDirty : 1;
    unsigned m_paintingInsideReflection : 1; // A state bit tracking if we are painting inside a replica.

    unsigned m_visibleContentStatusDirty : 1;
    unsigned m_hasVisibleContent : 1;
    unsigned m_visibleDescendantStatusDirty : 1;
    unsigned m_hasVisibleDescendant : 1;

    unsigned m_isPaginated : 1; // If we think this layer is split by a multi-column ancestor, then this bit will be set.

    unsigned m_3DTransformedDescendantStatusDirty : 1;
    // Set on a stacking context layer that has 3D descendants anywhere
    // in a preserves3D hierarchy. Hint to do 3D-aware hit testing.
    unsigned m_has3DTransformedDescendant : 1;

    unsigned m_containsDirtyOverlayScrollbars : 1;

    // This is an optimization added for <table>.
    // Currently cells do not need to update their repaint rectangles when scrolling. This also
    // saves a lot of time when scrolling on a table.
    const unsigned m_canSkipRepaintRectsUpdateOnScroll : 1;

    unsigned m_hasFilterInfo : 1;

    BlendMode m_blendMode;

    RenderLayerModelObject* m_renderer;

    RenderLayer* m_parent;
    RenderLayer* m_previous;
    RenderLayer* m_next;
    RenderLayer* m_first;
    RenderLayer* m_last;

    // Our current relative position offset.
    LayoutSize m_offsetForInFlowPosition;

    // Our (x,y) coordinates are in our parent layer's coordinate space.
    LayoutPoint m_topLeft;

    // The layer's width/height
    IntSize m_layerSize;

    // Cached normal flow values for absolute positioned elements with static left/top values.
    LayoutUnit m_staticInlinePosition;
    LayoutUnit m_staticBlockPosition;

    OwnPtr<TransformationMatrix> m_transform;

    // May ultimately be extended to many replicas (with their own paint order).
    RenderReplica* m_reflection;

    // Pointer to the enclosing RenderLayer that caused us to be paginated. It is 0 if we are not paginated.
    RenderLayer* m_enclosingPaginationLayer;

    // Properties that are computed while updating compositing layers. These values may be dirty/invalid if
    // compositing status is not up-to-date before using them.
    struct CompositingProperties {
        CompositingProperties()
            : hasCompositingDescendant(false)
            , viewportConstrainedNotCompositedReason(NoNotCompositedReason)
            , compositingReasons(CompositingReasonNone)
        { }

        // Used only while determining what layers should be composited. Applies to the tree of z-order lists.
        bool hasCompositingDescendant : 1;

        // The reason, if any exists, that a fixed-position layer is chosen not to be composited.
        unsigned viewportConstrainedNotCompositedReason : 2;

        // Once computed, indicates all that a layer needs to become composited using the CompositingReasons enum bitfield.
        CompositingReasons compositingReasons;
    };

    CompositingProperties m_compositingProperties;

    ForceNeedsCompositedScrollingMode m_forceNeedsCompositedScrolling;

private:
    enum CompositedScrollingHistogramBuckets {
        IsScrollableAreaBucket = 0,
        NeedsToBeStackingContainerBucket = 1,
        WillUseCompositedScrollingBucket = 2,
        CompositedScrollingHistogramMax = 3
    };

    IntRect m_blockSelectionGapsBounds;

    OwnPtr<CompositedLayerMapping> m_compositedLayerMapping;
    OwnPtr<RenderLayerScrollableArea> m_scrollableArea;

    RenderLayerRepainter m_repainter;
    RenderLayerClipper m_clipper; // FIXME: Lazily allocate?
    OwnPtr<RenderLayerStackingNode> m_stackingNode;
};

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showLayerTree(const WebCore::RenderLayer*);
void showLayerTree(const WebCore::RenderObject*);
#endif

#endif // RenderLayer_h
