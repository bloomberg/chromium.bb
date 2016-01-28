// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayerPainter.h"

#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/ClipPathOperation.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutFrame.h"
#include "core/layout/LayoutView.h"
#include "core/layout/svg/LayoutSVGResourceClipper.h"
#include "core/page/Page.h"
#include "core/paint/FilterPainter.h"
#include "core/paint/LayerClipRecorder.h"
#include "core/paint/LayerFixedPositionRecorder.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/SVGClipPainter.h"
#include "core/paint/ScopeRecorder.h"
#include "core/paint/ScrollRecorder.h"
#include "core/paint/ScrollableAreaPainter.h"
#include "core/paint/Transform3DRecorder.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/ClipPathRecorder.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"
#include "platform/graphics/paint/SubsequenceRecorder.h"
#include "platform/graphics/paint/Transform3DDisplayItem.h"
#include "wtf/Optional.h"

namespace blink {

static inline bool shouldSuppressPaintingLayer(PaintLayer* layer)
{
    // Avoid painting descendants of the root layer when stylesheets haven't loaded. This eliminates FOUC.
    // It's ok not to draw, because later on, when all the stylesheets do load, updateStyleSelector on the Document
    // will do a full paintInvalidationForWholeLayoutObject().
    if (layer->layoutObject()->document().didLayoutWithPendingStylesheets() && !layer->isRootLayer() && !layer->layoutObject()->isDocumentElement())
        return true;

    return false;
}

void PaintLayerPainter::paint(GraphicsContext& context, const LayoutRect& damageRect, const GlobalPaintFlags globalPaintFlags, PaintLayerFlags paintFlags)
{
    PaintLayerPaintingInfo paintingInfo(&m_paintLayer, LayoutRect(enclosingIntRect(damageRect)), globalPaintFlags, LayoutSize());
    if (shouldPaintLayerInSoftwareMode(globalPaintFlags, paintFlags))
        paintLayer(context, paintingInfo, paintFlags);
}

static ShouldRespectOverflowClip shouldRespectOverflowClip(PaintLayerFlags paintFlags, const LayoutObject* layoutObject)
{
    return (paintFlags & PaintLayerPaintingOverflowContents || (paintFlags & PaintLayerPaintingChildClippingMaskPhase && layoutObject->hasClipPath())) ? IgnoreOverflowClip : RespectOverflowClip;
}

PaintLayerPainter::PaintResult PaintLayerPainter::paintLayer(GraphicsContext& context, const PaintLayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    // https://code.google.com/p/chromium/issues/detail?id=343772
    DisableCompositingQueryAsserts disabler;

    if (m_paintLayer.compositingState() != NotComposited) {
        if (paintingInfo.globalPaintFlags() & GlobalPaintFlattenCompositingLayers) {
            // FIXME: ok, but what about GlobalPaintFlattenCompositingLayers? That's for printing and drag-image.
            // FIXME: why isn't the code here global, as opposed to being set on each paintLayer() call?
            paintFlags |= PaintLayerUncachedClipRects;
        }
    }

    // Non self-painting layers without self-painting descendants don't need to be painted as their
    // layoutObject() should properly paint itself.
    if (!m_paintLayer.isSelfPaintingLayer() && !m_paintLayer.hasSelfPaintingLayerDescendant())
        return FullyPainted;

    if (shouldSuppressPaintingLayer(&m_paintLayer))
        return FullyPainted;

    if (m_paintLayer.layoutObject()->isLayoutView() && toLayoutView(m_paintLayer.layoutObject())->frameView()->shouldThrottleRendering())
        return FullyPainted;

    // If this layer is totally invisible then there is nothing to paint.
    if (!m_paintLayer.layoutObject()->opacity() && !m_paintLayer.layoutObject()->hasBackdropFilter())
        return FullyPainted;

    if (m_paintLayer.paintsWithTransparency(paintingInfo.globalPaintFlags()))
        paintFlags |= PaintLayerHaveTransparency;

    LayerFixedPositionRecorder fixedPositionRecorder(context, *m_paintLayer.layoutObject());

    // PaintLayerAppliedTransform is used in LayoutReplica, to avoid applying the transform twice.
    if (m_paintLayer.paintsWithTransform(paintingInfo.globalPaintFlags()) && !(paintFlags & PaintLayerAppliedTransform))
        return paintLayerWithTransform(context, paintingInfo, paintFlags);

    return paintLayerContentsAndReflection(context, paintingInfo, paintFlags);
}

PaintLayerPainter::PaintResult PaintLayerPainter::paintLayerContentsAndReflection(GraphicsContext& context, const PaintLayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags, FragmentPolicy fragmentPolicy)
{
    ASSERT(m_paintLayer.isSelfPaintingLayer() || m_paintLayer.hasSelfPaintingLayerDescendant());

    PaintLayerFlags localPaintFlags = paintFlags & ~(PaintLayerAppliedTransform);

    PaintResult result = FullyPainted;

    // Paint the reflection first if we have one.
    if (m_paintLayer.reflectionInfo()) {
        ScopeRecorder scopeRecorder(context);
        if (m_paintLayer.reflectionInfo()->paint(context, paintingInfo, localPaintFlags) == MayBeClippedByPaintDirtyRect)
            result = MayBeClippedByPaintDirtyRect;
    }

    localPaintFlags |= PaintLayerPaintingCompositingAllPhases;
    if (paintLayerContents(context, paintingInfo, localPaintFlags, fragmentPolicy) == MayBeClippedByPaintDirtyRect)
        result = MayBeClippedByPaintDirtyRect;

    return result;
}

class ClipPathHelper {
public:
    ClipPathHelper(GraphicsContext& context, const PaintLayer& paintLayer, PaintLayerPaintingInfo& paintingInfo, LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed,
        const LayoutPoint& offsetFromRoot, PaintLayerFlags paintFlags)
        : m_resourceClipper(0), m_paintLayer(paintLayer), m_context(context)
    {
        const ComputedStyle& style = paintLayer.layoutObject()->styleRef();

        // Clip-path, like border radius, must not be applied to the contents of a composited-scrolling container.
        // It must, however, still be applied to the mask layer, so that the compositor can properly mask the
        // scrolling contents and scrollbars.
        if (!paintLayer.layoutObject()->hasClipPath() || (paintLayer.needsCompositedScrolling() && !(paintFlags & PaintLayerPaintingChildClippingMaskPhase)))
            return;

        m_clipperState = SVGClipPainter::ClipperNotApplied;

        paintingInfo.ancestorHasClipPathClipping = true;

        ASSERT(style.clipPath());
        if (style.clipPath()->type() == ClipPathOperation::SHAPE) {
            ShapeClipPathOperation* clipPath = toShapeClipPathOperation(style.clipPath());
            if (clipPath->isValid()) {
                if (!rootRelativeBoundsComputed) {
                    rootRelativeBounds = paintLayer.physicalBoundingBoxIncludingReflectionAndStackingChildren(offsetFromRoot);
                    rootRelativeBoundsComputed = true;
                }
                m_clipPathRecorder.emplace(context, *paintLayer.layoutObject(), clipPath->path(FloatRect(rootRelativeBounds)));
            }
        } else if (style.clipPath()->type() == ClipPathOperation::REFERENCE) {
            ReferenceClipPathOperation* referenceClipPathOperation = toReferenceClipPathOperation(style.clipPath());
            Document& document = paintLayer.layoutObject()->document();
            // FIXME: It doesn't work with forward or external SVG references (https://bugs.webkit.org/show_bug.cgi?id=90405)
            Element* element = document.getElementById(referenceClipPathOperation->fragment());
            if (isSVGClipPathElement(element) && element->layoutObject()) {
                if (!rootRelativeBoundsComputed) {
                    rootRelativeBounds = paintLayer.physicalBoundingBoxIncludingReflectionAndStackingChildren(offsetFromRoot);
                    rootRelativeBoundsComputed = true;
                }

                m_resourceClipper = toLayoutSVGResourceClipper(toLayoutSVGResourceContainer(element->layoutObject()));
                if (!SVGClipPainter(*m_resourceClipper).prepareEffect(*paintLayer.layoutObject(), FloatRect(rootRelativeBounds),
                    FloatRect(rootRelativeBounds), context, m_clipperState)) {
                    // No need to post-apply the clipper if this failed.
                    m_resourceClipper = 0;
                }
            }
        }
    }

    ~ClipPathHelper()
    {
        if (m_resourceClipper)
            SVGClipPainter(*m_resourceClipper).finishEffect(*m_paintLayer.layoutObject(), m_context, m_clipperState);
    }
private:
    LayoutSVGResourceClipper* m_resourceClipper;
    Optional<ClipPathRecorder> m_clipPathRecorder;
    SVGClipPainter::ClipperState m_clipperState;
    const PaintLayer& m_paintLayer;
    GraphicsContext& m_context;
};

static bool shouldCreateSubsequence(const PaintLayer& paintLayer, GraphicsContext& context, const PaintLayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    // Caching is not needed during printing.
    if (context.printing())
        return false;

    // Don't create subsequence for a composited layer because if it can be cached,
    // we can skip the whole painting in GraphicsLayer::paint() with CachedDisplayItemList.
    // This also avoids conflict of PaintLayer::previousXXX() when paintLayer is composited
    // scrolling and is painted twice for GraphicsLayers of container and scrolling contents.
    if (paintLayer.compositingState() == PaintsIntoOwnBacking)
        return false;

    // Don't create subsequence during special painting to avoid cache conflict with normal painting.
    if (paintingInfo.globalPaintFlags() & GlobalPaintFlattenCompositingLayers)
        return false;
    if (paintFlags & (PaintLayerPaintingReflection | PaintLayerPaintingRootBackgroundOnly | PaintLayerPaintingOverlayScrollbars | PaintLayerUncachedClipRects))
        return false;

    // Create subsequence for only stacking contexts whose painting are atomic.
    if (!paintLayer.stackingNode()->isStackingContext())
        return false;

    // The layer doesn't have children. Subsequence caching is not worth because normally the actual painting will be cheap.
    if (!PaintLayerStackingNodeIterator(*paintLayer.stackingNode(), AllChildren).next())
        return false;

    return true;
}

static bool shouldRepaintSubsequence(PaintLayer& paintLayer, const PaintLayerPaintingInfo& paintingInfo, ShouldRespectOverflowClip respectOverflowClip, const LayoutSize& subpixelAccumulation)
{
    bool needsRepaint = false;

    // Repaint subsequence if the layer is marked for needing repaint.
    if (paintLayer.needsRepaint())
        needsRepaint = true;

    // Repaint if layer's clip changes.
    ClipRects& clipRects = paintLayer.clipper().paintingClipRects(paintingInfo.rootLayer, respectOverflowClip, subpixelAccumulation);
    ClipRects* previousClipRects = paintLayer.previousPaintingClipRects();
    if (!needsRepaint && &clipRects != previousClipRects && (!previousClipRects || clipRects != *previousClipRects))
        needsRepaint = true;
    paintLayer.setPreviousPaintingClipRects(clipRects);

    // Repaint if previously the layer might be clipped by paintDirtyRect and paintDirtyRect changes.
    if (!needsRepaint && paintLayer.previousPaintResult() == PaintLayerPainter::MayBeClippedByPaintDirtyRect && paintLayer.previousPaintDirtyRect() != paintingInfo.paintDirtyRect)
        needsRepaint = true;
    paintLayer.setPreviousPaintDirtyRect(paintingInfo.paintDirtyRect);

    // Repaint if scroll offset accumulation changes.
    if (!needsRepaint && paintingInfo.scrollOffsetAccumulation != paintLayer.previousScrollOffsetAccumulationForPainting())
        needsRepaint = true;
    paintLayer.setPreviousScrollOffsetAccumulationForPainting(paintingInfo.scrollOffsetAccumulation);

    return needsRepaint;
}

PaintLayerPainter::PaintResult PaintLayerPainter::paintLayerContents(GraphicsContext& context, const PaintLayerPaintingInfo& paintingInfoArg, PaintLayerFlags paintFlags, FragmentPolicy fragmentPolicy)
{
    ASSERT(m_paintLayer.isSelfPaintingLayer() || m_paintLayer.hasSelfPaintingLayerDescendant());
    ASSERT(!(paintFlags & PaintLayerAppliedTransform));

    bool isSelfPaintingLayer = m_paintLayer.isSelfPaintingLayer();
    bool isPaintingOverlayScrollbars = paintFlags & PaintLayerPaintingOverlayScrollbars;
    bool isPaintingScrollingContent = paintFlags & PaintLayerPaintingCompositingScrollingPhase;
    bool isPaintingCompositedForeground = paintFlags & PaintLayerPaintingCompositingForegroundPhase;
    bool isPaintingCompositedBackground = paintFlags & PaintLayerPaintingCompositingBackgroundPhase;
    bool isPaintingOverflowContents = paintFlags & PaintLayerPaintingOverflowContents;
    // Outline always needs to be painted even if we have no visible content. Also,
    // the outline is painted in the background phase during composited scrolling.
    // If it were painted in the foreground phase, it would move with the scrolled
    // content. When not composited scrolling, the outline is painted in the
    // foreground phase. Since scrolled contents are moved by paint invalidation in this
    // case, the outline won't get 'dragged along'.
    bool shouldPaintSelfOutline = isSelfPaintingLayer && !isPaintingOverlayScrollbars
        && ((isPaintingScrollingContent && isPaintingCompositedBackground)
            || (!isPaintingScrollingContent && isPaintingCompositedForeground))
        && m_paintLayer.layoutObject()->styleRef().hasOutline();
    bool shouldPaintContent = m_paintLayer.hasVisibleContent() && isSelfPaintingLayer && !isPaintingOverlayScrollbars;

    PaintResult result = FullyPainted;

    if (paintFlags & PaintLayerPaintingRootBackgroundOnly && !m_paintLayer.layoutObject()->isLayoutView() && !m_paintLayer.layoutObject()->isDocumentElement())
        return result;

    if (m_paintLayer.layoutObject()->isLayoutView() && toLayoutView(m_paintLayer.layoutObject())->frameView()->shouldThrottleRendering())
        return result;

    // Ensure our lists are up-to-date.
    m_paintLayer.stackingNode()->updateLayerListsIfNeeded();

    LayoutSize subpixelAccumulation = m_paintLayer.compositingState() == PaintsIntoOwnBacking ? m_paintLayer.subpixelAccumulation() : paintingInfoArg.subPixelAccumulation;
    ShouldRespectOverflowClip respectOverflowClip = shouldRespectOverflowClip(paintFlags, m_paintLayer.layoutObject());

    Optional<SubsequenceRecorder> subsequenceRecorder;
    if (shouldCreateSubsequence(m_paintLayer, context, paintingInfoArg, paintFlags)) {
        if (!shouldRepaintSubsequence(m_paintLayer, paintingInfoArg, respectOverflowClip, subpixelAccumulation)
            && SubsequenceRecorder::useCachedSubsequenceIfPossible(context, m_paintLayer))
            return result;
        subsequenceRecorder.emplace(context, m_paintLayer);
    }

    PaintLayerPaintingInfo paintingInfo = paintingInfoArg;

    LayoutPoint offsetFromRoot;
    m_paintLayer.convertToLayerCoords(paintingInfo.rootLayer, offsetFromRoot);
    offsetFromRoot.move(subpixelAccumulation);

    LayoutRect bounds = m_paintLayer.physicalBoundingBox(offsetFromRoot);
    if (!paintingInfo.paintDirtyRect.contains(bounds))
        result = MayBeClippedByPaintDirtyRect;

    LayoutRect rootRelativeBounds;
    bool rootRelativeBoundsComputed = false;

    if (paintingInfo.ancestorHasClipPathClipping && m_paintLayer.layoutObject()->isPositioned())
        UseCounter::count(m_paintLayer.layoutObject()->document(), UseCounter::ClipPathOfPositionedElement);

    // These helpers output clip and compositing operations using a RAII pattern. Stack-allocated-varibles are destructed in the reverse order of construction,
    // so they are nested properly.
    ClipPathHelper clipPathHelper(context, m_paintLayer, paintingInfo, rootRelativeBounds, rootRelativeBoundsComputed, offsetFromRoot, paintFlags);

    Optional<CompositingRecorder> compositingRecorder;
    // Blending operations must be performed only with the nearest ancestor stacking context.
    // Note that there is no need to composite if we're painting the root.
    // FIXME: this should be unified further into PaintLayer::paintsWithTransparency().
    bool shouldCompositeForBlendMode = (!m_paintLayer.layoutObject()->isDocumentElement() || m_paintLayer.layoutObject()->isSVGRoot()) && m_paintLayer.stackingNode()->isStackingContext() && m_paintLayer.hasNonIsolatedDescendantWithBlendMode();
    if (shouldCompositeForBlendMode || m_paintLayer.paintsWithTransparency(paintingInfo.globalPaintFlags())) {
        FloatRect compositingBounds = FloatRect(m_paintLayer.paintingExtent(paintingInfo.rootLayer, paintingInfo.subPixelAccumulation, paintingInfo.globalPaintFlags()));
        compositingRecorder.emplace(context, *m_paintLayer.layoutObject(),
            WebCoreCompositeToSkiaComposite(CompositeSourceOver, m_paintLayer.layoutObject()->style()->blendMode()),
            m_paintLayer.layoutObject()->opacity(), &compositingBounds);
    }

    PaintLayerPaintingInfo localPaintingInfo(paintingInfo);
    localPaintingInfo.subPixelAccumulation = subpixelAccumulation;

    PaintLayerFragments layerFragments;
    if (shouldPaintContent || shouldPaintSelfOutline || isPaintingOverlayScrollbars) {
        // Collect the fragments. This will compute the clip rectangles and paint offsets for each layer fragment.
        ClipRectsCacheSlot cacheSlot = (paintFlags & PaintLayerUncachedClipRects) ? UncachedClipRects : PaintingClipRects;
        if (fragmentPolicy == ForceSingleFragment)
            m_paintLayer.appendSingleFragmentIgnoringPagination(layerFragments, localPaintingInfo.rootLayer, localPaintingInfo.paintDirtyRect, cacheSlot, IgnoreOverlayScrollbarSize, respectOverflowClip, &offsetFromRoot, localPaintingInfo.subPixelAccumulation);
        else
            m_paintLayer.collectFragments(layerFragments, localPaintingInfo.rootLayer, localPaintingInfo.paintDirtyRect, cacheSlot, IgnoreOverlayScrollbarSize, respectOverflowClip, &offsetFromRoot, localPaintingInfo.subPixelAccumulation);
        if (shouldPaintContent) {
            // TODO(wangxianzhu): This is for old slow scrolling. Implement similar optimization for slimming paint v2.
            shouldPaintContent = atLeastOneFragmentIntersectsDamageRect(layerFragments, localPaintingInfo, paintFlags, offsetFromRoot);
            if (!shouldPaintContent)
                result = MayBeClippedByPaintDirtyRect;
        }
    }

    bool selectionOnly = localPaintingInfo.globalPaintFlags() & GlobalPaintSelectionOnly;

    { // Begin block for the lifetime of any filter.
        FilterPainter filterPainter(m_paintLayer, context, offsetFromRoot, layerFragments.isEmpty() ? ClipRect() : layerFragments[0].backgroundRect, localPaintingInfo, paintFlags,
            rootRelativeBounds, rootRelativeBoundsComputed);

        Optional<ScopedPaintChunkProperties> scopedPaintChunkProperties;
        if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
            if (const auto* objectProperties = m_paintLayer.layoutObject()->objectPaintProperties()) {
                PaintChunkProperties properties(context.paintController().currentPaintChunkProperties());
                if (TransformPaintPropertyNode* transform = objectProperties->transformForLayerContents())
                    properties.transform = transform;
                if (EffectPaintPropertyNode* effect = objectProperties->effect())
                    properties.effect = effect;
                scopedPaintChunkProperties.emplace(context.paintController(), properties);
            }
        }

        bool shouldPaintBackground = isPaintingCompositedBackground && shouldPaintContent && !selectionOnly;
        bool shouldPaintNegZOrderList = (isPaintingScrollingContent && isPaintingOverflowContents) || (!isPaintingScrollingContent && isPaintingCompositedBackground);
        bool shouldPaintOwnContents = isPaintingCompositedForeground && shouldPaintContent;
        bool shouldPaintNormalFlowAndPosZOrderLists = isPaintingCompositedForeground;
        bool shouldPaintOverlayScrollbars = isPaintingOverlayScrollbars;

        if (shouldPaintBackground) {
            paintBackgroundForFragments(layerFragments, context, paintingInfo.paintDirtyRect,
                localPaintingInfo, paintFlags);
        }

        if (shouldPaintNegZOrderList) {
            if (paintChildren(NegativeZOrderChildren, context, paintingInfo, paintFlags) == MayBeClippedByPaintDirtyRect)
                result = MayBeClippedByPaintDirtyRect;
        }

        if (shouldPaintOwnContents) {
            paintForegroundForFragments(layerFragments, context, paintingInfo.paintDirtyRect,
                localPaintingInfo, selectionOnly, paintFlags);
        }

        if (shouldPaintSelfOutline)
            paintSelfOutlineForFragments(layerFragments, context, localPaintingInfo, paintFlags);

        if (shouldPaintNormalFlowAndPosZOrderLists) {
            if (paintChildren(NormalFlowChildren | PositiveZOrderChildren, context, paintingInfo, paintFlags) == MayBeClippedByPaintDirtyRect)
                result = MayBeClippedByPaintDirtyRect;
        }

        if (shouldPaintOverlayScrollbars)
            paintOverflowControlsForFragments(layerFragments, context, localPaintingInfo, paintFlags);
    } // FilterPainter block

    bool shouldPaintMask = (paintFlags & PaintLayerPaintingCompositingMaskPhase) && shouldPaintContent && m_paintLayer.layoutObject()->hasMask() && !selectionOnly;
    bool shouldPaintClippingMask = (paintFlags & PaintLayerPaintingChildClippingMaskPhase) && shouldPaintContent && !selectionOnly;

    if (shouldPaintMask)
        paintMaskForFragments(layerFragments, context, localPaintingInfo, paintFlags);
    if (shouldPaintClippingMask) {
        // Paint the border radius mask for the fragments.
        paintChildClippingMaskForFragments(layerFragments, context, localPaintingInfo, paintFlags);
    }

    if (subsequenceRecorder)
        m_paintLayer.setPreviousPaintResult(result);
    return result;
}

bool PaintLayerPainter::needsToClip(const PaintLayerPaintingInfo& localPaintingInfo, const ClipRect& clipRect)
{
    return clipRect.rect() != localPaintingInfo.paintDirtyRect || clipRect.hasRadius();
}

bool PaintLayerPainter::atLeastOneFragmentIntersectsDamageRect(PaintLayerFragments& fragments, const PaintLayerPaintingInfo& localPaintingInfo, PaintLayerFlags localPaintFlags, const LayoutPoint& offsetFromRoot)
{
    if (m_paintLayer.enclosingPaginationLayer())
        return true; // The fragments created have already been found to intersect with the damage rect.

    if (&m_paintLayer == localPaintingInfo.rootLayer && (localPaintFlags & PaintLayerPaintingOverflowContents))
        return true;

    for (PaintLayerFragment& fragment: fragments) {
        LayoutPoint newOffsetFromRoot = offsetFromRoot + fragment.paginationOffset;
        // Note that this really only works reliably on the first fragment. If the layer has visible
        // overflow and a subsequent fragment doesn't intersect with the border box of the layer
        // (i.e. only contains an overflow portion of the layer), intersection will fail. The reason
        // for this is that fragment.layerBounds is set to the border box, not the bounding box, of
        // the layer.
        if (m_paintLayer.intersectsDamageRect(fragment.layerBounds, fragment.backgroundRect.rect(), newOffsetFromRoot))
            return true;
    }
    return false;
}

PaintLayerPainter::PaintResult PaintLayerPainter::paintLayerWithTransform(GraphicsContext& context, const PaintLayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    TransformationMatrix layerTransform = m_paintLayer.renderableTransform(paintingInfo.globalPaintFlags());
    // If the transform can't be inverted, then don't paint anything.
    if (!layerTransform.isInvertible())
        return FullyPainted;

    // FIXME: We should make sure that we don't walk past paintingInfo.rootLayer here.
    // m_paintLayer may be the "root", and then we should avoid looking at its parent.
    PaintLayer* parentLayer = m_paintLayer.parent();

    ClipRect ancestorBackgroundClipRect;
    if (parentLayer) {
        // Calculate the clip rectangle that the ancestors establish.
        ClipRectsContext clipRectsContext(paintingInfo.rootLayer, (paintFlags & PaintLayerUncachedClipRects) ? UncachedClipRects : PaintingClipRects, IgnoreOverlayScrollbarSize);
        if (shouldRespectOverflowClip(paintFlags, m_paintLayer.layoutObject()) == IgnoreOverflowClip)
            clipRectsContext.setIgnoreOverflowClip();
        ancestorBackgroundClipRect = m_paintLayer.clipper().backgroundClipRect(clipRectsContext);
    }

    PaintLayer* paginationLayer = m_paintLayer.enclosingPaginationLayer();
    PaintLayerFragments fragments;
    if (paginationLayer) {
        // FIXME: This is a mess. Look closely at this code and the code in Layer and fix any
        // issues in it & refactor to make it obvious from code structure what it does and that it's
        // correct.
        ClipRectsCacheSlot cacheSlot = (paintFlags & PaintLayerUncachedClipRects) ? UncachedClipRects : PaintingClipRects;
        ShouldRespectOverflowClip respectOverflowClip = shouldRespectOverflowClip(paintFlags, m_paintLayer.layoutObject());
        // Calculate the transformed bounding box in the current coordinate space, to figure out
        // which fragmentainers (e.g. columns) we need to visit.
        LayoutRect transformedExtent = PaintLayer::transparencyClipBox(&m_paintLayer, paginationLayer, PaintLayer::PaintingTransparencyClipBox, PaintLayer::RootOfTransparencyClipBox, paintingInfo.subPixelAccumulation, paintingInfo.globalPaintFlags());
        // FIXME: we don't check if paginationLayer is within paintingInfo.rootLayer here.
        paginationLayer->collectFragments(fragments, paintingInfo.rootLayer, paintingInfo.paintDirtyRect, cacheSlot, IgnoreOverlayScrollbarSize, respectOverflowClip, 0, paintingInfo.subPixelAccumulation, &transformedExtent);
    } else {
        // We don't need to collect any fragments in the regular way here. We have already
        // calculated a clip rectangle for the ancestry if it was needed, and clipping this
        // layer is something that can be done further down the path, when the transform has
        // been applied.
        PaintLayerFragment fragment;
        fragment.backgroundRect = paintingInfo.paintDirtyRect;
        fragments.append(fragment);
    }

    bool needsScope = fragments.size() > 1;
    PaintResult result = FullyPainted;
    for (const auto& fragment : fragments) {
        Optional<ScopeRecorder> scopeRecorder;
        if (needsScope)
            scopeRecorder.emplace(context);
        Optional<LayerClipRecorder> clipRecorder;
        if (parentLayer) {
            ClipRect clipRectForFragment(ancestorBackgroundClipRect);
            clipRectForFragment.moveBy(fragment.paginationOffset);
            clipRectForFragment.intersect(fragment.backgroundRect);
            if (clipRectForFragment.isEmpty())
                continue;
            if (needsToClip(paintingInfo, clipRectForFragment)) {
                if (m_paintLayer.layoutObject()->isPositioned() && clipRectForFragment.isClippedByClipCss())
                    UseCounter::count(m_paintLayer.layoutObject()->document(), UseCounter::ClipCssOfPositionedElement);
                clipRecorder.emplace(context, *parentLayer->layoutObject(), DisplayItem::ClipLayerParent, clipRectForFragment, &paintingInfo, fragment.paginationOffset, paintFlags);
            }
        }
        if (paintFragmentByApplyingTransform(context, paintingInfo, paintFlags, fragment.paginationOffset) == MayBeClippedByPaintDirtyRect)
            result = MayBeClippedByPaintDirtyRect;
    }
    return result;
}

PaintLayerPainter::PaintResult PaintLayerPainter::paintFragmentByApplyingTransform(GraphicsContext& context, const PaintLayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags, const LayoutPoint& fragmentTranslation)
{
    // This involves subtracting out the position of the layer in our current coordinate space, but preserving
    // the accumulated error for sub-pixel layout.
    LayoutPoint delta;
    m_paintLayer.convertToLayerCoords(paintingInfo.rootLayer, delta);
    delta.moveBy(fragmentTranslation);
    TransformationMatrix transform(m_paintLayer.renderableTransform(paintingInfo.globalPaintFlags()));
    IntPoint roundedDelta = roundedIntPoint(delta);
    transform.translateRight(roundedDelta.x(), roundedDelta.y());
    LayoutSize adjustedSubPixelAccumulation = paintingInfo.subPixelAccumulation + (delta - roundedDelta);

    // TODO(jbroman): Put the real transform origin here, instead of using a
    // matrix with the origin baked in.
    FloatPoint3D transformOrigin;
    Transform3DRecorder transform3DRecorder(context, *m_paintLayer.layoutObject(), DisplayItem::Transform3DElementTransform, transform, transformOrigin);

    // Now do a paint with the root layer shifted to be us.
    PaintLayerPaintingInfo transformedPaintingInfo(&m_paintLayer, LayoutRect(enclosingIntRect(transform.inverse().mapRect(paintingInfo.paintDirtyRect))), paintingInfo.globalPaintFlags(),
        adjustedSubPixelAccumulation);
    transformedPaintingInfo.ancestorHasClipPathClipping = paintingInfo.ancestorHasClipPathClipping;
    return paintLayerContentsAndReflection(context, transformedPaintingInfo, paintFlags, ForceSingleFragment);
}

PaintLayerPainter::PaintResult PaintLayerPainter::paintChildren(unsigned childrenToVisit, GraphicsContext& context, const PaintLayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
{
    PaintResult result = FullyPainted;
    if (!m_paintLayer.hasSelfPaintingLayerDescendant())
        return result;

#if ENABLE(ASSERT)
    LayerListMutationDetector mutationChecker(m_paintLayer.stackingNode());
#endif

    PaintLayerStackingNodeIterator iterator(*m_paintLayer.stackingNode(), childrenToVisit);
    PaintLayerStackingNode* child = iterator.next();
    if (!child)
        return result;

    IntSize scrollOffsetAccumulationForChildren = paintingInfo.scrollOffsetAccumulation;
    if (m_paintLayer.layoutObject()->hasOverflowClip())
        scrollOffsetAccumulationForChildren += m_paintLayer.layoutBox()->scrolledContentOffset();

    for (; child; child = iterator.next()) {
        PaintLayerPainter childPainter(*child->layer());
        // If this Layer should paint into its own backing or a grouped backing, that will be done via CompositedLayerMapping::paintContents()
        // and CompositedLayerMapping::doPaintTask().
        if (!childPainter.shouldPaintLayerInSoftwareMode(paintingInfo.globalPaintFlags(), paintFlags))
            continue;

        PaintLayerPaintingInfo childPaintingInfo = paintingInfo;
        childPaintingInfo.scrollOffsetAccumulation = scrollOffsetAccumulationForChildren;
        // Rare case: accumulate scroll offset of non-stacking-context ancestors up to m_paintLayer.
        for (PaintLayer* parentLayer = child->layer()->parent(); parentLayer != &m_paintLayer; parentLayer = parentLayer->parent()) {
            if (parentLayer->layoutObject()->hasOverflowClip())
                childPaintingInfo.scrollOffsetAccumulation += parentLayer->layoutBox()->scrolledContentOffset();
        }

        if (childPainter.paintLayer(context, childPaintingInfo, paintFlags) == MayBeClippedByPaintDirtyRect)
            result = MayBeClippedByPaintDirtyRect;
    }

    return result;
}

// FIXME: inline this.
static bool paintForFixedRootBackground(const PaintLayer* layer, PaintLayerFlags paintFlags)
{
    return layer->layoutObject()->isDocumentElement() && (paintFlags & PaintLayerPaintingRootBackgroundOnly);
}

bool PaintLayerPainter::shouldPaintLayerInSoftwareMode(const GlobalPaintFlags globalPaintFlags, PaintLayerFlags paintFlags)
{
    DisableCompositingQueryAsserts disabler;

    return m_paintLayer.compositingState() == NotComposited
        || (globalPaintFlags & GlobalPaintFlattenCompositingLayers)
        || ((paintFlags & PaintLayerPaintingReflection) && !m_paintLayer.has3DTransform())
        || paintForFixedRootBackground(&m_paintLayer, paintFlags);
}

void PaintLayerPainter::paintOverflowControlsForFragments(const PaintLayerFragments& layerFragments, GraphicsContext& context, const PaintLayerPaintingInfo& localPaintingInfo, PaintLayerFlags paintFlags)
{
    bool needsScope = layerFragments.size() > 1;
    for (auto& fragment : layerFragments) {
        Optional<ScopeRecorder> scopeRecorder;
        if (needsScope)
            scopeRecorder.emplace(context);

        Optional<LayerClipRecorder> clipRecorder;

        if (needsToClip(localPaintingInfo, fragment.backgroundRect))
            clipRecorder.emplace(context, *m_paintLayer.layoutObject(), DisplayItem::ClipLayerOverflowControls, fragment.backgroundRect, &localPaintingInfo, fragment.paginationOffset, paintFlags);
        if (PaintLayerScrollableArea* scrollableArea = m_paintLayer.scrollableArea()) {
            CullRect cullRect(pixelSnappedIntRect(fragment.backgroundRect.rect()));
            ScrollableAreaPainter(*scrollableArea).paintOverflowControls(context, roundedIntPoint(toPoint(fragment.layerBounds.location() - m_paintLayer.layoutBoxLocation())), cullRect, true);
        }
    }
}

void PaintLayerPainter::paintFragmentWithPhase(PaintPhase phase, const PaintLayerFragment& fragment, GraphicsContext& context, const ClipRect& clipRect, const PaintLayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags, ClipState clipState)
{
    ASSERT(m_paintLayer.isSelfPaintingLayer());

    Optional<LayerClipRecorder> clipRecorder;
    if (clipState != HasClipped && paintingInfo.clipToDirtyRect && needsToClip(paintingInfo, clipRect)) {
        DisplayItem::Type clipType = DisplayItem::paintPhaseToClipLayerFragmentType(phase);
        LayerClipRecorder::BorderRadiusClippingRule clippingRule;
        switch (phase) {
        case PaintPhaseSelfBlockBackgroundOnly: // Background painting will handle clipping to self.
        case PaintPhaseSelfOutlineOnly:
        case PaintPhaseMask: // Mask painting will handle clipping to self.
            clippingRule = LayerClipRecorder::DoNotIncludeSelfForBorderRadius;
            break;
        default:
            clippingRule = LayerClipRecorder::IncludeSelfForBorderRadius;
            break;
        }

        clipRecorder.emplace(context, *m_paintLayer.layoutObject(), clipType, clipRect, &paintingInfo, fragment.paginationOffset, paintFlags, clippingRule);
    }

    LayoutRect newCullRect(clipRect.rect());
    Optional<ScrollRecorder> scrollRecorder;
    LayoutPoint paintOffset = toPoint(fragment.layerBounds.location() - m_paintLayer.layoutBoxLocation());
    if (!paintingInfo.scrollOffsetAccumulation.isZero()) {
        // As a descendant of the root layer, m_paintLayer's painting is not controlled by the ScrollRecorders
        // created by BlockPainter of the ancestor layers up to the root layer, so we need to issue ScrollRecorder
        // for this layer seperately, with the scroll offset accumulated from the root layer to the parent of this
        // layer, to get the same result as ScrollRecorder in BlockPainter.
        paintOffset += paintingInfo.scrollOffsetAccumulation;

        newCullRect.move(paintingInfo.scrollOffsetAccumulation);
        scrollRecorder.emplace(context, *m_paintLayer.layoutObject(), phase, paintingInfo.scrollOffsetAccumulation);
    }
    PaintInfo paintInfo(context, pixelSnappedIntRect(newCullRect), phase,
        paintingInfo.globalPaintFlags(), paintFlags, paintingInfo.rootLayer->layoutObject());

    m_paintLayer.layoutObject()->paint(paintInfo, paintOffset);
}

void PaintLayerPainter::paintBackgroundForFragments(const PaintLayerFragments& layerFragments,
    GraphicsContext& context, const LayoutRect& transparencyPaintDirtyRect,
    const PaintLayerPaintingInfo& localPaintingInfo, PaintLayerFlags paintFlags)
{
    bool needsScope = layerFragments.size() > 1;
    for (auto& fragment : layerFragments) {
        Optional<ScopeRecorder> scopeRecorder;
        if (needsScope)
            scopeRecorder.emplace(context);
        paintFragmentWithPhase(PaintPhaseSelfBlockBackgroundOnly, fragment, context, fragment.backgroundRect, localPaintingInfo, paintFlags, HasNotClipped);
    }
}

void PaintLayerPainter::paintForegroundForFragments(const PaintLayerFragments& layerFragments,
    GraphicsContext& context, const LayoutRect& transparencyPaintDirtyRect,
    const PaintLayerPaintingInfo& localPaintingInfo, bool selectionOnly, PaintLayerFlags paintFlags)
{
    // Optimize clipping for the single fragment case.
    bool shouldClip = localPaintingInfo.clipToDirtyRect && layerFragments.size() == 1 && !layerFragments[0].foregroundRect.isEmpty();
    ClipState clipState = HasNotClipped;
    Optional<LayerClipRecorder> clipRecorder;
    if (shouldClip && needsToClip(localPaintingInfo, layerFragments[0].foregroundRect)) {
        clipRecorder.emplace(context, *m_paintLayer.layoutObject(), DisplayItem::ClipLayerForeground, layerFragments[0].foregroundRect, &localPaintingInfo, layerFragments[0].paginationOffset, paintFlags);
        clipState = HasClipped;
    }

    // We have to loop through every fragment multiple times, since we have to issue paint invalidations in each specific phase in order for
    // interleaving of the fragments to work properly.
    paintForegroundForFragmentsWithPhase(selectionOnly ? PaintPhaseSelection : PaintPhaseDescendantBlockBackgroundsOnly,
        layerFragments, context, localPaintingInfo, paintFlags, clipState);

    if (!selectionOnly) {
        if (m_paintLayer.needsPaintPhaseFloat())
            paintForegroundForFragmentsWithPhase(PaintPhaseFloat, layerFragments, context, localPaintingInfo, paintFlags, clipState);
        paintForegroundForFragmentsWithPhase(PaintPhaseForeground, layerFragments, context, localPaintingInfo, paintFlags, clipState);
        if (m_paintLayer.needsPaintPhaseDescendantOutlines())
            paintForegroundForFragmentsWithPhase(PaintPhaseDescendantOutlinesOnly, layerFragments, context, localPaintingInfo, paintFlags, clipState);
    }
}

void PaintLayerPainter::paintForegroundForFragmentsWithPhase(PaintPhase phase,
    const PaintLayerFragments& layerFragments, GraphicsContext& context,
    const PaintLayerPaintingInfo& localPaintingInfo, PaintLayerFlags paintFlags, ClipState clipState)
{
    bool needsScope = layerFragments.size() > 1;
    for (auto& fragment : layerFragments) {
        if (!fragment.foregroundRect.isEmpty()) {
            Optional<ScopeRecorder> scopeRecorder;
            if (needsScope)
                scopeRecorder.emplace(context);
            paintFragmentWithPhase(phase, fragment, context, fragment.foregroundRect, localPaintingInfo, paintFlags, clipState);
        }
    }
}

void PaintLayerPainter::paintSelfOutlineForFragments(const PaintLayerFragments& layerFragments,
    GraphicsContext& context, const PaintLayerPaintingInfo& localPaintingInfo, PaintLayerFlags paintFlags)
{
    bool needsScope = layerFragments.size() > 1;
    for (auto& fragment : layerFragments) {
        if (!fragment.backgroundRect.isEmpty()) {
            Optional<ScopeRecorder> scopeRecorder;
            if (needsScope)
                scopeRecorder.emplace(context);
            paintFragmentWithPhase(PaintPhaseSelfOutlineOnly, fragment, context, fragment.backgroundRect, localPaintingInfo, paintFlags, HasNotClipped);
        }
    }
}

void PaintLayerPainter::paintMaskForFragments(const PaintLayerFragments& layerFragments,
    GraphicsContext& context, const PaintLayerPaintingInfo& localPaintingInfo, PaintLayerFlags paintFlags)
{
    bool needsScope = layerFragments.size() > 1;
    for (auto& fragment : layerFragments) {
        Optional<ScopeRecorder> scopeRecorder;
        if (needsScope)
            scopeRecorder.emplace(context);
        paintFragmentWithPhase(PaintPhaseMask, fragment, context, fragment.backgroundRect, localPaintingInfo, paintFlags, HasNotClipped);
    }
}

void PaintLayerPainter::paintChildClippingMaskForFragments(const PaintLayerFragments& layerFragments,
    GraphicsContext& context, const PaintLayerPaintingInfo& localPaintingInfo, PaintLayerFlags paintFlags)
{
    bool needsScope = layerFragments.size() > 1;
    for (auto& fragment: layerFragments) {
        Optional<ScopeRecorder> scopeRecorder;
        if (needsScope)
            scopeRecorder.emplace(context);
        paintFragmentWithPhase(PaintPhaseClippingMask, fragment, context, fragment.foregroundRect, localPaintingInfo, paintFlags, HasNotClipped);
    }
}

void PaintLayerPainter::paintOverlayScrollbars(GraphicsContext& context, const LayoutRect& damageRect, const GlobalPaintFlags paintFlags)
{
    if (!m_paintLayer.containsDirtyOverlayScrollbars())
        return;

    PaintLayerPaintingInfo paintingInfo(&m_paintLayer, LayoutRect(enclosingIntRect(damageRect)), paintFlags, LayoutSize());
    paintLayer(context, paintingInfo, PaintLayerPaintingOverlayScrollbars);

    m_paintLayer.setContainsDirtyOverlayScrollbars(false);
}

} // namespace blink
