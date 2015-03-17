// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/ResizeViewportAnchor.h"

#include "core/frame/FrameView.h"
#include "core/frame/PinchViewport.h"
#include "platform/geometry/DoublePoint.h"
#include "platform/geometry/DoubleSize.h"
#include "platform/geometry/FloatSize.h"

namespace blink {

ResizeViewportAnchor::ResizeViewportAnchor(FrameView& rootFrameView, PinchViewport& pinchViewport)
    : ViewportAnchor(rootFrameView, pinchViewport) { }


void ResizeViewportAnchor::setAnchor()
{
    m_pinchViewportInDocument = m_pinchViewport.visibleRectInDocument().location();
}

void ResizeViewportAnchor::restoreToAnchor()
{
    FloatSize delta = m_pinchViewportInDocument - m_pinchViewport.visibleRectInDocument().location();

    DoublePoint targetFrameViewPosition = m_rootFrameView.scrollPositionDouble() + DoubleSize(delta);
    m_rootFrameView.setScrollPosition(targetFrameViewPosition);

    DoubleSize remainder = targetFrameViewPosition - m_rootFrameView.scrollPositionDouble();
    m_pinchViewport.move(toFloatSize(remainder));
}

} // namespace blink
