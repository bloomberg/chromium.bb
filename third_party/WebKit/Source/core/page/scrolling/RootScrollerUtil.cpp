// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/RootScrollerUtil.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"
#include "core/paint/PaintLayerScrollableArea.h"

namespace blink {

namespace RootScrollerUtil {

ScrollableArea* scrollableAreaFor(const Element& element)
{
    if (!element.layoutObject() || !element.layoutObject()->isBox())
        return nullptr;

    LayoutBox* box = toLayoutBox(element.layoutObject());

    // For a FrameView, we use the layoutViewport rather than the
    // getScrollableArea() since that could be the RootFrameViewport. The
    // rootScroller's ScrollableArea will be swapped in as the layout viewport
    // in RootFrameViewport so we need to ensure we get the layout viewport.
    if (box->isDocumentElement())
        return element.document().view()->layoutViewportScrollableArea();

    return static_cast<PaintInvalidationCapableScrollableArea*>(
        box->getScrollableArea());
}

} // namespace RootScrollerUtil

} // namespace blink
