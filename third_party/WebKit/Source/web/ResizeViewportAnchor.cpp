// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/ResizeViewportAnchor.h"

#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/DoubleSize.h"
#include "platform/geometry/FloatSize.h"

namespace blink {

ResizeViewportAnchor::ResizeViewportAnchor(FrameView& rootFrameView, VisualViewport& visualViewport)
    : ViewportAnchor(rootFrameView, visualViewport)
    , m_visualViewportInDocument(rootFrameView.scrollableArea()->visibleContentRectDouble().location())
{
}

ResizeViewportAnchor::~ResizeViewportAnchor()
{
    m_rootFrameView->scrollableArea()->setScrollPosition(m_visualViewportInDocument, ProgrammaticScroll);
}

} // namespace blink
