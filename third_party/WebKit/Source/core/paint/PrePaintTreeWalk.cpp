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

void PrePaintTreeWalk::walk(FrameView& frameView,
                            const PrePaintTreeWalkContext& context) {
  if (frameView.shouldThrottleRendering())
    return;

  PrePaintTreeWalkContext localContext(context);
  m_propertyTreeBuilder.updatePropertiesAndContext(
      frameView, localContext.treeBuilderContext);

  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled()) {
    m_paintInvalidator.invalidatePaintIfNeeded(
        frameView, localContext.paintInvalidatorContext);
  }

  if (LayoutView* layoutView = frameView.layoutView())
    walk(*layoutView, localContext);

#if DCHECK_IS_ON()
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    frameView.layoutView()->assertSubtreeClearedPaintInvalidationFlags();
#endif
}

void PrePaintTreeWalk::walk(const LayoutObject& object,
                            const PrePaintTreeWalkContext& context) {
  PrePaintTreeWalkContext localContext(context);

  if (object.isLayoutMultiColumnSpannerPlaceholder()) {
    // Walk multi-column spanner as if it replaces the placeholder.
    // Set the flag so that the tree builder can specially handle out-of-flow
    // positioned descendants if their containers are between the multi-column
    // container and the spanner. See PaintPropertyTreeBuilder for details.
    localContext.treeBuilderContext.isUnderMultiColumnSpanner = true;
    object.getMutableForPainting().clearPaintInvalidationFlags();
    walk(*toLayoutMultiColumnSpannerPlaceholder(object)
              .layoutObjectInFlowThread(),
         localContext);
    return;
  }

  m_propertyTreeBuilder.updatePropertiesAndContextForSelf(
      object, localContext.treeBuilderContext);
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    m_paintInvalidator.invalidatePaintIfNeeded(
        object, localContext.paintInvalidatorContext);
  m_propertyTreeBuilder.updatePropertiesAndContextForChildren(
      object, localContext.treeBuilderContext);

  for (const LayoutObject* child = object.slowFirstChild(); child;
       child = child->nextSibling()) {
    // Column spanners are walked through their placeholders. See above.
    if (child->isColumnSpanAll())
      continue;
    walk(*child, localContext);
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
      walk(*toFrameView(widget), localContext);
    }
    // TODO(pdr): Investigate RemoteFrameView (crbug.com/579281).
  }
}

}  // namespace blink
