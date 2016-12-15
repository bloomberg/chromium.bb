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
  PrePaintTreeWalkContext()
      : paintInvalidatorContext(treeBuilderContext),
        ancestorOverflowPaintLayer(nullptr) {}
  PrePaintTreeWalkContext(const PrePaintTreeWalkContext& parentContext)
      : treeBuilderContext(parentContext.treeBuilderContext),
        paintInvalidatorContext(treeBuilderContext,
                                parentContext.paintInvalidatorContext),
        ancestorOverflowPaintLayer(parentContext.ancestorOverflowPaintLayer) {}

  PaintPropertyTreeBuilderContext treeBuilderContext;
  PaintInvalidatorContext paintInvalidatorContext;

  // The ancestor in the PaintLayer tree which has overflow clip, or
  // is the root layer. Note that it is tree ancestor, not containing
  // block or stacking ancestor.
  PaintLayer* ancestorOverflowPaintLayer;
};

void PrePaintTreeWalk::walk(FrameView& rootFrame) {
  DCHECK(rootFrame.frame().document()->lifecycle().state() ==
         DocumentLifecycle::InPrePaint);

  PrePaintTreeWalkContext initialContext;
  initialContext.treeBuilderContext =
      m_propertyTreeBuilder.setupInitialContext();
  walk(rootFrame, initialContext);
  m_paintInvalidator.processPendingDelayedPaintInvalidations();
}

bool PrePaintTreeWalk::walk(FrameView& frameView,
                            const PrePaintTreeWalkContext& context) {
  if (frameView.shouldThrottleRendering()) {
    // The walk was interrupted by throttled rendering so this subtree was not
    // fully updated.
    return false;
  }

  PrePaintTreeWalkContext localContext(context);

  if (frameView.shouldInvalidateAllPaintAndPaintProperties()) {
    localContext.treeBuilderContext.forceSubtreeUpdate = true;
    localContext.paintInvalidatorContext.forcedSubtreeInvalidationFlags |=
        PaintInvalidatorContext::ForcedWholeTreeFullInvalidation;
    frameView.clearShouldInvalidateAllPaintAndPaintProperties();
  }

  m_propertyTreeBuilder.updateProperties(frameView,
                                         localContext.treeBuilderContext);
  m_paintInvalidator.invalidatePaintIfNeeded(
      frameView, localContext.paintInvalidatorContext);

  LayoutView* view = frameView.layoutView();
  bool descendantsFullyUpdated = view ? walk(*view, localContext) : true;
  if (descendantsFullyUpdated) {
#if DCHECK_IS_ON()
    frameView.layoutView()->assertSubtreeClearedPaintInvalidationFlags();
#endif
    // If descendants were not fully updated, do not clear flags. During the
    // next PrePaintTreeWalk, these flags will be used again.
    frameView.clearNeedsPaintPropertyUpdate();
  }
  return descendantsFullyUpdated;
}

static void updateAuxiliaryObjectProperties(
    const LayoutObject& object,
    PrePaintTreeWalkContext& localContext) {
  PaintLayer* paintLayer = nullptr;

  if (object.isBoxModelObject() && object.hasLayer())
    paintLayer = object.enclosingLayer();

  if (paintLayer) {
    paintLayer->updateAncestorOverflowLayer(
        localContext.ancestorOverflowPaintLayer);
  }

  if (object.styleRef().position() == StickyPosition && paintLayer) {
    paintLayer->layoutObject()->updateStickyPositionConstraints();

    // Sticky position constraints and ancestor overflow scroller affect
    // the sticky layer position, so we need to update it again here.
    // TODO(flackr): This should be refactored in the future to be clearer
    // (i.e. update layer position and ancestor inputs updates in the
    // same walk)
    paintLayer->updateLayerPosition();
  }

  if (object.hasOverflowClip() || (paintLayer && paintLayer->isRootLayer())) {
    DCHECK(paintLayer);
    localContext.ancestorOverflowPaintLayer = paintLayer;
  }
}

bool PrePaintTreeWalk::walk(const LayoutObject& object,
                            const PrePaintTreeWalkContext& context) {
  PrePaintTreeWalkContext localContext(context);

  // TODO(pdr): Ensure multi column works with incremental property tree
  // construction.
  if (object.isLayoutMultiColumnSpannerPlaceholder()) {
    // Walk multi-column spanner as if it replaces the placeholder.
    // Set the flag so that the tree builder can specially handle out-of-flow
    // positioned descendants if their containers are between the multi-column
    // container and the spanner. See PaintPropertyTreeBuilder for details.
    localContext.treeBuilderContext.isUnderMultiColumnSpanner = true;
    const auto& placeholder = toLayoutMultiColumnSpannerPlaceholder(object);
    bool descendantsFullyUpdated =
        walk(*placeholder.layoutObjectInFlowThread(), localContext);
    if (descendantsFullyUpdated) {
      // If descendants were not fully updated, do not clear flags. During the
      // next PrePaintTreeWalk, these flags will be used again.
      object.getMutableForPainting().clearPaintFlags();
    }
    return descendantsFullyUpdated;
  }

  // This must happen before updateContextForBoxPosition, because the
  // latter reads some of the state computed uere.
  updateAuxiliaryObjectProperties(object, localContext);

  // Ensure the current context takes into account the box position. This can
  // change the current context's paint offset so it must precede the paint
  // offset property update check.
  m_propertyTreeBuilder.updateContextForBoxPosition(
      object, localContext.treeBuilderContext);
  // Many paint properties depend on paint offset so we force an update of
  // properties if the paint offset changes.
  if (object.previousPaintOffset() !=
      localContext.treeBuilderContext.current.paintOffset) {
    object.getMutableForPainting().setNeedsPaintPropertyUpdate();
  }

  // Early out from the treewalk if possible.
  if (!object.needsPaintPropertyUpdate() &&
      !object.descendantNeedsPaintPropertyUpdate() &&
      !localContext.treeBuilderContext.forceSubtreeUpdate &&
      !localContext.paintInvalidatorContext.forcedSubtreeInvalidationFlags &&
      !object
           .shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState()) {
    // Even though the subtree was not walked, we know that a walk will not
    // change anything and can return true as if the subtree was fully updated.
    return true;
  }

  m_propertyTreeBuilder.updatePropertiesForSelf(
      object, localContext.treeBuilderContext);
  m_paintInvalidator.invalidatePaintIfNeeded(
      object, localContext.paintInvalidatorContext);
  m_propertyTreeBuilder.updatePropertiesForChildren(
      object, localContext.treeBuilderContext);

  bool descendantsFullyUpdated = true;
  for (const LayoutObject* child = object.slowFirstChild(); child;
       child = child->nextSibling()) {
    // Column spanners are walked through their placeholders. See above.
    if (child->isColumnSpanAll())
      continue;
    bool childFullyUpdated = walk(*child, localContext);
    if (!childFullyUpdated)
      descendantsFullyUpdated = false;
  }

  if (object.isLayoutPart()) {
    const LayoutPart& layoutPart = toLayoutPart(object);
    Widget* widget = layoutPart.widget();
    if (widget && widget->isFrameView()) {
      localContext.treeBuilderContext.current.paintOffset +=
          layoutPart.replacedContentRect().location() -
          widget->frameRect().location();
      localContext.treeBuilderContext.current.paintOffset =
          roundedIntPoint(localContext.treeBuilderContext.current.paintOffset);
      bool frameFullyUpdated = walk(*toFrameView(widget), localContext);
      if (!frameFullyUpdated)
        descendantsFullyUpdated = false;
    }
    // TODO(pdr): Investigate RemoteFrameView (crbug.com/579281).
  }

  if (descendantsFullyUpdated) {
    // If descendants were not updated, do not clear flags. During the next
    // PrePaintTreeWalk, these flags will be used again.
    object.getMutableForPainting().clearPaintFlags();
  }
  return descendantsFullyUpdated;
}

}  // namespace blink
