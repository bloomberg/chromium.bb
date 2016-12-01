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

namespace blink {

struct PrePaintTreeWalkContext {
  PrePaintTreeWalkContext() : paintInvalidatorContext(treeBuilderContext) {}
  PrePaintTreeWalkContext(const PrePaintTreeWalkContext& parentContext)
      : treeBuilderContext(parentContext.treeBuilderContext),
        paintInvalidatorContext(treeBuilderContext,
                                parentContext.paintInvalidatorContext) {}

  PaintPropertyTreeBuilderContext treeBuilderContext;
  PaintInvalidatorContext paintInvalidatorContext;
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

bool PrePaintTreeWalk::walk(const LayoutObject& object,
                            const PrePaintTreeWalkContext& context) {
  // Early out from the treewalk if possible.
  if (!object.needsPaintPropertyUpdate() &&
      !object.descendantNeedsPaintPropertyUpdate() &&
      !context.treeBuilderContext.forceSubtreeUpdate &&
      !context.paintInvalidatorContext.forcedSubtreeInvalidationFlags &&
      !object
           .shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState()) {
    // Even though the subtree was not walked, we know that a walk will not
    // change anything and can return true as if the subtree was fully updated.
    return true;
  }

  PrePaintTreeWalkContext localContext(context);

  // TODO(pdr): These should be removable once paint offset changes mark an
  // object as needing a paint property update. Below, we temporarily re-use
  // paint invalidation flags to detect paint offset changes.
  if (localContext.paintInvalidatorContext.forcedSubtreeInvalidationFlags) {
    // forcedSubtreeInvalidationFlags will be true if locations have changed
    // which will affect paint properties (e.g., PaintOffset).
    localContext.treeBuilderContext.forceSubtreeUpdate = true;
  } else if (object.shouldDoFullPaintInvalidation()) {
    // shouldDoFullPaintInvalidation will be true when locations or overflow
    // changes which will affect paint properties (e.g., PaintOffset, scroll).
    object.getMutableForPainting().setNeedsPaintPropertyUpdate();
  } else if (object.mayNeedPaintInvalidation()) {
    // mayNeedpaintInvalidation will be true when locations change which will
    // affect paint properties (e.g., PaintOffset).
    object.getMutableForPainting().setNeedsPaintPropertyUpdate();
  }

  // TODO(pdr): Ensure multi column works with incremental property tree
  // construction.
  if (object.isLayoutMultiColumnSpannerPlaceholder()) {
    // Walk multi-column spanner as if it replaces the placeholder.
    // Set the flag so that the tree builder can specially handle out-of-flow
    // positioned descendants if their containers are between the multi-column
    // container and the spanner. See PaintPropertyTreeBuilder for details.
    localContext.treeBuilderContext.isUnderMultiColumnSpanner = true;
    bool descendantsFullyUpdated =
        walk(*toLayoutMultiColumnSpannerPlaceholder(object)
                  .layoutObjectInFlowThread(),
             localContext);
    if (descendantsFullyUpdated) {
      // If descendants were not fully updated, do not clear flags. During the
      // next PrePaintTreeWalk, these flags will be used again.
      object.getMutableForPainting().clearPaintInvalidationFlags();
      object.getMutableForPainting().clearNeedsPaintPropertyUpdate();
      object.getMutableForPainting().clearDescendantNeedsPaintPropertyUpdate();
    }
    return descendantsFullyUpdated;
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
    object.getMutableForPainting().clearPaintInvalidationFlags();
    object.getMutableForPainting().clearNeedsPaintPropertyUpdate();
    object.getMutableForPainting().clearDescendantNeedsPaintPropertyUpdate();
  }
  return descendantsFullyUpdated;
}

}  // namespace blink
