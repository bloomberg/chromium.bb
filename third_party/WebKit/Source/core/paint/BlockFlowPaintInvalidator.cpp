// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BlockFlowPaintInvalidator.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/paint/BoxPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"

namespace blink {

PaintInvalidationReason BlockFlowPaintInvalidator::invalidatePaintIfNeeded()
{
    PaintInvalidationReason reason = BoxPaintInvalidator(m_blockFlow, m_context).invalidatePaintIfNeeded();

    if (reason == PaintInvalidationNone || reason == PaintInvalidationDelayedFull)
        return reason;

    RootInlineBox* line = m_blockFlow.firstRootBox();
    if (line && line->isFirstLineStyle()) {
        // It's the RootInlineBox that paints the ::first-line background. Note that since it may be
        // expensive to figure out if the first line is affected by any ::first-line selectors at all,
        // we just invalidate it unconditionally which is typically cheaper.
        m_blockFlow.invalidateDisplayItemClient(*line, reason);
    }
    return reason;
}

} // namespace blink
