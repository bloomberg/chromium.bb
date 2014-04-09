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

#include "core/rendering/compositing/CompositedLayerMappingPtr.h"
#include "core/rendering/LayerPaintingInfo.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderLayerBlendInfo.h"
#include "core/rendering/RenderLayerClipper.h"
#include "core/rendering/RenderLayerFilterInfo.h"
#include "core/rendering/RenderLayerReflectionInfo.h"
#include "core/rendering/RenderLayerRepainter.h"
#include "core/rendering/RenderLayerScrollableArea.h"
#include "core/rendering/RenderLayerStackingNode.h"
#include "core/rendering/RenderLayerStackingNodeIterator.h"
#include "platform/graphics/CompositingReasons.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class FilterEffectRenderer;
class FilterOperations;
class HitTestRequest;
class HitTestResult;
class HitTestingTransformState;
class RenderFlowThread;
class RenderGeometryMap;
class CompositedLayerMapping;
class RenderLayerCompositor;
class RenderReplica;
class RenderStyle;
class TransformationMatrix;

enum BorderRadiusClippingRule { IncludeSelfForBorderRadius, DoNotIncludeSelfForBorderRadius };
enum IncludeSelfOrNot { IncludeSelf, ExcludeSelf };

enum CompositedScrollingHistogramBuckets {
    IsScrollableAreaBucket = 0,
    NeedsToBeStackingContainerBucket = 1,
    WillUseCompositedScrollingBucket = 2,
    CompositedScrollingHistogramMax = 3
};

enum CompositingQueryMode {
    CompositingQueriesAreAllowed,
    CompositingQueriesAreOnlyAllowedInCertainDocumentLifecyclePhases
};

// FIXME: remove this once the compositing query ASSERTS are no longer hit.
class DisableCompositingQueryAsserts {
    WTF_MAKE_NONCOPYABLE(DisableCompositingQueryAsserts);
public:
    DisableCompositingQueryAsserts();
private:
    TemporaryChange<CompositingQueryMode> m_disabler;
};

class RenderLayer {
    WTF_MAKE_NONCOPYABLE(RenderLayer);
public:
    RenderLayer(RenderLayerModelObject*, LayerType);
    ~RenderLayer();

    String debugName() const;

    RenderLayerModelObject* renderer() const { return m_renderer; }
    RenderBox* renderBox() const { return m_renderer && m_renderer->isBox() ? toRenderBox(m_renderer) : 0; }
    RenderLayer* parent() const { return m_parent; }
    RenderLayer* previousSibling() const { return m_previous; }
    RenderLayer* nextSibling() const { return m_next; }
    RenderLayer* firstChild() const { return m_first; }
    RenderLayer* lastChild() const { return m_last; }

    const RenderLayer* compositingContainer() const;

    void addChild(RenderLayer* newChild, RenderLayer* beforeChild = 0);
    RenderLayer* removeChild(RenderLayer*);

    void removeOnlyThisLayer();
    void insertOnlyThisLayer();

    void styleChanged(StyleDifference, const RenderStyle* oldStyle);

    bool isSelfPaintingLayer() const { return m_isSelfPaintingLayer; }
    bool isOverflowOnlyLayer() const { return m_layerType == OverflowClipLayer; }
    bool isForcedLayer() const { return m_layerType == ForcedLayer; }

    void setLayerType(LayerType layerType) { m_layerType = layerType; }

    bool cannotBlitToWindow() const;

    bool isTransparent() const;
    RenderLayer* transparentPaintingAncestor();
    void beginTransparencyLayers(GraphicsContext*, const RenderLayer* rootLayer, const LayoutRect& paintDirtyRect, const LayoutSize& subPixelAccumulation, PaintBehavior);

    bool isReflection() const { return renderer()->isReplica(); }
    RenderLayerReflectionInfo* reflectionInfo() { return m_reflectionInfo.get(); }
    const RenderLayerReflectionInfo* reflectionInfo() const { return m_reflectionInfo.get(); }

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

    bool isRootLayer() const { return m_isRootLayer; }

    RenderLayerCompositor* compositor() const;

    // Notification from the renderer that its content changed (e.g. current frame of image changed).
    // Allows updates of layer content without repainting.
    void contentChanged(ContentChangeType);

    bool canRender3DTransforms() const;

    enum UpdateLayerPositionsFlag {
        CheckForRepaint = 1 << 0,
        NeedsFullRepaintInBacking = 1 << 1,
        UpdatePagination = 1 << 2,
    };
    typedef unsigned UpdateLayerPositionsFlags;

    void updateLayerPositionsAfterLayout(const RenderLayer* rootLayer, UpdateLayerPositionsFlags);
    void updateLayerPositionsAfterOverflowScroll();
    void updateLayerPositionsAfterDocumentScroll();

    // FIXME: Should updateLayerPositions be private?
    void updateLayerPositions(RenderGeometryMap*, UpdateLayerPositionsFlags = CheckForRepaint);

    bool isPaginated() const { return m_isPaginated; }
    RenderLayer* enclosingPaginationLayer() const { return m_enclosingPaginationLayer; }

    void updateTransform();
    RenderLayer* renderingContextRoot();

    const LayoutSize& offsetForInFlowPosition() const { return m_offsetForInFlowPosition; }

    void addBlockSelectionGapsBounds(const LayoutRect&);
    void clearBlockSelectionGapsBounds();
    void repaintBlockSelectionGaps();
    bool hasBlockSelectionGapBounds() const;

    RenderLayerStackingNode* stackingNode() { return m_stackingNode.get(); }
    const RenderLayerStackingNode* stackingNode() const { return m_stackingNode.get(); }

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


    bool hasUnclippedDescendant() const { return m_hasUnclippedDescendant; }
    void setHasUnclippedDescendant(bool hasDescendant) { m_hasUnclippedDescendant = hasDescendant; }
    void updateHasUnclippedDescendant();
    bool isUnclippedDescendant() const { return m_isUnclippedDescendant; }

    // Will ensure that hasUnclippedDescendant and hasNonCompositiedChild are up to date.
    void updateScrollingStateAfterCompositingChange();
    bool hasVisibleNonLayerContent() const { return m_hasVisibleNonLayerContent; }
    bool hasNonCompositedChild() const { return m_compositingProperties.hasNonCompositedChild; }

    bool usedTransparency() const { return m_usedTransparency; }

    // Gets the nearest enclosing positioned ancestor layer (also includes
    // the <html> layer and the root layer).
    RenderLayer* enclosingPositionedAncestor() const;

    RenderLayer* enclosingOverflowClipLayer(IncludeSelfOrNot = IncludeSelf) const;

    // Enclosing compositing layer; if includeSelf is true, may return this.
    RenderLayer* enclosingCompositingLayer(IncludeSelfOrNot = IncludeSelf) const;
    RenderLayer* enclosingCompositingLayerForRepaint(IncludeSelfOrNot = IncludeSelf) const;
    // Ancestor compositing layer, excluding this.
    RenderLayer* ancestorCompositingLayer() const { return enclosingCompositingLayer(ExcludeSelf); }

    // Ancestor composited scrolling layer at or above our containing block.
    RenderLayer* ancestorCompositedScrollingLayer() const;

    // Ancestor scrolling layer at or above our containing block.
    RenderLayer* ancestorScrollingLayer() const;

    RenderLayer* enclosingFilterLayer(IncludeSelfOrNot = IncludeSelf) const;
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

    // The two main functions that use the layer system.  The paint method
    // paints the layers that intersect the damage rect from back to
    // front.  The hitTest method looks for mouse events by walking
    // layers that intersect the point from front to back.
    void paint(GraphicsContext*, const LayoutRect& damageRect, PaintBehavior = PaintBehaviorNormal, RenderObject* paintingRoot = 0, PaintLayerFlags = 0);
    bool hitTest(const HitTestRequest&, HitTestResult&);
    bool hitTest(const HitTestRequest&, const HitTestLocation&, HitTestResult&);
    void paintOverlayScrollbars(GraphicsContext*, const LayoutRect& damageRect, PaintBehavior, RenderObject* paintingRoot = 0);

    // Pass offsetFromRoot if known.
    bool intersectsDamageRect(const LayoutRect& layerBounds, const LayoutRect& damageRect, const RenderLayer* rootLayer, const LayoutPoint* offsetFromRoot = 0) const;

    // Bounding box relative to some ancestor layer. Pass offsetFromRoot if known.
    LayoutRect physicalBoundingBox(const RenderLayer* ancestorLayer, const LayoutPoint* offsetFromRoot = 0) const;
    LayoutRect physicalBoundingBoxIncludingReflectionAndStackingChildren(const RenderLayer* ancestorLayer, const LayoutPoint& offsetFromRoot) const;

    // FIXME: This function is inconsistent as to whether the returned rect has been flipped for writing mode.
    LayoutRect boundingBoxForCompositingOverlapTest() const { return overlapBoundsIncludeChildren() ? boundingBoxForCompositing() : logicalBoundingBox(); }

    // If true, this layer's children are included in its bounds for overlap testing.
    // We can't rely on the children's positions if this layer has a filter that could have moved the children's pixels around.
    bool overlapBoundsIncludeChildren() const { return hasFilter() && renderer()->style()->filter().hasFilterThatMovesPixels(); }

    enum CalculateBoundsOptions {
        ApplyBoundsChickenEggHacks,
        DoNotApplyBoundsChickenEggHacks,
    };
    LayoutRect boundingBoxForCompositing(const RenderLayer* ancestorLayer = 0, CalculateBoundsOptions = DoNotApplyBoundsChickenEggHacks) const;

    // WARNING: This method returns the offset for the parent as this is what updateLayerPositions expects.
    LayoutPoint computeOffsetFromRoot(bool& hasLayerOffset) const;

    LayoutUnit staticInlinePosition() const { return m_staticInlinePosition; }
    LayoutUnit staticBlockPosition() const { return m_staticBlockPosition; }

    void setStaticInlinePosition(LayoutUnit position) { m_staticInlinePosition = position; }
    void setStaticBlockPosition(LayoutUnit position) { m_staticBlockPosition = position; }

    LayoutSize subpixelAccumulation() const;
    void setSubpixelAccumulation(const LayoutSize&);

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

    // FIXME: reflections should force transform-style to be flat in the style: https://bugs.webkit.org/show_bug.cgi?id=106959
    bool shouldPreserve3D() const { return !renderer()->hasReflection() && renderer()->style()->transformStyle3D() == TransformStyle3DPreserve3D; }

    void filterNeedsRepaint();
    bool hasFilter() const { return renderer()->hasFilter(); }

    bool paintsWithBlendMode() const;

    void* operator new(size_t);
    // Only safe to call from RenderLayerModelObject::destroyLayer()
    void operator delete(void*);

    CompositingState compositingState() const;

    // This returns true if our document is in a phase of its lifestyle during which
    // compositing state may legally be read.
    bool isAllowedToQueryCompositingState() const;

    // This returns true if our current phase is the compositing update.
    bool isInCompositingUpdate() const;

    CompositedLayerMappingPtr compositedLayerMapping() const;
    CompositedLayerMappingPtr ensureCompositedLayerMapping();

    // NOTE: If you are using hasCompositedLayerMapping to determine the state of compositing for this layer,
    // (and not just to do bookkeeping related to the mapping like, say, allocating or deallocating a mapping),
    // then you may have incorrect logic. Use compositingState() instead.
    bool hasCompositedLayerMapping() const { return m_compositedLayerMapping.get(); }
    void clearCompositedLayerMapping(bool layerBeingDestroyed = false);

    CompositedLayerMapping* groupedMapping() const { return m_groupedMapping; }
    void setGroupedMapping(CompositedLayerMapping* groupedMapping, bool layerBeingDestroyed = false);

    bool hasCompositedMask() const;
    bool hasCompositedClippingMask() const;
    bool needsCompositedScrolling() const { return m_scrollableArea && m_scrollableArea->needsCompositedScrolling(); }

    bool clipsCompositingDescendantsWithBorderRadius() const;

    RenderLayer* scrollParent() const;
    RenderLayer* clipParent() const;

    bool needsCompositingLayersRebuiltForClip(const RenderStyle* oldStyle, const RenderStyle* newStyle) const;
    bool needsCompositingLayersRebuiltForOverflow(const RenderStyle* oldStyle, const RenderStyle* newStyle) const;
    bool needsCompositingLayersRebuiltForFilters(const RenderStyle* oldStyle, const RenderStyle* newStyle, bool didPaintWithFilters) const;
    bool needsCompositingLayersRebuiltForBlending(const RenderStyle* oldStyle, const RenderStyle* newStyle) const;

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
        NoNotCompositedReason = 0,
        NotCompositedForBoundsOutOfView,
        NotCompositedForNonViewContainer,
        NotCompositedForNoVisibleContent,
        NotCompositedForUnscrollableAncestors,
        NumNotCompositedReasons,

        // This is the number of bits used to store the viewport constrained not composited
        // reasons. We define this constant since sizeof won't return the number of bits, and we
        // shouldn't duplicate the constant.
        ViewportConstrainedNotCompositedReasonBits = 3
    };

    void setViewportConstrainedNotCompositedReason(ViewportConstrainedNotCompositedReason reason) { m_compositingProperties.viewportConstrainedNotCompositedReason = reason; }
    ViewportConstrainedNotCompositedReason viewportConstrainedNotCompositedReason() const { return static_cast<ViewportConstrainedNotCompositedReason>(m_compositingProperties.viewportConstrainedNotCompositedReason); }

    bool isOutOfFlowRenderFlowThread() const { return renderer()->isOutOfFlowRenderFlowThread(); }

    bool scrollsWithRespectTo(const RenderLayer*) const;

    void addLayerHitTestRects(LayerHitTestRects&) const;

    // Compute rects only for this layer
    void computeSelfHitTestRects(LayerHitTestRects&) const;

    // FIXME: This should probably return a ScrollableArea but a lot of internal methods are mistakenly exposed.
    RenderLayerScrollableArea* scrollableArea() const { return m_scrollableArea.get(); }
    RenderLayerRepainter& repainter() { return m_repainter; }
    RenderLayerClipper& clipper() { return m_clipper; }
    const RenderLayerClipper& clipper() const { return m_clipper; }

    inline bool isPositionedContainer() const
    {
        // FIXME: This is not in sync with containingBlock.
        // RenderObject::canContainFixedPositionedObject() should probably be used
        // instead.
        RenderLayerModelObject* layerRenderer = renderer();
        return isRootLayer() || layerRenderer->isPositioned() || hasTransform();
    }

    void paintLayer(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);

    PassOwnPtr<Vector<FloatRect> > collectTrackedRepaintRects() const;

    RenderLayerBlendInfo& blendInfo() { return m_blendInfo; }

    void setOffsetFromSquashingLayerOrigin(IntSize offset) { m_compositingProperties.offsetFromSquashingLayerOrigin = offset; }
    IntSize offsetFromSquashingLayerOrigin() const { return m_compositingProperties.offsetFromSquashingLayerOrigin; }

    bool scrollsOverflow() const;

    bool hasDirectReasonsForCompositing() const { return compositingReasons() & CompositingReasonComboAllDirectReasons; }
    CompositingReasons styleDeterminedCompositingReasons() const { return compositingReasons() & CompositingReasonComboAllStyleDeterminedReasons; }

    void clearAncestorDependentPropertyCache();

    class AncestorDependentProperties {
    public:
        IntRect clippedAbsoluteBoundingBox;
    };

    void setNeedsToUpdateAncestorDependentProperties();
    bool childNeedsToUpdateAncestorDependantProperties() const { return m_childNeedsToUpdateAncestorDependantProperties; }
    bool needsToUpdateAncestorDependentProperties() const { return m_needsToUpdateAncestorDependentProperties; }

    void updateAncestorDependentProperties(const AncestorDependentProperties&);
    void clearChildNeedsToUpdateAncestorDependantProperties();

    const AncestorDependentProperties& ancestorDependentProperties() { ASSERT(!m_needsToUpdateAncestorDependentProperties); return m_ancestorDependentProperties; }

    bool lostGroupedMapping() const { return m_compositingProperties.lostGroupedMapping; }
    void setLostGroupedMapping(bool b) { m_compositingProperties.lostGroupedMapping = b; }

    CompositingReasons compositingReasons() const { return m_compositingProperties.compositingReasons; }
    void setCompositingReasons(CompositingReasons, CompositingReasons mask = CompositingReasonAll);

    bool hasCompositingDescendant() const { return m_compositingProperties.hasCompositingDescendant; }
    void setHasCompositingDescendant(bool b)  { m_compositingProperties.hasCompositingDescendant = b; }

    bool shouldIsolateCompositedDescendants() const { return m_compositingProperties.shouldIsolateCompositedDescendants; }
    void setShouldIsolateCompositedDescendants(bool b)  { m_compositingProperties.shouldIsolateCompositedDescendants = b; }

    void updateDescendantDependentFlags();

    void updateOrRemoveFilterEffectRenderer();

    void updateSelfPaintingLayer();

    void paintLayerContents(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);

    RenderLayer* enclosingTransformedAncestor() const;

    void didUpdateNeedsCompositedScrolling();

private:
    // FIXME: Merge with AncestorDependentProperties.
    class AncestorDependentPropertyCache {
        WTF_MAKE_NONCOPYABLE(AncestorDependentPropertyCache);
    public:
        AncestorDependentPropertyCache();

        RenderLayer* ancestorCompositedScrollingLayer() const;
        void setAncestorCompositedScrollingLayer(RenderLayer*);

        RenderLayer* scrollParent() const;
        void setScrollParent(RenderLayer*);

        bool ancestorCompositedScrollingLayerDirty() const { return m_ancestorCompositedScrollingLayerDirty; }
        bool scrollParentDirty() const { return m_scrollParentDirty; }

    private:
        RenderLayer* m_ancestorCompositedScrollingLayer;
        RenderLayer* m_scrollParent;

        bool m_ancestorCompositedScrollingLayerDirty;
        bool m_scrollParentDirty;
    };

    void ensureAncestorDependentPropertyCache() const;

    // Bounding box in the coordinates of this layer.
    LayoutRect logicalBoundingBox() const;

    bool hasOverflowControls() const;

    void setIsUnclippedDescendant(bool isUnclippedDescendant) { m_isUnclippedDescendant = isUnclippedDescendant; }

    void setAncestorChainHasSelfPaintingLayerDescendant();
    void dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

    void setAncestorChainHasOutOfFlowPositionedDescendant();
    void dirtyAncestorChainHasOutOfFlowPositionedDescendantStatus();

    void clipToRect(const LayerPaintingInfo&, GraphicsContext*, const ClipRect&, BorderRadiusClippingRule = IncludeSelfForBorderRadius);
    void restoreClip(GraphicsContext*, const LayoutRect& paintDirtyRect, const ClipRect&);

    void updateOutOfFlowPositioned(const RenderStyle* oldStyle);

    // Returns true if the position changed.
    bool updateLayerPosition();

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

    void paintLayerContentsAndReflection(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintLayerByApplyingTransform(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags, const LayoutPoint& translationOffset = LayoutPoint());
    void paintChildren(unsigned childrenToVisit, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintPaginatedChildLayer(RenderLayer* childLayer, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintChildLayerIntoColumns(RenderLayer* childLayer, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags, const Vector<RenderLayer*>& columnLayers, size_t columnIndex);

    void collectFragments(LayerFragments&, const RenderLayer* rootLayer, const LayoutRect& dirtyRect,
        ClipRectsType, OverlayScrollbarSizeRelevancy inOverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize,
        ShouldRespectOverflowClip = RespectOverflowClip, const LayoutPoint* offsetFromRoot = 0,
        const LayoutSize& subPixelAccumulation = LayoutSize(), const LayoutRect* layerBoundingBox = 0);
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
    RenderLayer* hitTestChildren(ChildrenIteration, RenderLayer* rootLayer, const HitTestRequest&, HitTestResult&,
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

    bool childBackgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const;

    bool shouldBeSelfPaintingLayer() const;

    // FIXME: We should only create the stacking node if needed.
    bool requiresStackingNode() const { return true; }
    void updateStackingNode();

    void updateReflectionInfo(const RenderStyle*);

    // FIXME: We could lazily allocate our ScrollableArea based on style properties ('overflow', ...)
    // but for now, we are always allocating it for RenderBox as it's safer.
    bool requiresScrollableArea() const { return renderBox(); }
    void updateScrollableArea();

    void dirtyAncestorChainVisibleDescendantStatus();
    void setAncestorChainHasVisibleDescendant();

    void dirty3DTransformedDescendantStatus();
    // Both updates the status, and returns true if descendants of this have 3d.
    bool update3DTransformedDescendantStatus();

    void updateOrRemoveFilterClients();

    LayoutRect paintingExtent(const RenderLayer* rootLayer, const LayoutRect& paintDirtyRect, const LayoutSize& subPixelAccumulation, PaintBehavior);

    void updatePagination();

    // FIXME: Temporary. Remove when new columns come online.
    bool useRegionBasedColumns() const;

    LayerType m_layerType;

    // Self-painting layer is an optimization where we avoid the heavy RenderLayer painting
    // machinery for a RenderLayer allocated only to handle the overflow clip case.
    // FIXME(crbug.com/332791): Self-painting layer should be merged into the overflow-only concept.
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

    const unsigned m_isRootLayer : 1;

    unsigned m_usedTransparency : 1; // Tracks whether we need to close a transparent layer, i.e., whether
                                 // we ended up painting this layer or any descendants (and therefore need to
                                 // blend).

    unsigned m_visibleContentStatusDirty : 1;
    unsigned m_hasVisibleContent : 1;
    unsigned m_visibleDescendantStatusDirty : 1;
    unsigned m_hasVisibleDescendant : 1;

    unsigned m_hasVisibleNonLayerContent : 1;

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
    unsigned m_needsToUpdateAncestorDependentProperties : 1;
    unsigned m_childNeedsToUpdateAncestorDependantProperties : 1;

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

    // Pointer to the enclosing RenderLayer that caused us to be paginated. It is 0 if we are not paginated.
    RenderLayer* m_enclosingPaginationLayer;

    // Properties that are computed while updating compositing layers. These values may be dirty/invalid if
    // compositing status is not up-to-date before using them.
    struct CompositingProperties {
        CompositingProperties()
            : hasCompositingDescendant(false)
            , hasNonCompositedChild(false)
            , shouldIsolateCompositedDescendants(false)
            , lostGroupedMapping(false)
            , viewportConstrainedNotCompositedReason(NoNotCompositedReason)
            , compositingReasons(CompositingReasonNone)
        { }

        // Used only while determining what layers should be composited. Applies to the tree of z-order lists.
        bool hasCompositingDescendant : 1;

        // Applies to the real render layer tree (i.e., the tree determined by the layer's parent and children and
        // as opposed to the tree formed by the z-order and normal flow lists).
        bool hasNonCompositedChild : 1;

        // Should be for stacking contexts having unisolated blending descendants.
        bool shouldIsolateCompositedDescendants : 1;

        // True if this render layer just lost its grouped mapping due to the CompositedLayerMapping being destroyed,
        // and we don't yet know to what graphics layer this RenderLayer will be assigned.
        bool lostGroupedMapping : 1;

        // The reason, if any exists, that a fixed-position layer is chosen not to be composited.
        unsigned viewportConstrainedNotCompositedReason : ViewportConstrainedNotCompositedReasonBits;

        // Once computed, indicates all that a layer needs to become composited using the CompositingReasons enum bitfield.
        CompositingReasons compositingReasons;

        // Used for invalidating this layer's contents on the squashing GraphicsLayer.
        IntSize offsetFromSquashingLayerOrigin;
    };

    // FIXME: Merge m_ancestorDependentPropertyCache into m_ancestorDependentProperties;
    AncestorDependentProperties m_ancestorDependentProperties;

    CompositingProperties m_compositingProperties;

    IntRect m_blockSelectionGapsBounds;

    OwnPtr<CompositedLayerMapping> m_compositedLayerMapping;
    OwnPtr<RenderLayerScrollableArea> m_scrollableArea;

    mutable OwnPtr<AncestorDependentPropertyCache> m_ancestorDependentPropertyCache;

    CompositedLayerMapping* m_groupedMapping;

    RenderLayerRepainter m_repainter;
    RenderLayerClipper m_clipper; // FIXME: Lazily allocate?
    OwnPtr<RenderLayerStackingNode> m_stackingNode;
    OwnPtr<RenderLayerReflectionInfo> m_reflectionInfo;
    RenderLayerBlendInfo m_blendInfo;

    LayoutSize m_subpixelAccumulation; // The accumulated subpixel offset of a composited layer's composited bounds compared to absolute coordinates.
};

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showLayerTree(const WebCore::RenderLayer*);
void showLayerTree(const WebCore::RenderObject*);
#endif

#endif // RenderLayer_h
