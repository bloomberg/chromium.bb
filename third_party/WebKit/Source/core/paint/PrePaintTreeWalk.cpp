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
  m_propertyTreeBuilder.buildTreeNodes(frameView,
                                       localContext.treeBuilderContext);

  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    m_paintInvalidator.invalidatePaintIfNeeded(
        frameView, localContext.paintInvalidatorContext);

  if (LayoutView* layoutView = frameView.layoutView())
    walk(*layoutView, localContext);
}

void PrePaintTreeWalk::walk(const LayoutObject& object,
                            const PrePaintTreeWalkContext& context) {
  PrePaintTreeWalkContext localContext(context);

  m_propertyTreeBuilder.buildTreeNodesForSelf(object,
                                              localContext.treeBuilderContext);
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    m_paintInvalidator.invalidatePaintIfNeeded(
        object, localContext.paintInvalidatorContext);
  m_propertyTreeBuilder.buildTreeNodesForChildren(
      object, localContext.treeBuilderContext);

  for (const LayoutObject* child = object.slowFirstChild(); child;
       child = child->nextSibling()) {
    // Column spanners are walked through their placeholders. See below.
    if (child->isColumnSpanAll())
      continue;
    walk(*child, localContext);
  }

  if (object.isLayoutMultiColumnSpannerPlaceholder())
    walk(*toLayoutMultiColumnSpannerPlaceholder(object)
              .layoutObjectInFlowThread(),
         localContext);

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
