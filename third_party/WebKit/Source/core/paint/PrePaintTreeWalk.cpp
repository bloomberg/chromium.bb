// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PrePaintTreeWalk.h"

#include "core/dom/DocumentLifecycle.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutMultiColumnSpannerPlaceholder.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"

namespace blink {

struct PrePaintTreeWalkContext {
  PrePaintTreeWalkContext(GeometryMapper& geometryMapper)
      : treeBuilderContext(
            WTF::wrapUnique(new PaintPropertyTreeBuilderContext)),
        paintInvalidatorContext(*treeBuilderContext, geometryMapper),
        ancestorOverflowPaintLayer(nullptr),
        ancestorTransformedOrRootPaintLayer(nullptr) {}
  PrePaintTreeWalkContext(const PrePaintTreeWalkContext& parentContext)
      : treeBuilderContext(WTF::wrapUnique(new PaintPropertyTreeBuilderContext(
            *parentContext.treeBuilderContext))),
        paintInvalidatorContext(*treeBuilderContext,
                                parentContext.paintInvalidatorContext),
        ancestorOverflowPaintLayer(parentContext.ancestorOverflowPaintLayer),
        ancestorTransformedOrRootPaintLayer(
            parentContext.ancestorTransformedOrRootPaintLayer) {}

  // PaintPropertyTreeBuilderContext is large and can lead to stack overflows
  // when recursion is deep so this context object is allocated on the heap.
  // See: https://crbug.com/698653.
  std::unique_ptr<PaintPropertyTreeBuilderContext> treeBuilderContext;

  PaintInvalidatorContext paintInvalidatorContext;

  // The ancestor in the PaintLayer tree which has overflow clip, or
  // is the root layer. Note that it is tree ancestor, not containing
  // block or stacking ancestor.
  PaintLayer* ancestorOverflowPaintLayer;
  PaintLayer* ancestorTransformedOrRootPaintLayer;
};

void PrePaintTreeWalk::walk(FrameView& rootFrame) {
  DCHECK(rootFrame.frame().document()->lifecycle().state() ==
         DocumentLifecycle::InPrePaint);

  PrePaintTreeWalkContext initialContext(m_geometryMapper);
  m_propertyTreeBuilder.setupInitialContext(*initialContext.treeBuilderContext);
  initialContext.ancestorTransformedOrRootPaintLayer =
      rootFrame.layoutView()->layer();

  // GeometryMapper depends on paint properties.
  if (rootFrame.needsPaintPropertyUpdate() ||
      (rootFrame.layoutView() &&
       !shouldEndWalkBefore(*rootFrame.layoutView(), initialContext)))
    m_geometryMapper.clearCache();

  walk(rootFrame, initialContext);
  m_paintInvalidator.processPendingDelayedPaintInvalidations();
}

void PrePaintTreeWalk::walk(FrameView& frameView,
                            const PrePaintTreeWalkContext& parentContext) {
  if (frameView.shouldThrottleRendering()) {
    // Skip the throttled frame. Will update it when it becomes unthrottled.
    return;
  }

  PrePaintTreeWalkContext context(parentContext);
  // ancestorOverflowLayer does not cross frame boundaries.
  context.ancestorOverflowPaintLayer = nullptr;
  m_propertyTreeBuilder.updateProperties(frameView,
                                         *context.treeBuilderContext);
  m_paintInvalidator.invalidatePaintIfNeeded(frameView,
                                             context.paintInvalidatorContext);

  if (LayoutView* view = frameView.layoutView()) {
    walk(*view, context);
#if DCHECK_IS_ON()
    view->assertSubtreeClearedPaintInvalidationFlags();
#endif
  }
  frameView.clearNeedsPaintPropertyUpdate();
}

static void updateAuxiliaryObjectProperties(const LayoutObject& object,
                                            PrePaintTreeWalkContext& context) {
  if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  if (!object.hasLayer())
    return;

  PaintLayer* paintLayer = object.enclosingLayer();
  paintLayer->updateAncestorOverflowLayer(context.ancestorOverflowPaintLayer);

  if (object.styleRef().position() == EPosition::kSticky) {
    paintLayer->layoutObject().updateStickyPositionConstraints();

    // Sticky position constraints and ancestor overflow scroller affect the
    // sticky layer position, so we need to update it again here.
    // TODO(flackr): This should be refactored in the future to be clearer (i.e.
    // update layer position and ancestor inputs updates in the same walk).
    paintLayer->updateLayerPosition();
  }

  if (paintLayer->isRootLayer() || object.hasOverflowClip())
    context.ancestorOverflowPaintLayer = paintLayer;
}

// Returns whether |a| is an ancestor of or equal to |b|.
static bool isAncestorOfOrEqualTo(const ClipPaintPropertyNode* a,
                                  const ClipPaintPropertyNode* b) {
  while (b && b != a) {
    b = b->parent();
  }
  return b == a;
}

void PrePaintTreeWalk::computeClipRectForContext(
    const PaintPropertyTreeBuilderContext::ContainingBlockContext& context,
    const EffectPaintPropertyNode* effect,
    const PropertyTreeState& ancestorState,
    const LayoutPoint& ancestorPaintOffset,
    bool& hasClip,
    FloatClipRect& clipRect) {
  // Only return a non-infinite clip if clips differ, or the "ancestor" state is
  // actually an ancestor clip. This ensures no accuracy issues due to
  // transforms applied to infinite rects.
  if (isAncestorOfOrEqualTo(context.clip, ancestorState.clip()))
    clipRect = FloatClipRect();

  hasClip = true;
  PropertyTreeState localState(context.transform, context.clip, effect);

  clipRect =
      m_geometryMapper.sourceToDestinationClipRect(localState, ancestorState);
  clipRect.moveBy(-FloatPoint(ancestorPaintOffset));
}

void PrePaintTreeWalk::invalidatePaintLayerOptimizationsIfNeeded(
    const LayoutObject& object,
    PrePaintTreeWalkContext& context) {
  if (!object.hasLayer())
    return;

  PaintLayer& paintLayer = *toLayoutBoxModelObject(object).layer();
  if (object.styleRef().hasTransform() ||
      &object == context.paintInvalidatorContext.paintInvalidationContainer) {
    context.ancestorTransformedOrRootPaintLayer = &paintLayer;
  }

  const ObjectPaintProperties& ancestorPaintProperties =
      *context.ancestorTransformedOrRootPaintLayer->layoutObject()
           .paintProperties();
  PropertyTreeState ancestorState =
      *ancestorPaintProperties.localBorderBoxProperties();

#ifdef CHECK_CLIP_RECTS
  ShouldRespectOverflowClipType respectOverflowClip = RespectOverflowClip;
#endif
  if (context.ancestorTransformedOrRootPaintLayer->compositingState() ==
          PaintsIntoOwnBacking &&
      ancestorPaintProperties.overflowClip()) {
    ancestorState.setClip(ancestorPaintProperties.overflowClip());
#ifdef CHECK_CLIP_RECTS
    respectOverflowClip = IgnoreOverflowClip;
#endif
  }

#ifdef CHECK_CLIP_RECTS
  ClipRects& oldClipRects =
      paintLayer.clipper(PaintLayer::DoNotUseGeometryMapper)
          .paintingClipRects(&ancestorTransformedOrRootPaintLayer,
                             respectOverflowClip, LayoutSize());
#endif

  bool hasClip = false;
  RefPtr<ClipRects> clipRects = ClipRects::create();
  const LayoutPoint& ancestorPaintOffset =
      context.ancestorTransformedOrRootPaintLayer->layoutObject().paintOffset();

  FloatClipRect clipRect;
  const EffectPaintPropertyNode* effect =
      context.treeBuilderContext->currentEffect;
  computeClipRectForContext(context.treeBuilderContext->current, effect,
                            ancestorState, ancestorPaintOffset, hasClip,
                            clipRect);
  clipRects->setOverflowClipRect(clipRect);
#ifdef CHECK_CLIP_RECTS
  CHECK(!hasClip ||
        clipRects->overflowClipRect() == oldClipRects.overflowClipRect())
      << "rect= " << clipRects->overflowClipRect().toString();
#endif

  computeClipRectForContext(context.treeBuilderContext->fixedPosition, effect,
                            ancestorState, ancestorPaintOffset, hasClip,
                            clipRect);
  clipRects->setFixedClipRect(clipRect);
#ifdef CHECK_CLIP_RECTS
  CHECK(hasClip || clipRects->fixedClipRect() == oldClipRects.fixedClipRect())
      << " fixed=" << clipRects->fixedClipRect().toString();
#endif

  computeClipRectForContext(context.treeBuilderContext->absolutePosition,
                            effect, ancestorState, ancestorPaintOffset, hasClip,
                            clipRect);
  clipRects->setPosClipRect(clipRect);
#ifdef CHECK_CLIP_RECTS
  CHECK(!hasClip || clipRects->posClipRect() == oldClipRects.posClipRect())
      << " abs=" << clipRects->posClipRect().toString();
#endif

  ClipRects* previousClipRects = paintLayer.previousPaintingClipRects();

  if (!previousClipRects || *clipRects != *previousClipRects) {
    paintLayer.setNeedsRepaint();
    paintLayer.setPreviousPaintPhaseDescendantOutlinesEmpty(false);
    paintLayer.setPreviousPaintPhaseFloatEmpty(false);
    paintLayer.setPreviousPaintPhaseDescendantBlockBackgroundsEmpty(false);
    // All subsequences which are contained below this paintLayer must also
    // be checked.
    context.paintInvalidatorContext.forcedSubtreeInvalidationFlags |=
        PaintInvalidatorContext::ForcedSubtreeInvalidationRectUpdate;
  }

  paintLayer.setPreviousPaintingClipRects(*clipRects);
}

bool PrePaintTreeWalk::shouldEndWalkBefore(
    const LayoutObject& object,
    const PrePaintTreeWalkContext& context) {
  return !object.needsPaintPropertyUpdate() &&
         !object.descendantNeedsPaintPropertyUpdate() &&
         !context.treeBuilderContext->forceSubtreeUpdate &&
         !context.paintInvalidatorContext.forcedSubtreeInvalidationFlags &&
         !object.shouldCheckForPaintInvalidation();
}

void PrePaintTreeWalk::walk(const LayoutObject& object,
                            const PrePaintTreeWalkContext& parentContext) {
  if (shouldEndWalkBefore(object, parentContext))
    return;

  PrePaintTreeWalkContext context(parentContext);

  // This must happen before updatePropertiesForSelf, because the latter reads
  // some of the state computed here.
  updateAuxiliaryObjectProperties(object, context);

  m_propertyTreeBuilder.updatePropertiesForSelf(object,
                                                *context.treeBuilderContext);
  m_paintInvalidator.invalidatePaintIfNeeded(object,
                                             context.paintInvalidatorContext);
  m_propertyTreeBuilder.updatePropertiesForChildren(
      object, *context.treeBuilderContext);

  invalidatePaintLayerOptimizationsIfNeeded(object, context);

  for (const LayoutObject* child = object.slowFirstChild(); child;
       child = child->nextSibling()) {
    if (child->isLayoutMultiColumnSpannerPlaceholder()) {
      child->getMutableForPainting().clearPaintFlags();
      continue;
    }
    walk(*child, context);
  }

  if (object.isLayoutPart()) {
    const LayoutPart& layoutPart = toLayoutPart(object);
    FrameViewBase* frameViewBase = layoutPart.frameViewBase();
    if (frameViewBase && frameViewBase->isFrameView()) {
      context.treeBuilderContext->current.paintOffset +=
          layoutPart.replacedContentRect().location() -
          frameViewBase->frameRect().location();
      context.treeBuilderContext->current.paintOffset =
          roundedIntPoint(context.treeBuilderContext->current.paintOffset);
      walk(*toFrameView(frameViewBase), context);
    }
    // TODO(pdr): Investigate RemoteFrameView (crbug.com/579281).
  }

  object.getMutableForPainting().clearPaintFlags();
}

}  // namespace blink
