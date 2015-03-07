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

#ifndef Layer_h
#define Layer_h

#include "core/layout/LayerClipper.h"
#include "core/layout/LayerFilterInfo.h"
#include "core/layout/LayerFragment.h"
#include "core/layout/LayerReflectionInfo.h"
#include "core/layout/LayerScrollableArea.h"
#include "core/layout/LayerStackingNode.h"
#include "core/layout/LayerStackingNodeIterator.h"
#include "core/layout/LayoutBox.h"
#include "platform/graphics/CompositingReasons.h"
#include "public/platform/WebBlendMode.h"
#include "wtf/OwnPtr.h"

namespace blink {

class FilterEffectRenderer;
class FilterOperations;
class HitTestRequest;
class HitTestResult;
class HitTestingTransformState;
class LayerCompositor;
class CompositedLayerMapping;
class LayoutStyle;
class TransformationMatrix;

enum IncludeSelfOrNot { IncludeSelf, ExcludeSelf };

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

class Layer {
    WTF_MAKE_NONCOPYABLE(Layer);
public:
    Layer(LayoutBoxModelObject*, LayerType);
    ~Layer();

    String debugName() const;

    LayoutBoxModelObject* layoutObject() const { return m_renderer; }
    LayoutBox* layoutBox() const { return m_renderer && m_renderer->isBox() ? toLayoutBox(m_renderer) : 0; }
    Layer* parent() const { return m_parent; }
    Layer* previousSibling() const { return m_previous; }
    Layer* nextSibling() const { return m_next; }
    Layer* firstChild() const { return m_first; }
    Layer* lastChild() const { return m_last; }

    const Layer* compositingContainer() const;

    void addChild(Layer* newChild, Layer* beforeChild = 0);
    Layer* removeChild(Layer*);

    void removeOnlyThisLayer();
    void insertOnlyThisLayer();

    void styleChanged(StyleDifference, const LayoutStyle* oldStyle);

    // FIXME: Many people call this function while it has out-of-date information.
    bool isSelfPaintingLayer() const { return m_isSelfPaintingLayer; }

    void setLayerType(LayerType layerType) { m_layerType = layerType; }

    bool isTransparent() const { return layoutObject()->isTransparent() || layoutObject()->style()->hasBlendMode() || layoutObject()->hasMask(); }

    bool isReflection() const { return layoutObject()->isReplica(); }
    LayerReflectionInfo* reflectionInfo() { return m_reflectionInfo.get(); }
    const LayerReflectionInfo* reflectionInfo() const { return m_reflectionInfo.get(); }

    const Layer* root() const
    {
        const Layer* curr = this;
        while (curr->parent())
            curr = curr->parent();
        return curr;
    }

    const LayoutPoint& location() const { ASSERT(!m_needsPositionUpdate); return m_location; }
    // FIXME: size() should ASSERT(!m_needsPositionUpdate) as well, but that fails in some tests,
    // for example, fast/repaint/clipped-relative.html.
    const IntSize& size() const { return m_size; }
    void setSizeHackForLayoutTreeAsText(const IntSize& size) { m_size = size; }

    LayoutRect rect() const { return LayoutRect(location(), LayoutSize(size())); }

    bool isRootLayer() const { return m_isRootLayer; }

    LayerCompositor* compositor() const;

    // Notification from the renderer that its content changed (e.g. current frame of image changed).
    // Allows updates of layer content without invalidating paint.
    void contentChanged(ContentChangeType);

    void updateLayerPositionsAfterLayout();
    void updateLayerPositionsAfterOverflowScroll();

    bool isPaginated() const { return m_isPaginated; }
    Layer* enclosingPaginationLayer() const { return m_enclosingPaginationLayer; }

    void updateTransformationMatrix();
    Layer* renderingContextRoot();

    const LayoutSize& offsetForInFlowPosition() const { return m_offsetForInFlowPosition; }

    void blockSelectionGapsBoundsChanged();
    void addBlockSelectionGapsBounds(const LayoutRect&);
    void clearBlockSelectionGapsBounds();
    void invalidatePaintForBlockSelectionGaps();
    IntRect blockSelectionGapsBounds() const;
    bool hasBlockSelectionGapBounds() const;

    LayerStackingNode* stackingNode() { return m_stackingNode.get(); }
    const LayerStackingNode* stackingNode() const { return m_stackingNode.get(); }

    bool subtreeIsInvisible() const { return !hasVisibleContent() && !hasVisibleDescendant(); }

    // FIXME: hasVisibleContent() should call updateDescendantDependentFlags() if m_visibleContentStatusDirty.
    bool hasVisibleContent() const { ASSERT(!m_visibleContentStatusDirty); return m_hasVisibleContent; }

    // FIXME: hasVisibleDescendant() should call updateDescendantDependentFlags() if m_visibleDescendantStatusDirty.
    bool hasVisibleDescendant() const { ASSERT(!m_visibleDescendantStatusDirty); return m_hasVisibleDescendant; }

    void dirtyVisibleContentStatus();
    void potentiallyDirtyVisibleContentStatus(EVisibility);

    bool hasBoxDecorationsOrBackground() const;
    bool hasVisibleBoxDecorations() const;
    // True if this layer container renderers that paint.
    bool hasNonEmptyChildRenderers() const;

    // Will ensure that hasNonCompositiedChild are up to date.
    void updateScrollingStateAfterCompositingChange();
    bool hasVisibleNonLayerContent() const { return m_hasVisibleNonLayerContent; }
    bool hasNonCompositedChild() const { ASSERT(isAllowedToQueryCompositingState()); return m_hasNonCompositedChild; }

    // Gets the nearest enclosing positioned ancestor layer (also includes
    // the <html> layer and the root layer).
    Layer* enclosingPositionedAncestor() const;

    bool isPaintInvalidationContainer() const;

    // Do *not* call this method unless you know what you are dooing. You probably want to call enclosingCompositingLayerForPaintInvalidation() instead.
    // If includeSelf is true, may return this.
    Layer* enclosingLayerWithCompositedLayerMapping(IncludeSelfOrNot) const;

    // Returns the enclosing layer root into which this layer paints, inclusive of this one. Note that the enclosing layer may or may not have its own
    // GraphicsLayer backing, but is nevertheless the root for a call to the Layer::paint*() methods.
    Layer* enclosingLayerForPaintInvalidation() const;

    Layer* enclosingLayerForPaintInvalidationCrossingFrameBoundaries() const;

    bool hasAncestorWithFilterOutsets() const;

    bool canUseConvertToLayerCoords() const
    {
        // These LayoutObjects have an impact on their layers without the renderers knowing about it.
        return !layoutObject()->hasColumns() && !layoutObject()->hasTransformRelatedProperty() && !layoutObject()->isSVGRoot();
    }

    void convertToLayerCoords(const Layer* ancestorLayer, LayoutPoint&) const;
    void convertToLayerCoords(const Layer* ancestorLayer, LayoutRect&) const;

    // Does the same as convertToLayerCoords() when not in multicol. For multicol, however,
    // convertToLayerCoords() calculates the offset in flow-thread coordinates (what the layout
    // engine uses internally), while this method calculates the visual coordinates; i.e. it figures
    // out which column the layer starts in and adds in the offset. See
    // http://www.chromium.org/developers/design-documents/multi-column-layout for more info.
    LayoutPoint visualOffsetFromAncestor(const Layer* ancestorLayer) const;

    // The hitTest() method looks for mouse events by walking layers that intersect the point from front to back.
    bool hitTest(const HitTestRequest&, HitTestResult&);
    bool hitTest(const HitTestRequest&, const HitTestLocation&, HitTestResult&);

    // Pass offsetFromRoot if known.
    bool intersectsDamageRect(const LayoutRect& layerBounds, const LayoutRect& damageRect, const Layer* rootLayer, const LayoutPoint* offsetFromRoot = 0) const;

    // Bounding box relative to some ancestor layer. Pass offsetFromRoot if known.
    LayoutRect physicalBoundingBox(const Layer* ancestorLayer, const LayoutPoint* offsetFromRoot = 0) const;
    LayoutRect physicalBoundingBoxIncludingReflectionAndStackingChildren(const Layer* ancestorLayer, const LayoutPoint& offsetFromRoot) const;
    LayoutRect fragmentsBoundingBox(const Layer* ancestorLayer) const;

    LayoutRect boundingBoxForCompositingOverlapTest() const;

    // If true, this layer's children are included in its bounds for overlap testing.
    // We can't rely on the children's positions if this layer has a filter that could have moved the children's pixels around.
    bool overlapBoundsIncludeChildren() const { return hasFilter() && layoutObject()->style()->filter().hasFilterThatMovesPixels(); }

    enum CalculateBoundsOptions {
        ApplyBoundsChickenEggHacks,
        DoNotApplyBoundsChickenEggHacks,
    };
    LayoutRect boundingBoxForCompositing(const Layer* ancestorLayer = 0, CalculateBoundsOptions = DoNotApplyBoundsChickenEggHacks) const;

    LayoutUnit staticInlinePosition() const { return m_staticInlinePosition; }
    LayoutUnit staticBlockPosition() const { return m_staticBlockPosition; }

    void setStaticInlinePosition(LayoutUnit position) { m_staticInlinePosition = position; }
    void setStaticBlockPosition(LayoutUnit position) { m_staticBlockPosition = position; }

    LayoutSize subpixelAccumulation() const;
    void setSubpixelAccumulation(const LayoutSize&);

    bool hasTransformRelatedProperty() const { return layoutObject()->hasTransformRelatedProperty(); }
    // Note that this transform has the transform-origin baked in.
    TransformationMatrix* transform() const { return m_transform.get(); }
    void setTransform(PassOwnPtr<TransformationMatrix> transform) { m_transform = transform; }
    void clearTransform() { m_transform.clear(); }

    // currentTransform computes a transform which takes accelerated animations into account. The
    // resulting transform has transform-origin baked in. If the layer does not have a transform,
    // returns the identity matrix.
    TransformationMatrix currentTransform(LayoutStyle::ApplyTransformOrigin = LayoutStyle::IncludeTransformOrigin) const;
    TransformationMatrix renderableTransform(PaintBehavior) const;

    // Get the perspective transform, which is applied to transformed sublayers.
    // Returns true if the layer has a -webkit-perspective.
    // Note that this transform has the perspective-origin baked in.
    TransformationMatrix perspectiveTransform() const;
    FloatPoint perspectiveOrigin() const;
    bool preserves3D() const { return layoutObject()->style()->transformStyle3D() == TransformStyle3DPreserve3D; }
    bool has3DTransform() const { return m_transform && !m_transform->isAffine(); }

    // FIXME: reflections should force transform-style to be flat in the style: https://bugs.webkit.org/show_bug.cgi?id=106959
    bool shouldPreserve3D() const { return !layoutObject()->hasReflection() && layoutObject()->style()->transformStyle3D() == TransformStyle3DPreserve3D; }

    void filterNeedsPaintInvalidation();
    bool hasFilter() const { return layoutObject()->hasFilter(); }

    void* operator new(size_t);
    // Only safe to call from LayoutBoxModelObject::destroyLayer()
    void operator delete(void*);

    CompositingState compositingState() const;

    // This returns true if our document is in a phase of its lifestyle during which
    // compositing state may legally be read.
    bool isAllowedToQueryCompositingState() const;

    // Don't null check this.
    CompositedLayerMapping* compositedLayerMapping() const;
    GraphicsLayer* graphicsLayerBacking() const;
    GraphicsLayer* graphicsLayerBackingForScrolling() const;
    // NOTE: If you are using hasCompositedLayerMapping to determine the state of compositing for this layer,
    // (and not just to do bookkeeping related to the mapping like, say, allocating or deallocating a mapping),
    // then you may have incorrect logic. Use compositingState() instead.
    // FIXME: This is identical to null checking compositedLayerMapping(), why not just call that?
    bool hasCompositedLayerMapping() const { return m_compositedLayerMapping.get(); }
    void ensureCompositedLayerMapping();
    void clearCompositedLayerMapping(bool layerBeingDestroyed = false);
    CompositedLayerMapping* groupedMapping() const { return m_groupedMapping; }
    void setGroupedMapping(CompositedLayerMapping* groupedMapping, bool layerBeingDestroyed = false);

    bool hasCompositedMask() const;
    bool hasCompositedClippingMask() const;
    bool needsCompositedScrolling() const { return m_scrollableArea && m_scrollableArea->needsCompositedScrolling(); }

    // Computes the position of the given render object in the space of |paintInvalidationContainer|.
    // FIXME: invert the logic to have paint invalidation containers take care of painting objects into them, rather than the reverse.
    // This will allow us to clean up this static method messiness.
    static LayoutPoint positionFromPaintInvalidationBacking(const LayoutObject*, const LayoutBoxModelObject* paintInvalidationContainer, const PaintInvalidationState* = 0);

    static void mapPointToPaintBackingCoordinates(const LayoutBoxModelObject* paintInvalidationContainer, FloatPoint&);
    static void mapRectToPaintBackingCoordinates(const LayoutBoxModelObject* paintInvalidationContainer, LayoutRect&);

    // Adjusts the given rect (in the coordinate space of the LayoutObject) to the coordinate space of |paintInvalidationContainer|'s GraphicsLayer backing.
    static void mapRectToPaintInvalidationBacking(const LayoutObject*, const LayoutBoxModelObject* paintInvalidationContainer, LayoutRect&, const PaintInvalidationState* = 0);

    // Computes the bounding paint invalidation rect for |layoutObject|, in the coordinate space of |paintInvalidationContainer|'s GraphicsLayer backing.
    static LayoutRect computePaintInvalidationRect(const LayoutObject*, const Layer* paintInvalidationContainer, const PaintInvalidationState* = 0);

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

    FilterOperations computeFilterOperations(const LayoutStyle&);
    bool paintsWithFilters() const;
    FilterEffectRenderer* filterRenderer() const
    {
        LayerFilterInfo* filterInfo = this->filterInfo();
        return filterInfo ? filterInfo->layoutObject() : 0;
    }

    LayerFilterInfo* filterInfo() const { return hasFilterInfo() ? LayerFilterInfo::filterInfoForLayer(this) : 0; }
    LayerFilterInfo* ensureFilterInfo() { return LayerFilterInfo::createFilterInfoForLayerIfNeeded(this); }
    void removeFilterInfoIfNeeded()
    {
        if (hasFilterInfo())
            LayerFilterInfo::removeFilterInfoForLayer(this);
    }

    bool hasFilterInfo() const { return m_hasFilterInfo; }
    void setHasFilterInfo(bool hasFilterInfo) { m_hasFilterInfo = hasFilterInfo; }

    void updateFilters(const LayoutStyle* oldStyle, const LayoutStyle& newStyle);

    Node* enclosingElement() const;

    bool isInTopLayer() const;

    bool scrollsWithViewport() const;
    bool scrollsWithRespectTo(const Layer*) const;

    void addLayerHitTestRects(LayerHitTestRects&) const;

    // Compute rects only for this layer
    void computeSelfHitTestRects(LayerHitTestRects&) const;

    // FIXME: This should probably return a ScrollableArea but a lot of internal methods are mistakenly exposed.
    LayerScrollableArea* scrollableArea() const { return m_scrollableArea.get(); }
    LayerClipper& clipper() { return m_clipper; }
    const LayerClipper& clipper() const { return m_clipper; }

    inline bool isPositionedContainer() const
    {
        // FIXME: This is not in sync with containingBlock.
        // LayoutObject::canContainFixedPositionedObject() should probably be used
        // instead.
        LayoutBoxModelObject* layerRenderer = layoutObject();
        return isRootLayer() || layerRenderer->isPositioned() || hasTransformRelatedProperty();
    }

    bool scrollsOverflow() const;

    CompositingReasons potentialCompositingReasonsFromStyle() const { return m_potentialCompositingReasonsFromStyle; }
    void setPotentialCompositingReasonsFromStyle(CompositingReasons reasons) { ASSERT(reasons == (reasons & CompositingReasonComboAllStyleDeterminedReasons)); m_potentialCompositingReasonsFromStyle = reasons; }

    bool hasStyleDeterminedDirectCompositingReasons() const { return m_potentialCompositingReasonsFromStyle & CompositingReasonComboAllDirectStyleDeterminedReasons; }

    class AncestorDependentCompositingInputs {
    public:
        AncestorDependentCompositingInputs()
            : opacityAncestor(0)
            , transformAncestor(0)
            , filterAncestor(0)
            , clippingContainer(0)
            , ancestorScrollingLayer(0)
            , scrollParent(0)
            , clipParent(0)
            , hasAncestorWithClipPath(false)
        { }

        IntRect clippedAbsoluteBoundingBox;
        const Layer* opacityAncestor;
        const Layer* transformAncestor;
        const Layer* filterAncestor;
        const LayoutObject* clippingContainer;
        const Layer* ancestorScrollingLayer;

        // A scroll parent is a compositor concept. It's only needed in blink
        // because we need to use it as a promotion trigger. A layer has a
        // scroll parent if neither its compositor scrolling ancestor, nor any
        // other layer scrolled by this ancestor, is a stacking ancestor of this
        // layer. Layers with scroll parents must be scrolled with the main
        // scrolling layer by the compositor.
        const Layer* scrollParent;

        // A clip parent is another compositor concept that has leaked into
        // blink so that it may be used as a promotion trigger. Layers with clip
        // parents escape the clip of a stacking tree ancestor. The compositor
        // needs to know about clip parents in order to circumvent its normal
        // clipping logic.
        const Layer* clipParent;

        unsigned hasAncestorWithClipPath : 1;
    };

    class DescendantDependentCompositingInputs {
    public:
        DescendantDependentCompositingInputs()
            : hasDescendantWithClipPath(false)
            , hasNonIsolatedDescendantWithBlendMode(false)
        { }

        unsigned hasDescendantWithClipPath : 1;
        unsigned hasNonIsolatedDescendantWithBlendMode : 1;
    };

    void setNeedsCompositingInputsUpdate();
    bool childNeedsCompositingInputsUpdate() const { return m_childNeedsCompositingInputsUpdate; }
    bool needsCompositingInputsUpdate() const
    {
        // While we're updating the compositing inputs, these values may differ.
        // We should never be asking for this value when that is the case.
        ASSERT(m_needsDescendantDependentCompositingInputsUpdate == m_needsAncestorDependentCompositingInputsUpdate);
        return m_needsDescendantDependentCompositingInputsUpdate;
    }

    void updateAncestorDependentCompositingInputs(const AncestorDependentCompositingInputs&);
    void updateDescendantDependentCompositingInputs(const DescendantDependentCompositingInputs&);
    void didUpdateCompositingInputs();

    const AncestorDependentCompositingInputs& ancestorDependentCompositingInputs() const { ASSERT(!m_needsAncestorDependentCompositingInputsUpdate); return m_ancestorDependentCompositingInputs; }
    const DescendantDependentCompositingInputs& descendantDependentCompositingInputs() const { ASSERT(!m_needsDescendantDependentCompositingInputsUpdate); return m_descendantDependentCompositingInputs; }

    IntRect clippedAbsoluteBoundingBox() const { return ancestorDependentCompositingInputs().clippedAbsoluteBoundingBox; }
    const Layer* opacityAncestor() const { return ancestorDependentCompositingInputs().opacityAncestor; }
    const Layer* transformAncestor() const { return ancestorDependentCompositingInputs().transformAncestor; }
    const Layer* filterAncestor() const { return ancestorDependentCompositingInputs().filterAncestor; }
    const LayoutObject* clippingContainer() const { return ancestorDependentCompositingInputs().clippingContainer; }
    const Layer* ancestorScrollingLayer() const { return ancestorDependentCompositingInputs().ancestorScrollingLayer; }
    Layer* scrollParent() const { return const_cast<Layer*>(ancestorDependentCompositingInputs().scrollParent); }
    Layer* clipParent() const { return const_cast<Layer*>(ancestorDependentCompositingInputs().clipParent); }
    bool hasAncestorWithClipPath() const { return ancestorDependentCompositingInputs().hasAncestorWithClipPath; }
    bool hasDescendantWithClipPath() const { return descendantDependentCompositingInputs().hasDescendantWithClipPath; }
    bool hasNonIsolatedDescendantWithBlendMode() const;

    bool lostGroupedMapping() const { ASSERT(isAllowedToQueryCompositingState()); return m_lostGroupedMapping; }
    void setLostGroupedMapping(bool b) { m_lostGroupedMapping = b; }

    CompositingReasons compositingReasons() const { ASSERT(isAllowedToQueryCompositingState()); return m_compositingReasons; }
    void setCompositingReasons(CompositingReasons, CompositingReasons mask = CompositingReasonAll);

    bool hasCompositingDescendant() const { ASSERT(isAllowedToQueryCompositingState()); return m_hasCompositingDescendant; }
    void setHasCompositingDescendant(bool);

    bool shouldIsolateCompositedDescendants() const { ASSERT(isAllowedToQueryCompositingState()); return m_shouldIsolateCompositedDescendants; }
    void setShouldIsolateCompositedDescendants(bool);

    void updateDescendantDependentFlags();
    void updateDescendantDependentFlagsForEntireSubtree();

    void updateOrRemoveFilterEffectRenderer();

    void updateSelfPaintingLayer();

    Layer* enclosingTransformedAncestor() const;
    LayoutPoint computeOffsetFromTransformedAncestor() const;

    void didUpdateNeedsCompositedScrolling();

    void setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();

    bool hasSelfPaintingLayerDescendant() const
    {
        if (m_hasSelfPaintingLayerDescendantDirty)
            updateHasSelfPaintingLayerDescendant();
        ASSERT(!m_hasSelfPaintingLayerDescendantDirty);
        return m_hasSelfPaintingLayerDescendant;
    }
    LayoutRect paintingExtent(const Layer* rootLayer, const LayoutRect& paintDirtyRect, const LayoutSize& subPixelAccumulation, PaintBehavior);
    void appendSingleFragmentIgnoringPagination(LayerFragments&, const Layer* rootLayer, const LayoutRect& dirtyRect, ClipRectsCacheSlot, OverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize, ShouldRespectOverflowClip = RespectOverflowClip, const LayoutPoint* offsetFromRoot = 0, const LayoutSize& subPixelAccumulation = LayoutSize());
    void collectFragments(LayerFragments&, const Layer* rootLayer, const LayoutRect& dirtyRect,
        ClipRectsCacheSlot, OverlayScrollbarSizeRelevancy inOverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize,
        ShouldRespectOverflowClip = RespectOverflowClip, const LayoutPoint* offsetFromRoot = 0,
        const LayoutSize& subPixelAccumulation = LayoutSize(), const LayoutRect* layerBoundingBox = 0);

    LayoutPoint layoutBoxLocation() const { return layoutObject()->isBox() ? toLayoutBox(layoutObject())->location() : LayoutPoint(); }

    enum TransparencyClipBoxBehavior {
        PaintingTransparencyClipBox,
        HitTestingTransparencyClipBox
    };

    enum TransparencyClipBoxMode {
        DescendantsOfTransparencyClipBox,
        RootOfTransparencyClipBox
    };

    static LayoutRect transparencyClipBox(const Layer*, const Layer* rootLayer, TransparencyClipBoxBehavior transparencyBehavior,
        TransparencyClipBoxMode transparencyMode, const LayoutSize& subPixelAccumulation, PaintBehavior = 0);

private:
    // Bounding box in the coordinates of this layer.
    LayoutRect logicalBoundingBox() const;

    bool hasOverflowControls() const;

    void dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

    // Returns true if the position changed.
    bool updateLayerPosition();

    void updateLayerPositionRecursive();
    void updateLayerPositionsAfterScrollRecursive();

    void setNextSibling(Layer* next) { m_next = next; }
    void setPreviousSibling(Layer* prev) { m_previous = prev; }
    void setFirstChild(Layer* first) { m_first = first; }
    void setLastChild(Layer* last) { m_last = last; }

    void updateHasSelfPaintingLayerDescendant() const;
    Layer* hitTestLayer(Layer* rootLayer, Layer* containerLayer, const HitTestRequest&, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&, bool appliedTransform,
        const HitTestingTransformState* = 0, double* zOffset = 0);
    Layer* hitTestLayerByApplyingTransform(Layer* rootLayer, Layer* containerLayer, const HitTestRequest&, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState* = 0, double* zOffset = 0,
        const LayoutPoint& translationOffset = LayoutPoint());
    Layer* hitTestChildren(ChildrenIteration, Layer* rootLayer, const HitTestRequest&, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&,
        const HitTestingTransformState*, double* zOffsetForDescendants, double* zOffset,
        const HitTestingTransformState* unflattenedTransformState, bool depthSortDescendants);
    Layer* hitTestPaginatedChildLayer(Layer* childLayer, Layer* rootLayer, const HitTestRequest&, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&,
        const HitTestingTransformState*, double* zOffset);
    Layer* hitTestChildLayerColumns(Layer* childLayer, Layer* rootLayer, const HitTestRequest&, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&,
        const HitTestingTransformState*, double* zOffset,
        const Vector<Layer*>& columnLayers, size_t columnIndex);

    PassRefPtr<HitTestingTransformState> createLocalTransformState(Layer* rootLayer, Layer* containerLayer,
        const LayoutRect& hitTestRect, const HitTestLocation&,
        const HitTestingTransformState* containerTransformState,
        const LayoutPoint& translationOffset = LayoutPoint()) const;

    bool hitTestContents(const HitTestRequest&, HitTestResult&, const LayoutRect& layerBounds, const HitTestLocation&, HitTestFilter) const;
    bool hitTestContentsForFragments(const LayerFragments&, const HitTestRequest&, HitTestResult&, const HitTestLocation&, HitTestFilter, bool& insideClipRect) const;
    Layer* hitTestTransformedLayerInFragments(Layer* rootLayer, Layer* containerLayer, const HitTestRequest&, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState* = 0, double* zOffset = 0);

    bool childBackgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const;

    bool shouldBeSelfPaintingLayer() const;

    // FIXME: We should only create the stacking node if needed.
    bool requiresStackingNode() const { return true; }
    void updateStackingNode();

    void updateReflectionInfo(const LayoutStyle*);

    // FIXME: We could lazily allocate our ScrollableArea based on style properties ('overflow', ...)
    // but for now, we are always allocating it for LayoutBox as it's safer.
    bool requiresScrollableArea() const { return layoutBox(); }
    void updateScrollableArea();

    void dirtyAncestorChainVisibleDescendantStatus();

    bool attemptDirectCompositingUpdate(StyleDifference, const LayoutStyle* oldStyle);
    void updateTransform(const LayoutStyle* oldStyle, const LayoutStyle& newStyle);

    void dirty3DTransformedDescendantStatus();
    // Both updates the status, and returns true if descendants of this have 3d.
    bool update3DTransformedDescendantStatus();

    void updateOrRemoveFilterClients();

    void updatePaginationRecursive(bool needsPaginationUpdate = false);
    void updatePagination();
    void clearPaginationRecursive();

    // FIXME: Temporary. Remove when new columns come online.
    bool useRegionBasedColumns() const;

    LayerType m_layerType;

    // Self-painting layer is an optimization where we avoid the heavy Layer painting
    // machinery for a Layer allocated only to handle the overflow clip case.
    // FIXME(crbug.com/332791): Self-painting layer should be merged into the overflow-only concept.
    unsigned m_isSelfPaintingLayer : 1;

    // If have no self-painting descendants, we don't have to walk our children during painting. This can lead to
    // significant savings, especially if the tree has lots of non-self-painting layers grouped together (e.g. table cells).
    mutable unsigned m_hasSelfPaintingLayerDescendant : 1;
    mutable unsigned m_hasSelfPaintingLayerDescendantDirty : 1;

    const unsigned m_isRootLayer : 1;

    unsigned m_visibleContentStatusDirty : 1;
    unsigned m_hasVisibleContent : 1;
    unsigned m_visibleDescendantStatusDirty : 1;
    unsigned m_hasVisibleDescendant : 1;

    unsigned m_hasVisibleNonLayerContent : 1;

    unsigned m_isPaginated : 1; // If we think this layer is split by a multi-column ancestor, then this bit will be set.

#if ENABLE(ASSERT)
    unsigned m_needsPositionUpdate : 1;
#endif

    unsigned m_3DTransformedDescendantStatusDirty : 1;
    // Set on a stacking context layer that has 3D descendants anywhere
    // in a preserves3D hierarchy. Hint to do 3D-aware hit testing.
    unsigned m_has3DTransformedDescendant : 1;

    unsigned m_containsDirtyOverlayScrollbars : 1;

    unsigned m_hasFilterInfo : 1;
    unsigned m_needsAncestorDependentCompositingInputsUpdate : 1;
    unsigned m_needsDescendantDependentCompositingInputsUpdate : 1;
    unsigned m_childNeedsCompositingInputsUpdate : 1;

    // Used only while determining what layers should be composited. Applies to the tree of z-order lists.
    unsigned m_hasCompositingDescendant : 1;

    // Applies to the real render layer tree (i.e., the tree determined by the layer's parent and children and
    // as opposed to the tree formed by the z-order and normal flow lists).
    unsigned m_hasNonCompositedChild : 1;

    // Should be for stacking contexts having unisolated blending descendants.
    unsigned m_shouldIsolateCompositedDescendants : 1;

    // True if this render layer just lost its grouped mapping due to the CompositedLayerMapping being destroyed,
    // and we don't yet know to what graphics layer this Layer will be assigned.
    unsigned m_lostGroupedMapping : 1;

    LayoutBoxModelObject* m_renderer;

    Layer* m_parent;
    Layer* m_previous;
    Layer* m_next;
    Layer* m_first;
    Layer* m_last;

    // Our current relative position offset.
    LayoutSize m_offsetForInFlowPosition;

    // Our (x,y) coordinates are in our parent layer's coordinate space.
    LayoutPoint m_location;

    // The layer's width/height
    IntSize m_size;

    // Cached normal flow values for absolute positioned elements with static left/top values.
    LayoutUnit m_staticInlinePosition;
    LayoutUnit m_staticBlockPosition;

    OwnPtr<TransformationMatrix> m_transform;

    // Pointer to the enclosing Layer that caused us to be paginated. It is 0 if we are not paginated.
    //
    // See LayoutMultiColumnFlowThread and
    // https://sites.google.com/a/chromium.org/dev/developers/design-documents/multi-column-layout
    // for more information about the multicol implementation. It's important to understand the
    // difference between flow thread coordinates and visual coordinates when working with multicol
    // in Layer, since Layer is one of the few places where we have to worry about the
    // visual ones. Internally we try to use flow-thread coordinates whenever possible.
    Layer* m_enclosingPaginationLayer;

    // These compositing reasons are updated whenever style changes, not while updating compositing layers.
    // They should not be used to infer the compositing state of this layer.
    CompositingReasons m_potentialCompositingReasonsFromStyle;

    // Once computed, indicates all that a layer needs to become composited using the CompositingReasons enum bitfield.
    CompositingReasons m_compositingReasons;

    DescendantDependentCompositingInputs m_descendantDependentCompositingInputs;
    AncestorDependentCompositingInputs m_ancestorDependentCompositingInputs;

    IntRect m_blockSelectionGapsBounds;

    OwnPtr<CompositedLayerMapping> m_compositedLayerMapping;
    OwnPtr<LayerScrollableArea> m_scrollableArea;

    CompositedLayerMapping* m_groupedMapping;

    LayerClipper m_clipper; // FIXME: Lazily allocate?
    OwnPtr<LayerStackingNode> m_stackingNode;
    OwnPtr<LayerReflectionInfo> m_reflectionInfo;

    LayoutSize m_subpixelAccumulation; // The accumulated subpixel offset of a composited layer's composited bounds compared to absolute coordinates.
};

} // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showLayerTree(const blink::Layer*);
void showLayerTree(const blink::LayoutObject*);
#endif

#endif // Layer_h
