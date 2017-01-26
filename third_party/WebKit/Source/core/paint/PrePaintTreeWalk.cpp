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

void PrePaintTreeWalk::walk(FrameView& frameView,
                            const PrePaintTreeWalkContext& parentContext) {
  if (frameView.shouldThrottleRendering()) {
    // Skip the throttled frame. Will update it when it becomes unthrottled.
    return;
  }

  PrePaintTreeWalkContext context(parentContext);
  // ancestorOverflowLayer does not cross frame boundaries.
  context.ancestorOverflowPaintLayer = nullptr;
  m_propertyTreeBuilder.updateProperties(frameView, context.treeBuilderContext);
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
  if (!object.hasLayer())
    return;

  PaintLayer* paintLayer = object.enclosingLayer();
  paintLayer->updateAncestorOverflowLayer(context.ancestorOverflowPaintLayer);

  if (object.styleRef().position() == StickyPosition) {
    paintLayer->layoutObject()->updateStickyPositionConstraints();

    // Sticky position constraints and ancestor overflow scroller affect the
    // sticky layer position, so we need to update it again here.
    // TODO(flackr): This should be refactored in the future to be clearer (i.e.
    // update layer position and ancestor inputs updates in the same walk).
    paintLayer->updateLayerPosition();
  }

  if (paintLayer->isRootLayer() || object.hasOverflowClip())
    context.ancestorOverflowPaintLayer = paintLayer;
}

void PrePaintTreeWalk::walk(const LayoutObject& object,
                            const PrePaintTreeWalkContext& parentContext) {
  PrePaintTreeWalkContext context(parentContext);

  // This must happen before updateContextForBoxPosition, because the
  // latter reads some of the state computed uere.
  updateAuxiliaryObjectProperties(object, context);

  // Ensure the current context takes into account the box's position. This can
  // force a subtree update due to paint offset changes and must precede any
  // early out from the treewalk.
  m_propertyTreeBuilder.updateContextForBoxPosition(object,
                                                    context.treeBuilderContext);

  // Early out from the treewalk if possible.
  if (!object.needsPaintPropertyUpdate() &&
      !object.descendantNeedsPaintPropertyUpdate() &&
      !context.treeBuilderContext.forceSubtreeUpdate &&
      !context.paintInvalidatorContext.forcedSubtreeInvalidationFlags &&
      !object
           .shouldCheckForPaintInvalidationRegardlessOfPaintInvalidationState()) {
    return;
  }

  m_propertyTreeBuilder.updatePropertiesForSelf(object,
                                                context.treeBuilderContext);
  m_paintInvalidator.invalidatePaintIfNeeded(object,
                                             context.paintInvalidatorContext);
  m_propertyTreeBuilder.updatePropertiesForChildren(object,
                                                    context.treeBuilderContext);

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
    Widget* widget = layoutPart.widget();
    if (widget && widget->isFrameView()) {
      context.treeBuilderContext.current.paintOffset +=
          layoutPart.replacedContentRect().location() -
          widget->frameRect().location();
      context.treeBuilderContext.current.paintOffset =
          roundedIntPoint(context.treeBuilderContext.current.paintOffset);
      walk(*toFrameView(widget), context);
    }
    // TODO(pdr): Investigate RemoteFrameView (crbug.com/579281).
  }

  object.getMutableForPainting().clearPaintFlags();
}

}  // namespace blink
