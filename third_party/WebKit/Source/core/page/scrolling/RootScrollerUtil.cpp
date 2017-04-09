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

ScrollableArea* ScrollableAreaForRootScroller(const Node* node) {
  if (!node)
    return nullptr;

  if (node->IsDocumentNode() || node == node->GetDocument().documentElement()) {
    if (!node->GetDocument().View())
      return nullptr;

    // For a FrameView, we use the layoutViewport rather than the
    // getScrollableArea() since that could be the RootFrameViewport. The
    // rootScroller's ScrollableArea will be swapped in as the layout viewport
    // in RootFrameViewport so we need to ensure we get the layout viewport.
    return node->GetDocument().View()->LayoutViewportScrollableArea();
  }

  DCHECK(node->IsElementNode());
  const Element* element = ToElement(node);

  if (!element->GetLayoutObject() || !element->GetLayoutObject()->IsBox())
    return nullptr;

  return static_cast<PaintInvalidationCapableScrollableArea*>(
      ToLayoutBoxModelObject(element->GetLayoutObject())->GetScrollableArea());
}

PaintLayer* PaintLayerForRootScroller(const Node* node) {
  if (!node)
    return nullptr;

  if (node->IsDocumentNode() || node == node->GetDocument().documentElement()) {
    if (!node->GetDocument().GetLayoutView())
      return nullptr;

    return node->GetDocument().GetLayoutView()->Layer();
  }

  DCHECK(node->IsElementNode());
  const Element* element = ToElement(node);
  if (!element->GetLayoutObject() || !element->GetLayoutObject()->IsBox())
    return nullptr;

  LayoutBox* box = ToLayoutBox(element->GetLayoutObject());
  return box->Layer();
}

}  // namespace RootScrollerUtil

}  // namespace blink
