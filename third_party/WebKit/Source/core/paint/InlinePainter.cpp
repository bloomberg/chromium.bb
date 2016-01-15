// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/InlinePainter.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/LineBoxListPainter.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/geometry/LayoutPoint.h"
#include <limits>

namespace blink {

void InlinePainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (RuntimeEnabledFeatures::slimmingPaintOffsetCachingEnabled() && !paintInfo.context.paintController().skippingCache()) {
        if (m_layoutInline.paintOffsetChanged(paintOffset)) {
            paintInfo.context.paintController().invalidatePaintOffset(m_layoutInline);
            LineBoxListPainter(*m_layoutInline.lineBoxes()).invalidateLineBoxPaintOffsets(paintInfo);
        }
        // Set previousPaintOffset here in case that m_layoutInline paints nothing and no
        // LayoutObjectDrawingRecorder updates its previousPaintOffset.
        // TODO(wangxianzhu): Integrate paint offset checking into new paint invalidation.
        m_layoutInline.mutableForPainting().setPreviousPaintOffset(paintOffset);
    }

    // FIXME: When Skia supports annotation rect covering (https://code.google.com/p/skia/issues/detail?id=3872),
    // this rect may be covered by foreground and descendant drawings. Then we may need a dedicated paint phase.
    if (paintInfo.phase == PaintPhaseForeground && paintInfo.isPrinting())
        ObjectPainter(m_layoutInline).addPDFURLRectIfNeeded(paintInfo, paintOffset);

    if (shouldPaintSelfOutline(paintInfo.phase) || shouldPaintDescendantOutlines(paintInfo.phase)) {
        ObjectPainter painter(m_layoutInline);
        if (shouldPaintDescendantOutlines(paintInfo.phase))
            painter.paintInlineChildrenOutlines(paintInfo, paintOffset);
        if (shouldPaintSelfOutline(paintInfo.phase) && !m_layoutInline.isElementContinuation())
            painter.paintOutline(paintInfo, paintOffset);
        return;
    }

    LineBoxListPainter(*m_layoutInline.lineBoxes()).paint(m_layoutInline, paintInfo, paintOffset);
}

} // namespace blink
