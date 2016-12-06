// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/RootScrollerUtil.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayerScrollableArea.h"

namespace blink {

namespace RootScrollerUtil {

ScrollableArea* scrollableAreaForRootScroller(const Node* node) {
  if (!node)
    return nullptr;

  if (node->isDocumentNode() ||
      node->isSameNode(node->document().documentElement())) {
    if (!node->document().view())
      return nullptr;

    // For a FrameView, we use the layoutViewport rather than the
    // getScrollableArea() since that could be the RootFrameViewport. The
    // rootScroller's ScrollableArea will be swapped in as the layout viewport
    // in RootFrameViewport so we need to ensure we get the layout viewport.
    return node->document().view()->layoutViewportScrollableArea();
  }

  DCHECK(node->isElementNode());
  const Element* element = toElement(node);

  if (!element->layoutObject() || !element->layoutObject()->isBox())
    return nullptr;

  return static_cast<PaintInvalidationCapableScrollableArea*>(
      toLayoutBoxModelObject(element->layoutObject())->getScrollableArea());
}

PaintLayer* paintLayerForRootScroller(const Node* node) {
  if (!node)
    return nullptr;

  if (node->isDocumentNode() ||
      node->isSameNode(node->document().documentElement())) {
    if (!node->document().layoutView())
      return nullptr;

    return node->document().layoutView()->layer();
  }

  DCHECK(node->isElementNode());
  const Element* element = toElement(node);
  if (!element->layoutObject() || !element->layoutObject()->isBox())
    return nullptr;

  LayoutBox* box = toLayoutBox(element->layoutObject());
  return box->layer();
}

}  // namespace RootScrollerUtil

}  // namespace blink
